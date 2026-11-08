#include <android/log.h>
#include <errno.h>
#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

#include "zygisk.hpp"

#define ARR_LEN(a) (sizeof(a) / (sizeof((a)[0])))
#define LOG_TAG "rvmm-zygisk-mount"
#define LOGD(fmt, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[%d] " fmt, __LINE__, ##__VA_ARGS__)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

static bool sendProcInfo(int fd, const char* proc) {
    pid_t pid = getpid();

    ssize_t w = write(fd, &pid, sizeof(pid));
    if (unlikely(w < (ssize_t)sizeof(pid))) {
        LOGD("ERROR write pid (w=%d): %s", (int)w, strerror(errno));
        return false;
    }

    ssize_t proc_len = strlen(proc) + 1;
    w = write(fd, proc, proc_len);
    if (unlikely(w < proc_len)) {
        LOGD("ERROR write proc (w=%d): %s", (int)w, strerror(errno));
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
        if (!args || !args->nice_name) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        const char* proc = env->GetStringUTFChars(args->nice_name, NULL);
        if (proc != nullptr) {
            int fd = api->connectCompanion();
            if (fd >= 0) {
                sendProcInfo(fd, proc);
                close(fd);
            }
            env->ReleaseStringUTFChars(args->nice_name, proc);
        }
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

   private:
    zygisk::Api* api;
    JNIEnv* env;
};

static bool getMountSrcDst(const char* procs_map, const char* proc, uint8_t proc_len,
                           char* out_src, size_t out_src_max,
                           char* out_dst, size_t out_dst_max) {
    size_t i = 0;
    uint8_t tplen;
    while ((tplen = procs_map[i])) {
        const char* tpptr = procs_map + i + sizeof(tplen);

        bool matches = false;
        if (tplen == proc_len && memcmp(tpptr, proc, tplen) == 0) {
            matches = true;
        } else if (proc_len > tplen && proc[tplen] == ':' && memcmp(tpptr, proc, tplen) == 0) {
            matches = true;
        }

        if (!matches) {
            uint8_t _len = tplen;
            for (size_t j = 0; j < 3; j++) {
                i += sizeof(_len) + _len + 1;
                _len = procs_map[i];
            }
            continue;
        }

        const char* src_ptr = tpptr + tplen + 2 * sizeof(uint8_t);
        uint8_t src_len = *(uint8_t*)(tpptr + tplen + sizeof(uint8_t));
        const char* dst_ptr = src_ptr + src_len + 2 * sizeof(uint8_t);
        uint8_t dst_len = *(uint8_t*)(src_ptr + src_len + sizeof(uint8_t));

        if (src_len >= out_src_max || dst_len >= out_dst_max) {
            LOGD("Path length exceeds buffer (src_len=%d, dst_len=%d)", (int)src_len, (int)dst_len);
            return false;
        }

        memcpy(out_src, src_ptr, src_len);
        out_src[src_len] = '\0';

        memcpy(out_dst, dst_ptr, dst_len);
        out_dst[dst_len] = '\0';

        return true;
    }
    return false;
}

static char* readFileToNullStr(const char* path) {
    char* buf = nullptr;
    off_t size_read = 0;

    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (unlikely(fd == -1)) {
        LOGD("ERROR open: %s", strerror(errno));
        return nullptr;
    }

    struct stat st;
    if (unlikely(fstat(fd, &st) == -1)) {
        LOGD("ERROR fstat: %s", strerror(errno));
        close(fd);
        return nullptr;
    }
    if (unlikely(st.st_size <= 0)) {
        close(fd);
        return nullptr;
    }

    buf = (char*)malloc(st.st_size + 1);
    if (!buf) {
        close(fd);
        return nullptr;
    }

    while (size_read < st.st_size) {
        ssize_t ret = read(fd, buf + size_read, st.st_size - size_read);
        if (unlikely(ret < 0)) {
            LOGD("ERROR read: %s", strerror(errno));
            free(buf);
            close(fd);
            return nullptr;
        } else if (ret == 0) {
            break;
        } else {
            size_read += ret;
        }
    }
    buf[size_read] = '\0';
    close(fd);
    return buf;
}

static bool injectMount(const char* src, const char* dst, pid_t pid) {
    char ns_path[64];
    snprintf(ns_path, ARR_LEN(ns_path), "/proc/%d/ns/mnt", pid);

    int ns_fd = open(ns_path, O_RDONLY | O_CLOEXEC);
    if (unlikely(ns_fd == -1)) {
        LOGD("ERROR open (%s): %s", ns_path, strerror(errno));
        return false;
    }

    int ns = setns(ns_fd, CLONE_NEWNS);
    close(ns_fd);
    if (unlikely(ns == -1)) {
        LOGD("ERROR setns for pid %d: %s", pid, strerror(errno));
        return false;
    }

    if (unlikely(mount(src, dst, NULL, MS_BIND, NULL) != 0)) {
        LOGD("ERROR mount (%s -> %s for pid %d): %s", src, dst, pid, strerror(errno));
        return false;
    }

    LOGD("SUCCESS mount (%s -> %s for pid %d)", src, dst, pid);
    return true;
}

static bool receiveProcInfo(int fd, char* out_src, size_t out_src_max,
                           char* out_dst, size_t out_dst_max, pid_t* pid) {
    ssize_t r = read(fd, pid, sizeof(*pid));
    if (unlikely(r < (ssize_t)sizeof(*pid))) {
        LOGD("ERROR read pid (r=%d): %s", (int)r, strerror(errno));
        return false;
    }

    uint8_t proc_len = 0;
    char proc[UINT8_MAX];
    for (;;) {
        r = read(fd, proc + proc_len, sizeof(proc) - proc_len);
        if (r == 0) {
            break;
        } else if (unlikely(r < 0)) {
            LOGD("ERROR read proc (r=%d): %s", (int)r, strerror(errno));
            return false;
        } else {
            proc_len += r;
        }
    }

    if (proc_len == 0) return false;

    char* procs_map = readFileToNullStr("/data/adb/modules/rvmm-zygisk-mount/procs_map");
    if (procs_map == nullptr) {
        LOGD("procs_map not found or empty");
        return false;
    }

    bool s = getMountSrcDst(procs_map, proc, proc_len - 1, out_src, out_src_max, out_dst, out_dst_max);
    free(procs_map);
    if (!s) return false;

    LOGD("Matched: %s: %s -> %s (pid %d)", proc, out_src, out_dst, *pid);
    return true;
}

static void companionHandler(int fd) {
    char src[512] = {0};
    char dst[512] = {0};
    pid_t pid = 0;

    if (!receiveProcInfo(fd, src, sizeof(src), dst, sizeof(dst), &pid)) {
        return;
    }

    pid_t child = fork();
    if (child == 0) {
        injectMount(src, dst, pid);
        _exit(0);
    } else if (child == -1) {
        LOGD("ERROR fork: %s", strerror(errno));
    } else {
        int status;
        waitpid(child, &status, 0);
    }
}

REGISTER_ZYGISK_MODULE(RVMMZygiskMount)
REGISTER_ZYGISK_COMPANION(companionHandler)
