#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "zygisk.hpp"

#define ARR_LEN(a) (sizeof(a) / (sizeof((a)[0])))
#define LOGD(fmt, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, "rvmm-zygisk-mount", "[%d] " fmt, __LINE__, ##__VA_ARGS__)

static bool sendProcInfo(int fd, const char* proc) {
    pid_t pid = getpid();
    if (write(fd, &pid, sizeof(pid)) <= 0) {
        LOGD("ERROR write: %s", strerror(errno));
        return false;
    }

    unsigned int proc_len = strlen(proc) + 1;
    if (write(fd, &proc_len, sizeof(proc_len)) <= 0) {
        LOGD("ERROR write: %s", strerror(errno));
        return false;
    }

    if (write(fd, proc, proc_len) <= 0) {
        LOGD("ERROR write: %s", strerror(errno));
        return false;
    }

    return true;
}

class RVMMZygiskMount : public zygisk::ModuleBase {
   public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs* args) override {
        (void)args;
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* proc = env->GetStringUTFChars(args->nice_name, NULL);

        int fd = api->connectCompanion();
        sendProcInfo(fd, proc);

        close(fd);
        env->ReleaseStringUTFChars(args->nice_name, proc);
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

   private:
    zygisk::Api* api;
    JNIEnv* env;
};

static bool getMountSrcDst(const char* procs_map, const char* proc, const char** src, const char** dst) {
    uint8_t proc_len = strlen(proc);

    size_t i = 0;
    uint8_t tplen;
    while ((tplen = procs_map[i])) {
        const char* tpptr = procs_map + i + sizeof(tplen);

        uint8_t _len = tplen;
        for (size_t j = 0; j < 3; j++) {
            i += sizeof(_len) + _len + 1;
            _len = procs_map[i];
        }

        if (tplen != proc_len) continue;
        if (memcmp(tpptr, proc, tplen) != 0) continue;

        *src = tpptr + tplen + 2 * sizeof(uint8_t);
        uint8_t src_len = *(uint8_t*)(tpptr + tplen + sizeof(uint8_t));
        *dst = *src + src_len + 2 * sizeof(uint8_t);

        return true;
    }
    return false;
}

static char* readFileToNullStr(const char* path) {
    char* buf = nullptr;
    off_t size_read = 0;

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        LOGD("ERROR open: %s", strerror(errno));
        goto defer;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        LOGD("ERROR fstat: %s", strerror(errno));
        goto defer;
    }
    if (st.st_size <= 0) {
        LOGD("ERROR size=%d", (int)st.st_size);
        goto defer;
    }

    buf = (char*)malloc(st.st_size + 1);
    while (size_read < st.st_size) {
        ssize_t ret = read(fd, buf + size_read, st.st_size - size_read);
        if (ret < 0) {
            LOGD("ERROR read: %s", strerror(errno));
            free(buf);
            buf = nullptr;
            goto defer;
        } else {
            size_read += ret;
        }
    }
    buf[st.st_size] = 0;

defer:
    close(fd);
    return buf;
}

static bool injectMount(const char* src, const char* dst, pid_t pid) {
    // int ns_fd = syscall(SYS_pidfd_open, pid, 0);
    // if (ns_fd == -1) {
    //     LOGD("ERROR pidfd_open: %s", strerror(errno));
    //     return false;
    // }

    char ns_path[64];
    snprintf(ns_path, ARR_LEN(ns_path), "/proc/%d/ns/mnt", pid);

    int ns_fd = open(ns_path, O_RDONLY);
    if (ns_fd == -1) {
        LOGD("ERROR open (%s): %s", ns_path, strerror(errno));
        return false;
    }

    int r = setns(ns_fd, CLONE_NEWNS);
    close(ns_fd);
    if (r == -1) {
        LOGD("ERROR setns: %s", strerror(errno));
        return false;
    }

    if (mount(src, dst, NULL, MS_BIND, NULL) != 0) {
        LOGD("ERROR mount: %s", strerror(errno));
        return false;
    }

    return true;
}

static bool receiveProcInfo(int fd, const char** src, const char** dst, pid_t* pid) {
    if (read(fd, pid, sizeof(*pid)) <= 0) {
        LOGD("ERROR read: %s", strerror(errno));
        return false;
    }

    unsigned int proc_len;
    if (read(fd, &proc_len, sizeof(proc_len)) <= 0) {
        LOGD("ERROR read: %s", strerror(errno));
        return false;
    }

    char proc[proc_len];
    if (read(fd, proc, proc_len) <= 0) {
        LOGD("ERROR read: %s", strerror(errno));
        return false;
    }

    char* procs_map = readFileToNullStr("/data/adb/modules/rvmm-zygisk-mount/procs_map");
    if (procs_map == nullptr) return false;

    bool r = getMountSrcDst(procs_map, proc, src, dst);
    free(procs_map);
    if (!r) return false;

    LOGD("%s: %s -> %s", proc, *src, *dst);
    return true;
}

static void companionHandler(int fd) {
    const char *src, *dst;
    pid_t pid;
    if (!receiveProcInfo(fd, &src, &dst, &pid)) return;

    pid_t child = fork();
    if (child == 0) {
        injectMount(src, dst, pid);
        exit(0);
    } else if (child == -1) {
        LOGD("ERROR fork: %s", strerror(errno));
    }
}

REGISTER_ZYGISK_MODULE(RVMMZygiskMount)
REGISTER_ZYGISK_COMPANION(companionHandler)
