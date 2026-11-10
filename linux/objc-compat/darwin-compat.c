// darwin-compat.c - the Darwin libc calls Argon's frameworks expect, on Linux
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _GNU_SOURCE
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>

extern char **environ;

static int argon_argc;
static char **argon_argv;
static char *argon_progname;

// glibc hands init_array constructors the process arguments
static void argon_capture_args(int argc, char **argv, char **envp)
{
    (void)envp;
    argon_argc = argc;
    argon_argv = argv;
    if (argc > 0 && argv[0] != NULL) {
        char *slash = strrchr(argv[0], '/');
        argon_progname = slash ? slash + 1 : argv[0];
    }
}

__attribute__((section(".init_array"), used))
static void (*argon_capture_args_entry)(int, char **, char **) = argon_capture_args;

int *_NSGetArgc(void) { return &argon_argc; }
char ***_NSGetArgv(void) { return &argon_argv; }
char ***_NSGetEnviron(void) { return &environ; }
char **_NSGetProgname(void) { return &argon_progname; }

const char *getprogname(void) { return argon_progname ? argon_progname : "argon"; }
void setprogname(const char *name) { argon_progname = (char *)name; }

int pthread_main_np(void)
{
    return syscall(SYS_gettid) == getpid();
}

// libobjc2 records no image per class, the loader knows which object holds its storage
const char *class_getImageName(void *cls)
{
    Dl_info info;

    if (cls != NULL && dladdr(cls, &info) && info.dli_fname != NULL)
        return info.dli_fname;
    return NULL;
}

// Mach stand-ins: ports get names so code that only allocates one can start, sends find no receiver
#include <time.h>
#include <stdatomic.h>
typedef unsigned int natural_t;
typedef natural_t mach_port_t;
static _Atomic natural_t argon_next_port = 0x1000;
mach_port_t bootstrap_port = 0;

mach_port_t mach_task_self(void) { return 1; }

int mach_port_allocate(mach_port_t task, natural_t right, mach_port_t *name)
{
    (void)task;
    (void)right;
    *name = atomic_fetch_add(&argon_next_port, 1);
    return 0;
}

int mach_port_insert_right(mach_port_t task, mach_port_t name, mach_port_t poly, natural_t type)
{
    (void)task; (void)name; (void)poly; (void)type;
    return 0;
}

int mach_port_deallocate(mach_port_t task, mach_port_t name) { (void)task; (void)name; return 0; }
int mach_port_mod_refs(mach_port_t task, mach_port_t name, natural_t right, int delta) { (void)task; (void)name; (void)right; (void)delta; return 0; }

int bootstrap_look_up(mach_port_t bp, const char *service, mach_port_t *sp) { (void)bp; (void)service; *sp = 0; return 1102; }
int bootstrap_check_in(mach_port_t bp, const char *service, mach_port_t *sp) { (void)bp; (void)service; *sp = 0; return 1102; }

// a send has no destination, a receive waits out its timeout or forever
natural_t mach_msg(void *msg, natural_t option, natural_t send_size, natural_t rcv_size, mach_port_t rcv_name,
                   natural_t timeout, mach_port_t notify)
{
    (void)msg; (void)send_size; (void)rcv_size; (void)rcv_name; (void)notify;
    if (option & 0x1)
        return 0x10000003;
    if (option & 0x2) {
        if (option & 0x100) {
            struct timespec wait = { timeout / 1000, (long)(timeout % 1000) * 1000000L };
            nanosleep(&wait, NULL);
            return 0x10004003;
        }
        for (;;)
            pause();
    }
    return 0;
}

// BSD's unmount(2), Linux spells it umount2
#include <sys/mount.h>
int unmount(const char *dir, int flags)
{
    (void)flags;
    return umount2(dir, 0);
}

// sysctl names PureDarwin code reads: hardware sizes, boot time and the process table
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <stdint.h>
#include "include/sys/sysctl.h"

static int sysctl_copy(void *oldp, size_t *oldlenp, const void *value, size_t size)
{
    if (oldlenp == NULL)
        return errno = EINVAL, -1;
    if (oldp != NULL) {
        if (*oldlenp < size)
            return errno = ENOMEM, -1;
        memcpy(oldp, value, size);
    }
    *oldlenp = size;
    return 0;
}

