// argon-darwin.h: the Darwin libc and objc4 calls PureDarwin's frameworks use, for Argon on Linux.
// force-included into every framework source, implemented in darwin-compat.c
#pragma once
#ifndef __ASSEMBLER__
#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/vfs.h>
#include <uuid/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef OBJC_EXPORT
#define OBJC_EXPORT extern
#endif

// CoreGraphics' event taps hand back a CFMachPort, which Linux CF does not build
typedef struct __CFMachPort *CFMachPortRef;

#ifdef ARGON_DARWIN_STATFS
// Darwin's statfs for app code: Linux's fields plus the type and mount names Darwin carries
#include <sys/statfs.h>
#include <mntent.h>
#include <stdio.h>
#include <string.h>
struct argon_statfs {
    unsigned long f_bsize, f_blocks, f_bfree, f_bavail, f_files, f_ffree;
    fsid_t f_fsid;
    unsigned long f_flags;
    char f_fstypename[16], f_mntonname[1024], f_mntfromname[1024];
};
static inline int argon_statfs(const char *path, struct argon_statfs *out)
{
    struct statfs fs;
    FILE *table;
    struct mntent *m;
    size_t best = 0;

    if (statfs(path, &fs) != 0)
        return -1;
    memset(out, 0, sizeof(*out));
    out->f_bsize = fs.f_bsize; out->f_blocks = fs.f_blocks; out->f_bfree = fs.f_bfree;
    out->f_bavail = fs.f_bavail; out->f_files = fs.f_files; out->f_ffree = fs.f_ffree;
    out->f_fsid = fs.f_fsid; out->f_flags = fs.f_flags;
    // the longest mount point the path sits under names its file system
    if ((table = setmntent("/proc/self/mounts", "r")) != NULL) {
        while ((m = getmntent(table)) != NULL) {
            size_t n = strlen(m->mnt_dir);
            if (strncmp(path, m->mnt_dir, n) == 0 && (path[n] == '/' || path[n] == 0 || n == 1) && n >= best) {
                best = n;
                snprintf(out->f_fstypename, sizeof(out->f_fstypename), "%s", m->mnt_type);
                snprintf(out->f_mntonname, sizeof(out->f_mntonname), "%s", m->mnt_dir);
                snprintf(out->f_mntfromname, sizeof(out->f_mntfromname), "%s", m->mnt_fsname);
            }
        }
        endmntent(table);
    }
    return 0;
}
#define statfs argon_statfs
#endif

// Darwin's last-login record, Linux keeps none outside wtmp so callers take their utmpx fallback
#include <utmpx.h>
struct lastlogx { __typeof__(((struct utmpx *)0)->ut_tv) ll_tv; char ll_line[32]; char ll_host[256]; };
static inline struct lastlogx *getlastlogxbyname(const char *user, struct lastlogx *out) { (void)user; (void)out; return NULL; }

// MacTypes' success result
#ifndef noErr
#define noErr 0
#endif

// uuid_unparse's output buffer, as Darwin's uuid.h names it
typedef char uuid_string_t[37];

// whether this is the thread main() ran on
int pthread_main_np(void);

// the executable's name, set from argv[0] at load
const char *getprogname(void);
void setprogname(const char *name);

#if defined(__OBJC__) || defined(__OBJC2__)
#include <objc/runtime.h>
#include <objc/objc-arc.h>
// objc4 weak references keep the object itself in the location, as PureDarwin's code reads them
id argon_storeWeak(id *location, id obj);
id argon_loadWeakRetained(id *location);
id argon_loadWeak(id *location);
#define objc_storeWeak(location, obj) argon_storeWeak((location), (obj))
#define objc_storeWeakOrNil(location, obj) argon_storeWeak((location), (obj))
#define objc_loadWeakRetained(location) argon_loadWeakRetained(location)
#define objc_loadWeak(location) argon_loadWeak(location)
// the path of the shared object that defines a class
const char *class_getImageName(Class cls);
#endif

#ifdef __cplusplus
}
#endif
#endif
