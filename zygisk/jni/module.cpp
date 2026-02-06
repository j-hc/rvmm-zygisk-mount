#include <android/log.h>
#include <asm-generic/fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <unistd.h>

#include "zygisk.hpp"

#define ARR_LEN(a) (sizeof(a) / (sizeof((a)[0])))
#define LOGD(fmt, ...) \
    __android_log_print(ANDROID_LOG_DEBUG, "rvmm-zygisk-mount", "[%d] " fmt, __LINE__, ##__VA_ARGS__)

static char* PROCS_MAP = NULL;

class RVMMZygiskMount : public zygisk::ModuleBase {
   public:
    void onLoad(zygisk::Api* api, JNIEnv* env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs* args) override {
        const char* proc = env->GetStringUTFChars(args->nice_name, NULL);
        run(proc);
        env->ReleaseStringUTFChars(args->nice_name, proc);
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

   private:
    zygisk::Api* api;
    JNIEnv* env;

    void run(const char* proc) {
        int fd = api->connectCompanion();
        size_t comp_len = this->read_companion(fd);
        close(fd);
        if (comp_len == 0) return;

        const char *src, *dst;
        if (!getMountSrcDst(proc, &src, &dst)) return;

        LOGD("%s: %s -> %s", proc, src, dst);

        if (unshare(CLONE_NEWNS) != 0) {
            LOGD("ERROR unshare: %s", strerror(errno));
            return;
        }

        if (mount(src, dst, NULL, MS_BIND, NULL) != 0) {
            LOGD("ERROR mount: %s", strerror(errno));
            return;
        }
    }

    bool getMountSrcDst(const char* proc, const char** src, const char** dst) {
        uint8_t proc_len = strlen(proc);

        bool found = false;
        size_t i = 0;
        uint8_t tplen;
        while ((tplen = PROCS_MAP[i])) {
            const char* tpptr = PROCS_MAP + i + sizeof(tplen);

            uint8_t _len = tplen;
            for (size_t j = 0; j < 3; j++) {
                i += sizeof(_len) + _len + 1;
                _len = PROCS_MAP[i];
            }

            if (tplen != proc_len) continue;
            if (memcmp(tpptr, proc, tplen) != 0) continue;

            *src = tpptr + tplen + 2 * sizeof(uint8_t);
            uint8_t src_len = *(uint8_t*)(tpptr + tplen + sizeof(uint8_t));
            *dst = *src + src_len + 2 * sizeof(uint8_t);
            found = true;
            break;
        }
        return found;
    }

    size_t read_companion(int fd) {
        off_t size;
        if (read(fd, &size, sizeof(size)) < 0) {
            LOGD("ERROR read: %s", strerror(errno));
            return 0;
        }

        if (size <= 0) {
            LOGD("ERROR read_companion: size=%zu", size);
            return 0;
        }

        PROCS_MAP = (char*)malloc(size + 1);
        off_t size_read = 0;
        while (size_read < size) {
            ssize_t ret = read(fd, PROCS_MAP, size - size_read);
            if (ret < 0) {
                LOGD("ERROR read: %s", strerror(errno));
                return 0;
            } else {
                size_read += ret;
            }
        }
        PROCS_MAP[size] = 0;
        return (size_t)size;
    }
};

static void companion_handler(int remote_fd) {
    off_t size = 0;
    int fd = open("/data/adb/modules/rvmm-zygisk-mount/procs_map", O_RDONLY);
    if (fd == -1) {
        LOGD("ERROR open: %s", strerror(errno));
        goto bail;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        LOGD("ERROR fstat: %s", strerror(errno));
        goto bail;
    }
    size = st.st_size;

bail:
    if (write(remote_fd, &size, sizeof(size)) < 0) {
        LOGD("ERROR write: %s", strerror(errno));
        size = 0;
    }
    if (fd > 0) {
        if (size > 0 && sendfile(remote_fd, fd, NULL, size) < 0) {
            LOGD("ERROR sendfile: %s", strerror(errno));
        }
        close(fd);
    }
}

REGISTER_ZYGISK_MODULE(RVMMZygiskMount)
REGISTER_ZYGISK_COMPANION(companion_handler)