static int sysctl_string(void *oldp, size_t *oldlenp, const char *text)
{
    return sysctl_copy(oldp, oldlenp, text, strlen(text) + 1);
}

static int boot_time(void *oldp, size_t *oldlenp)
{
    struct sysinfo info;
    struct timeval boot;

    if (sysinfo(&info) != 0)
        return -1;
    gettimeofday(&boot, NULL);
    boot.tv_sec -= info.uptime;
    boot.tv_usec = 0;
    return sysctl_copy(oldp, oldlenp, &boot, sizeof(boot));
}

// the first line of a /proc/cpuinfo field, for the CPU brand
static int cpuinfo(const char *field, char *out, size_t size)
{
    FILE *f = fopen("/proc/cpuinfo", "r");
    char line[512];
    int found = 0;

    while (f && !found && fgets(line, sizeof(line), f)) {
        char *colon = strchr(line, ':');
        if (colon && strncmp(line, field, strlen(field)) == 0) {
            snprintf(out, size, "%s", colon + 2);
            out[strcspn(out, "\n")] = 0;
            found = 1;
        }
    }
    if (f)
        fclose(f);
    return found;
}

// distinct physical id and core id pairs in /proc/cpuinfo, the online count when it lists neither
static int physical_cores(void)
{
    char line[256], seen[256][24];
    int n = 0, package = 0;
    FILE *f = fopen("/proc/cpuinfo", "r");

    while (f && fgets(line, sizeof(line), f)) {
        char key[24];
        int core, dup = 0;

        if (sscanf(line, "physical id : %d", &package) == 1)
            continue;
        if (sscanf(line, "core id : %d", &core) != 1)
            continue;
        snprintf(key, sizeof(key), "%d:%d", package, core);
        for (int i = 0; i < n; i++)
            dup |= strcmp(seen[i], key) == 0;
        if (!dup && n < 256)
            snprintf(seen[n++], sizeof(seen[0]), "%s", key);
    }
    if (f)
        fclose(f);
    return n > 0 ? n : (int)sysconf(_SC_NPROCESSORS_ONLN);
}

int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{
    (void)newp;
    (void)newlen;
    if (strcmp(name, "hw.ncpu") == 0 || strcmp(name, "hw.logicalcpu") == 0 || strcmp(name, "hw.activecpu") == 0) {
        int n = (int)sysconf(_SC_NPROCESSORS_ONLN);
        return sysctl_copy(oldp, oldlenp, &n, sizeof(n));
    }
    if (strcmp(name, "hw.physicalcpu") == 0 || strcmp(name, "hw.physicalcpu_max") == 0) {
        int n = physical_cores();
        return sysctl_copy(oldp, oldlenp, &n, sizeof(n));
    }
    if (strcmp(name, "hw.memsize") == 0 || strcmp(name, "hw.physmem") == 0) {
        uint64_t bytes = (uint64_t)sysconf(_SC_PHYS_PAGES) * (uint64_t)sysconf(_SC_PAGESIZE);
        return sysctl_copy(oldp, oldlenp, &bytes, sizeof(bytes));
    }
    if (strcmp(name, "hw.pagesize") == 0) {
        int n = (int)sysconf(_SC_PAGESIZE);
        return sysctl_copy(oldp, oldlenp, &n, sizeof(n));
    }
    if (strcmp(name, "kern.boottime") == 0)
        return boot_time(oldp, oldlenp);
    if (strcmp(name, "machdep.cpu.brand_string") == 0) {
        char brand[256];
        return cpuinfo("model name", brand, sizeof(brand)) ? sysctl_string(oldp, oldlenp, brand) : (errno = ENOENT, -1);
    }
    if (strcmp(name, "hw.machine") == 0)
        return sysctl_string(oldp, oldlenp, "x86_64");
    if (strcmp(name, "hw.model") == 0) {
        char model[256] = "PC";
        FILE *f = fopen("/sys/class/dmi/id/product_name", "r");
        if (f) {
            if (fgets(model, sizeof(model), f))
                model[strcspn(model, "\n")] = 0;
            fclose(f);
        }
        return sysctl_string(oldp, oldlenp, model);
    }
    if (strcmp(name, "kern.ostype") == 0)
        return sysctl_string(oldp, oldlenp, "Linux");
    if (strcmp(name, "kern.hostname") == 0) {
        char host[256];
        gethostname(host, sizeof(host));
        return sysctl_string(oldp, oldlenp, host);
    }
    errno = ENOENT;
    return -1;
}

// every process from /proc/<pid>/stat as Darwin's kinfo_proc
static int process_table(void *oldp, size_t *oldlenp)
{
    DIR *proc = opendir("/proc");
    struct dirent *entry;
    size_t count = 0, capacity = oldp ? *oldlenp / sizeof(struct kinfo_proc) : 0;

    if (proc == NULL)
        return -1;
    while ((entry = readdir(proc)) != NULL) {
        char path[64], line[512];
        FILE *f;

        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
        if (oldp == NULL) {
            count++;
            continue;
        }
        if (count >= capacity)
            break;
        snprintf(path, sizeof(path), "/proc/%s/stat", entry->d_name);
        if ((f = fopen(path, "r")) == NULL)
            continue;
        if (fgets(line, sizeof(line), f)) {
            char *open = strchr(line, '('), *close = strrchr(line, ')');
            struct kinfo_proc *k = (struct kinfo_proc *)oldp + count;

            if (open && close && close > open) {
                memset(k, 0, sizeof(*k));
                k->kp_proc.p_pid = atoi(entry->d_name);
                snprintf(k->kp_proc.p_comm, sizeof(k->kp_proc.p_comm), "%.*s", (int)(close - open - 1), open + 1);
                k->kp_proc.p_stat = close[2] == 'Z' ? SZOMB : close[2] == 'T' ? SSTOP : close[2] == 'R' ? SRUN : SSLEEP;
                count++;
            }
        }
        fclose(f);
    }
    closedir(proc);
    *oldlenp = count * sizeof(struct kinfo_proc);
    return 0;
}

int sysctl(const int *name, unsigned int namelen, void *oldp, size_t *oldlenp, const void *newp, size_t newlen)
{
    (void)newp;
    (void)newlen;
    if (namelen >= 2 && name[0] == CTL_KERN && name[1] == KERN_PROC)
        return process_table(oldp, oldlenp);
    if (namelen >= 2 && name[0] == CTL_KERN && name[1] == KERN_BOOTTIME)
        return boot_time(oldp, oldlenp);
    errno = ENOENT;
    return -1;
}

// Mach's VM counters from /proc/meminfo, in pages as host_statistics64 reports them
mach_port_t mach_host_self(void) { return 2; }

int host_statistics64(mach_port_t host, int flavor, int *info, unsigned int *count)
{
    struct { uint64_t free_count, active_count, inactive_count, wire_count, compressor_page_count, speculative_count; } *vm = (void *)info;
    FILE *f = fopen("/proc/meminfo", "r");
    char key[64];
    unsigned long long kb, page = (unsigned long long)sysconf(_SC_PAGESIZE) / 1024;

    (void)host;
    if (flavor != 4 || f == NULL || *count * sizeof(int) < sizeof(*vm)) {
        if (f)
            fclose(f);
        return 4;
    }
    memset(vm, 0, sizeof(*vm));
    while (fscanf(f, "%63[^:]: %llu kB\n", key, &kb) == 2) {
        uint64_t pages = kb / page;
        if (strcmp(key, "MemFree") == 0) vm->free_count = pages;
        else if (strcmp(key, "Active") == 0) vm->active_count = pages;
        else if (strcmp(key, "Inactive") == 0) vm->inactive_count = pages;
        // what the kernel holds and cannot page out, Darwin's wired memory
        else if (strcmp(key, "Unevictable") == 0 || strcmp(key, "KernelStack") == 0 || strcmp(key, "PageTables") == 0 || strcmp(key, "SUnreclaim") == 0) vm->wire_count += pages;
        else if (strcmp(key, "Zswap") == 0) vm->compressor_page_count = pages;
    }
    fclose(f);
    *count = sizeof(*vm) / sizeof(int);
    return 0;
}
