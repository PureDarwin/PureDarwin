// procfs.c - the /proc and /sys files Linux programs read, made from what Darwin reports
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _DARWIN_C_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libproc.h>
#include <limits.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "elftrans.h"

extern char **environ;

char guest_exe[PATH_MAX];
const char *const *guest_argv;
int guest_argc;

typedef struct {
    char *p;
    size_t n, cap;
} Buf;

__attribute__((format(printf, 2, 3))) static void
put(Buf *b, const char *fmt, ...)
{
    va_list ap;
    int n;

    for (;;) {
        va_start(ap, fmt);
        n = vsnprintf(b->p ? b->p + b->n : NULL, b->p ? b->cap - b->n : 0, fmt, ap);
        va_end(ap);
        if (n < 0)
            return;
        if (b->p && b->n + (size_t)n < b->cap)
            break;
        b->cap = (b->cap + (size_t)n + 1) * 2;
        if (!(b->p = realloc(b->p, b->cap)))
            _exit(126);
    }
    b->n += (size_t)n;
}

static int
ncpu(void)
{
    int n = 1;
    size_t len = sizeof(n);

    sysctlbyname("hw.logicalcpu", &n, &len, NULL, 0);
    return n > 0 ? n : 1;
}

static void
sysctl_string(const char *name, char *out, size_t size)
{
    size_t len = size;

    if (sysctlbyname(name, out, &len, NULL, 0))
        out[0] = 0;
}

static long
sysctl_long(const char *name)
{
    long v = 0;
    size_t len = sizeof(v);

    sysctlbyname(name, &v, &len, NULL, 0);
    return v;
}

// Darwin's feature names upper case, Linux's lower case, with a few spelled differently
static void
cpu_flags(Buf *b)
{
    static const char *const names[] = { "machdep.cpu.features", "machdep.cpu.leaf7_features",
                                         "machdep.cpu.extfeatures" };
    char s[2048];

    for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        char *tok, *save;

        sysctl_string(names[i], s, sizeof(s));
        for (tok = strtok_r(s, " ", &save); tok; tok = strtok_r(NULL, " ", &save)) {
            char *c;

            for (c = tok; *c; c++) {
                if (*c >= 'A' && *c <= 'Z')
                    *c = (char)(*c - 'A' + 'a');
                else if (*c == '.')
                    *c = '_';
            }
            if (!strcmp(tok, "sse4_1") || !strcmp(tok, "sse4_2") || !strcmp(tok, "avx1_0"))
                put(b, " %s", strcmp(tok, "avx1_0") ? tok : "avx");
            else if (!strcmp(tok, "sse3"))
                put(b, " pni");
            else
                put(b, " %s", tok);
        }
    }
}

static void
gen_cpuinfo(Buf *b)
{
    char brand[128], vendor[32];
    int n = ncpu(), cores = (int)sysctl_long("machdep.cpu.core_count");
    long mhz = sysctl_long("hw.cpufrequency") / 1000000;

    sysctl_string("machdep.cpu.brand_string", brand, sizeof(brand));
    sysctl_string("machdep.cpu.vendor", vendor, sizeof(vendor));
    if (cores <= 0)
        cores = n;
    for (int i = 0; i < n; i++) {
        put(b, "processor\t: %d\nvendor_id\t: %s\ncpu family\t: %ld\nmodel\t\t: %ld\nmodel name\t: %s\n", i, vendor,
            sysctl_long("machdep.cpu.family"), sysctl_long("machdep.cpu.model"), brand);
        put(b, "stepping\t: %ld\ncpu MHz\t\t: %ld.000\ncache size\t: %ld KB\nphysical id\t: 0\nsiblings\t: %d\n",
            sysctl_long("machdep.cpu.stepping"), mhz ? mhz : 1000, sysctl_long("hw.l2cachesize") / 1024, n);
        put(b, "core id\t\t: %d\ncpu cores\t: %d\napicid\t\t: %d\nfpu\t\t: yes\nfpu_exception\t: yes\n", i % cores,
            cores, i);
        put(b, "cpuid level\t: 13\nwp\t\t: yes\nflags\t\t:");
        cpu_flags(b);
        put(b, "\nbogomips\t: %ld.00\nclflush size\t: 64\ncache_alignment\t: 64\n", (mhz ? mhz : 1000) * 2);
        put(b, "address sizes\t: 48 bits physical, 48 bits virtual\npower management:\n\n");
    }
}

static void
gen_meminfo(Buf *b)
{
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    uint64_t total = (uint64_t)sysctl_long("hw.memsize"), page = (uint64_t)getpagesize(), free_kb = 0, avail_kb = 0,
             cached_kb = 0;

    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS) {
        free_kb = (uint64_t)vm.free_count * page / 1024;
        cached_kb = (uint64_t)vm.external_page_count * page / 1024;
        avail_kb = (uint64_t)(vm.free_count + vm.inactive_count + vm.purgeable_count) * page / 1024;
    }
    put(b, "MemTotal:       %8llu kB\nMemFree:        %8llu kB\nMemAvailable:   %8llu kB\n",
        (unsigned long long)(total / 1024), (unsigned long long)free_kb, (unsigned long long)avail_kb);
    put(b, "Buffers:               0 kB\nCached:         %8llu kB\nSwapCached:            0 kB\n",
        (unsigned long long)cached_kb);
    put(b, "SwapTotal:             0 kB\nSwapFree:              0 kB\nShmem:                 0 kB\n");
    put(b, "CommitLimit:    %8llu kB\nVmallocTotal:   34359738367 kB\nHugePages_Total:       0\n"
        "Hugepagesize:       2048 kB\n", (unsigned long long)(total / 1024));
}

static void
gen_stat(Buf *b)
{
    processor_info_array_t info;
    mach_msg_type_number_t count;
    natural_t n = 0;
    struct timeval boot;
    size_t len = sizeof(boot);
    unsigned long long all[4] = { 0 };

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO, &n, &info, &count) != KERN_SUCCESS)
        n = 0;
    for (natural_t i = 0; i < n; i++) {
        for (int s = 0; s < 4; s++)
            all[s] += (unsigned)info[i * CPU_STATE_MAX + s];
    }
    // Darwin's states are user, system, idle and nice, Linux's lines are user nice system idle
    put(b, "cpu  %llu %llu %llu %llu 0 0 0 0 0 0\n", all[CPU_STATE_USER], all[CPU_STATE_NICE], all[CPU_STATE_SYSTEM],
        all[CPU_STATE_IDLE]);
    for (natural_t i = 0; i < n; i++) {
        integer_t *c = &info[i * CPU_STATE_MAX];

        put(b, "cpu%u %u %u %u %u 0 0 0 0 0 0\n", i, c[CPU_STATE_USER], c[CPU_STATE_NICE], c[CPU_STATE_SYSTEM],
            c[CPU_STATE_IDLE]);
    }
    if (n)
        vm_deallocate(mach_task_self(), (vm_address_t)info, count * sizeof(*info));
    if (sysctlbyname("kern.boottime", &boot, &len, NULL, 0))
        boot.tv_sec = 0;
    put(b, "intr 0\nctxt 0\nbtime %ld\nprocesses 0\nprocs_running 1\nprocs_blocked 0\n", (long)boot.tv_sec);
}

static double
uptime(void)
{
    struct timeval boot, now;
    size_t len = sizeof(boot);

    if (sysctlbyname("kern.boottime", &boot, &len, NULL, 0))
        return 0;
    gettimeofday(&now, NULL);
    return (double)(now.tv_sec - boot.tv_sec) + (now.tv_usec - boot.tv_usec) / 1e6;
}

static void
gen_maps(Buf *b)
{
    mach_vm_address_t addr = 0;
    mach_vm_size_t size;
    natural_t depth = 0;

    for (;;) {
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        char path[PATH_MAX];

        if (mach_vm_region_recurse(mach_task_self(), &addr, &size, &depth, (vm_region_recurse_info_t)&info, &count) !=
            KERN_SUCCESS)
            break;
        if (info.is_submap) {
            depth++;
            continue;
        }
        // the mapped file, through proc_pidinfo: libSystem has no proc_regionfilename export
        struct proc_regionwithpathinfo rp;

        path[0] = 0;
        if (proc_pidinfo(getpid(), PROC_PIDREGIONPATHINFO, addr, &rp, sizeof(rp)) == (int)sizeof(rp) &&
            rp.prp_prinfo.pri_address <= addr)
            snprintf(path, sizeof(path), "%s", rp.prp_vip.vip_path);
        put(b, "%08llx-%08llx %c%c%c%c %08llx 00:00 0", (unsigned long long)addr, (unsigned long long)(addr + size),
            info.protection & VM_PROT_READ ? 'r' : '-', info.protection & VM_PROT_WRITE ? 'w' : '-',
            info.protection & VM_PROT_EXECUTE ? 'x' : '-', info.share_mode == SM_SHARED ? 's' : 'p',
            (unsigned long long)info.offset);
        put(b, path[0] ? "                          %s\n" : "\n", path);
        addr += size;
    }
}

static const char *
comm(void)
{
    const char *name = guest_argc ? guest_argv[0] : guest_exe, *slash = strrchr(name, '/');

    return slash ? slash + 1 : name;
}

static int
threads(void)
{
    thread_act_array_t list;
    mach_msg_type_number_t n = 0;

    if (task_threads(mach_task_self(), &list, &n) != KERN_SUCCESS)
        return 1;
    for (mach_msg_type_number_t i = 0; i < n; i++)
        mach_port_deallocate(mach_task_self(), list[i]);
    vm_deallocate(mach_task_self(), (vm_address_t)list, n * sizeof(*list));
    return (int)n;
}

static void
rss(uint64_t *virt, uint64_t *res)
{
    mach_task_basic_info_data_t info;
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;

    *virt = *res = 0;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, (task_info_t)&info, &count) == KERN_SUCCESS)
        *virt = info.virtual_size, *res = info.resident_size;
}

static void
gen_status(Buf *b)
{
    uint64_t virt, res;
    char name[16];

    rss(&virt, &res);
    snprintf(name, sizeof(name), "%s", comm());
    put(b, "Name:\t%s\nUmask:\t0022\nState:\tR (running)\nTgid:\t%d\nNgid:\t0\nPid:\t%d\nPPid:\t%d\n", name, getpid(),
        getpid(), getppid());
    put(b, "TracerPid:\t0\nUid:\t%d\t%d\t%d\t%d\nGid:\t%d\t%d\t%d\t%d\nFDSize:\t256\n", getuid(), geteuid(),
        geteuid(), geteuid(), getgid(), getegid(), getegid(), getegid());
    put(b, "VmPeak:\t%llu kB\nVmSize:\t%llu kB\nVmHWM:\t%llu kB\nVmRSS:\t%llu kB\nThreads:\t%d\n",
        (unsigned long long)(virt / 1024), (unsigned long long)(virt / 1024), (unsigned long long)(res / 1024),
        (unsigned long long)(res / 1024), threads());
    put(b, "SigQ:\t0/0\nSigPnd:\t0000000000000000\nShdPnd:\t0000000000000000\nSigBlk:\t%016llx\n",
        (unsigned long long)signals_mask());
    put(b, "SigIgn:\t%016llx\nSigCgt:\t0000000000000000\nCapInh:\t0000000000000000\nCapPrm:\t0000000000000000\n",
        (unsigned long long)signals_ignored());
    put(b, "CapEff:\t0000000000000000\nCapBnd:\t000001ffffffffff\nSeccomp:\t0\nCpus_allowed_list:\t0-%d\n",
        ncpu() - 1);
}

// the 52 fields of /proc/pid/stat, in clock ticks of 100 Hz
static void
gen_pidstat(Buf *b)
{
    struct rusage ru;
    uint64_t virt, res;
    long ticks_u, ticks_s;

    getrusage(RUSAGE_SELF, &ru);
    rss(&virt, &res);
    ticks_u = ru.ru_utime.tv_sec * 100 + ru.ru_utime.tv_usec / 10000;
    ticks_s = ru.ru_stime.tv_sec * 100 + ru.ru_stime.tv_usec / 10000;
    put(b, "%d (%.15s) R %d %d %d 0 -1 4194304 %ld 0 %ld 0 %ld %ld 0 0 20 0 %d 0 %ld %llu %llu", getpid(), comm(),
        getppid(), getpgrp(), getsid(0), ru.ru_minflt, ru.ru_majflt, ticks_u, ticks_s, threads(),
        (long)((uptime() - 0) * 100), (unsigned long long)virt, (unsigned long long)(res / (uint64_t)getpagesize()));
    put(b, " 18446744073709551615 0 0 0 0 0 0 0 0 0 0 0 0 17 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\n");
}

static void
gen_cmdline(Buf *b)
{
    for (int i = 0; i < guest_argc; i++) {
        put(b, "%s", guest_argv[i]);
        put(b, "%c", 0);
    }
}

static void
gen_environ(Buf *b)
{
    for (char **e = environ; *e; e++) {
        put(b, "%s", *e);
        put(b, "%c", 0);
    }
}

static void
gen_mounts(Buf *b)
{
    struct statfs *m;
    int n = getmntinfo(&m, MNT_NOWAIT);

    for (int i = 0; i < n; i++)
        put(b, "%s %s %s %s 0 0\n", m[i].f_mntfromname, m[i].f_mntonname, m[i].f_fstypename,
            m[i].f_flags & MNT_RDONLY ? "ro" : "rw");
    put(b, "proc /proc proc rw 0 0\nsysfs /sys sysfs ro 0 0\n");
}

static void
gen_cpulist(Buf *b)
{
    int n = ncpu();

    if (n > 1)
        put(b, "0-%d\n", n - 1);
    else
        put(b, "0\n");
}

typedef enum { F_FILE, F_DIR, F_LINK } Kind;

typedef struct {
    const char *path;
    Kind kind;
    void (*gen)(Buf *);
    const char *text;
} Node;

static void gen_uptime(Buf *b) { double u = uptime(); put(b, "%.2f %.2f\n", u, u * ncpu()); }
static void gen_loadavg(Buf *b) { double l[3] = { 0 }; getloadavg(l, 3); put(b, "%.2f %.2f %.2f 1/%d %d\n", l[0], l[1], l[2], threads(), getpid()); }
static void gen_comm(Buf *b) { put(b, "%.15s\n", comm()); }
static void gen_pid_max(Buf *b) { put(b, "%d\n", 99999); }
static void gen_hostname(Buf *b) { char h[256]; if (gethostname(h, sizeof(h))) strcpy(h, "localhost"); put(b, "%s\n", h); }

// relative to /proc/self or /, the self files listed with the "self/" prefix
static const Node nodes[] = {
    { "proc", F_DIR, NULL, NULL },
    { "proc/self", F_LINK, NULL, NULL },
    { "proc/thread-self", F_LINK, NULL, NULL },
    { "self", F_DIR, NULL, NULL },
    { "self/fd", F_DIR, NULL, NULL },
    { "self/exe", F_LINK, NULL, NULL },
    { "self/cwd", F_LINK, NULL, NULL },
    { "self/maps", F_FILE, gen_maps, NULL },
    { "self/status", F_FILE, gen_status, NULL },
    { "self/stat", F_FILE, gen_pidstat, NULL },
    { "self/cmdline", F_FILE, gen_cmdline, NULL },
    { "self/environ", F_FILE, gen_environ, NULL },
    { "self/comm", F_FILE, gen_comm, NULL },
    { "self/mounts", F_FILE, gen_mounts, NULL },
    { "self/oom_score_adj", F_FILE, NULL, "0\n" },
    { "self/cgroup", F_FILE, NULL, "0::/\n" },
    { "self/limits", F_FILE, NULL, "Limit                     Soft Limit           Hard Limit           Units\n" },
    { "proc/cpuinfo", F_FILE, gen_cpuinfo, NULL },
    { "proc/meminfo", F_FILE, gen_meminfo, NULL },
    { "proc/stat", F_FILE, gen_stat, NULL },
    { "proc/uptime", F_FILE, gen_uptime, NULL },
    { "proc/loadavg", F_FILE, gen_loadavg, NULL },
    { "proc/mounts", F_FILE, gen_mounts, NULL },
    { "proc/filesystems", F_FILE, NULL, "\text4\n\tapfs\n\thfs\nnodev\tproc\nnodev\tsysfs\nnodev\ttmpfs\n" },
    { "proc/version", F_FILE, NULL, "Linux version 6.6.0-puredarwin (elftrans) #1 SMP PureDarwin elftrans\n" },
    { "proc/sys", F_DIR, NULL, NULL },
    { "proc/sys/kernel", F_DIR, NULL, NULL },
    { "proc/sys/kernel/ostype", F_FILE, NULL, "Linux\n" },
    { "proc/sys/kernel/osrelease", F_FILE, NULL, "6.6.0-puredarwin\n" },
    { "proc/sys/kernel/pid_max", F_FILE, gen_pid_max, NULL },
    { "proc/sys/kernel/hostname", F_FILE, gen_hostname, NULL },
    { "proc/sys/kernel/randomize_va_space", F_FILE, NULL, "2\n" },
    { "proc/sys/kernel/yama", F_DIR, NULL, NULL },
    { "proc/sys/kernel/yama/ptrace_scope", F_FILE, NULL, "1\n" },
    { "proc/sys/vm", F_DIR, NULL, NULL },
    { "proc/sys/vm/overcommit_memory", F_FILE, NULL, "0\n" },
    { "proc/sys/vm/max_map_count", F_FILE, NULL, "65530\n" },
    { "proc/sys/fs", F_DIR, NULL, NULL },
    { "proc/sys/fs/file-max", F_FILE, NULL, "9223372036854775807\n" },
    { "proc/sys/fs/nr_open", F_FILE, NULL, "1048576\n" },
    { "sys", F_DIR, NULL, NULL },
    { "sys/devices", F_DIR, NULL, NULL },
    { "sys/devices/system", F_DIR, NULL, NULL },
    { "sys/devices/system/cpu", F_DIR, NULL, NULL },
    { "sys/devices/system/cpu/online", F_FILE, gen_cpulist, NULL },
    { "sys/devices/system/cpu/possible", F_FILE, gen_cpulist, NULL },
    { "sys/devices/system/cpu/present", F_FILE, gen_cpulist, NULL },
    { "sys/kernel", F_DIR, NULL, NULL },
    { "sys/kernel/mm", F_DIR, NULL, NULL },
    { "sys/kernel/mm/transparent_hugepage", F_DIR, NULL, NULL },
    { "sys/kernel/mm/transparent_hugepage/enabled", F_FILE, NULL, "always madvise [never]\n" },
};

// a guest path inside /proc or /sys, as a key into nodes ("self/..." for this process), with the fd number of
// /proc/self/fd/N in *fdnum. 0 when the path is elsewhere: the host answers for it
static int
key_of(const char *path, char *key, size_t size, int *fdnum)
{
    char pid[24];
    size_t plen;
    const char *rest;

    *fdnum = -1;
    if (!path || path[0] != '/')
        return 0;
    while (path[1] == '/')
        path++;
    if (strncmp(path, "/proc", 5) && strncmp(path, "/sys", 4))
        return 0;
    if (!strncmp(path, "/sys", 4) && path[4] && path[4] != '/')
        return 0;
    if (!strncmp(path, "/proc", 5) && path[5] && path[5] != '/')
        return 0;
    snprintf(pid, sizeof(pid), "/proc/%d", getpid());
    plen = strlen(pid);
    rest = NULL;
    if (!strncmp(path, "/proc/self", 10) && (!path[10] || path[10] == '/'))
        rest = path + 10;
    else if (!strncmp(path, "/proc/thread-self", 17) && (!path[17] || path[17] == '/') && path[17])
        rest = path + 17;
    else if (!strncmp(path, pid, plen) && (!path[plen] || path[plen] == '/'))
        rest = path + plen;
    if (rest) {
        snprintf(key, size, "self%s", rest);
    } else {
        snprintf(key, size, "%s", path + 1);
    }
    // trailing slashes off
    for (size_t n = strlen(key); n > 1 && key[n - 1] == '/'; n--)
        key[n - 1] = 0;
    if (!strncmp(key, "self/fd/", 8)) {
        char *end;
        long n = strtol(key + 8, &end, 10);

        if (*end || n < 0)
            return 1;
        *fdnum = (int)n;
    }
    return 1;
}

static const Node *
find(const char *key)
{
    for (unsigned i = 0; i < sizeof(nodes) / sizeof(nodes[0]); i++) {
        if (!strcmp(nodes[i].path, key))
            return &nodes[i];
    }
    return NULL;
}

// a file holding the generated text, unlinked and at offset 0
static int
file_of(const char *text, size_t n)
{
    char path[PATH_MAX];
    const char *tmp = getenv("TMPDIR");
    int fd;

    snprintf(path, sizeof(path), "%s/elftrans-proc-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if ((fd = mkstemp(path)) < 0)
        return -linux_error(errno);
    unlink(path);
    if (n && write(fd, text, n) != (ssize_t)n) {
        close(fd);
        return -5;
    }
    lseek(fd, 0, SEEK_SET);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}

// a directory with one empty entry per name, opened and then removed: getdents lists what was in it
static int
dir_of(const char *const *names, unsigned n)
{
    char path[PATH_MAX], entry[PATH_MAX];
    const char *tmp = getenv("TMPDIR");
    int fd;

    snprintf(path, sizeof(path), "%s/elftrans-dir-XXXXXX", tmp && *tmp ? tmp : "/tmp");
    if (!mkdtemp(path))
        return -linux_error(errno);
    for (unsigned i = 0; i < n; i++) {
        int f;

        snprintf(entry, sizeof(entry), "%s/%s", path, names[i]);
        if ((f = open(entry, O_CREAT | O_WRONLY, 0444)) >= 0)
            close(f);
    }
    fd = open(path, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    // the entries stay until the next one is made: a removed directory reads as empty
    static char last[PATH_MAX];
    if (last[0]) {
        DIR *d = opendir(last);
        struct dirent *e;

        while (d && (e = readdir(d))) {
            if (strcmp(e->d_name, ".") && strcmp(e->d_name, "..")) {
                snprintf(entry, sizeof(entry), "%s/%s", last, e->d_name);
                unlink(entry);
            }
        }
        if (d)
            closedir(d);
        rmdir(last);
    }
    snprintf(last, sizeof(last), "%s", path);
    return fd < 0 ? -linux_error(errno) : fd;
}

static int
open_dir(const char *key)
{
    const char *names[256];
    char store[256][16];
    unsigned n = 0;
    size_t klen = strlen(key);

    if (!strcmp(key, "self/fd")) {
        int max = getdtablesize();

        for (int fd = 0; fd < max && n < 256; fd++) {
            if (fcntl(fd, F_GETFD) != -1) {
                snprintf(store[n], sizeof(store[n]), "%d", fd);
                names[n] = store[n], n++;
            }
        }
        return dir_of(names, n);
    }
    // a directory's children are the nodes one level below it
    for (unsigned i = 0; i < sizeof(nodes) / sizeof(nodes[0]) && n < 256; i++) {
        const char *p = nodes[i].path;

        if (!strncmp(p, key, klen) && p[klen] == '/' && !strchr(p + klen + 1, '/'))
            names[n++] = p + klen + 1;
    }
    if (!strcmp(key, "proc")) {
        snprintf(store[0], sizeof(store[0]), "%d", getpid());
        names[n++] = store[0];
    }
    if (!strcmp(key, "sys/devices/system/cpu")) {
        int c = ncpu();

        for (int i = 0; i < c && n < 256 && i < 200; i++) {
            snprintf(store[i + 1], sizeof(store[i + 1]), "cpu%d", i);
            names[n++] = store[i + 1];
        }
    }
    return dir_of(names, n);
}

int
procfs_open(const char *path, int linux_flags, long *result)
{
    char key[PATH_MAX];
    int fdnum;
    const Node *node;

    if (!key_of(path, key, sizeof(key), &fdnum))
        return 0;
    if (fdnum >= 0) {
        // reopening one of its own fds: what it refers to, from the start
        char real[PATH_MAX];

        if (fcntl(fdnum, F_GETPATH, real) < 0) {
            *result = -2;
        } else {
            int fd = open(real, (linux_flags & 3) == 0 ? O_RDONLY : (linux_flags & 3) == 1 ? O_WRONLY : O_RDWR);
            *result = fd < 0 ? -linux_error(errno) : fd;
        }
        return 1;
    }
    if (!strcmp(key, "self/exe")) {
        int fd = open(guest_exe, O_RDONLY | O_CLOEXEC);

        *result = fd < 0 ? -linux_error(errno) : fd;
        return 1;
    }
    if (!(node = find(key))) {
        // per-CPU directories under /sys/devices/system/cpu exist, everything else does not
        *result = -2;
        return 1;
    }
    if ((linux_flags & 3) != 0) {
        *result = -13;
        return 1;
    }
    if (node->kind == F_DIR || node->kind == F_LINK) {
        *result = open_dir(node->kind == F_LINK ? "self" : key);
        return 1;
    }
    {
        Buf b = { 0 };

        if (node->gen)
            node->gen(&b);
        else
            put(&b, "%s", node->text);
        *result = file_of(b.p ? b.p : "", b.n);
        free(b.p);
    }
    return 1;
}

static int
link_target(const char *key, int fdnum, char *out, size_t size)
{
    if (fdnum >= 0) {
        struct stat st;

        if (fcntl(fdnum, F_GETPATH, out) >= 0)
            return 0;
        // no path: pipes and sockets are named the way Linux names them
        if (fstat(fdnum, &st))
            return -2;
        snprintf(out, size, "%s:[%llu]", S_ISFIFO(st.st_mode) ? "pipe" : S_ISSOCK(st.st_mode) ? "socket" :
                 "anon_inode", (unsigned long long)st.st_ino);
        return 0;
    }
    if (!strcmp(key, "self/exe"))
        return snprintf(out, size, "%s", guest_exe) < (int)size ? 0 : -36;
    if (!strcmp(key, "self/cwd"))
        return getcwd(out, size) ? 0 : -linux_error(errno);
    if (!strcmp(key, "proc/self") || !strcmp(key, "proc/thread-self"))
        return snprintf(out, size, "%d", getpid()) < (int)size ? 0 : -36;
    return -22;
}

int
procfs_readlink(const char *path, char *buf, size_t size, long *result)
{
    char key[PATH_MAX], target[PATH_MAX];
    int fdnum, rc;
    const Node *node;

    if (!key_of(path, key, sizeof(key), &fdnum))
        return 0;
    if (fdnum < 0 && (!(node = find(key)) || node->kind != F_LINK)) {
        *result = node ? -22 : -2;
        return 1;
    }
    if ((rc = link_target(key, fdnum, target, sizeof(target)))) {
        *result = rc;
        return 1;
    }
    size_t n = strlen(target);

    if (n > size)
        n = size;
    memcpy(buf, target, n);
    *result = (long)n;
    return 1;
}

int
procfs_stat(const char *path, int follow, struct stat *st, long *result)
{
    char key[PATH_MAX], target[PATH_MAX];
    int fdnum;
    const Node *node;
    Kind kind;

    if (!key_of(path, key, sizeof(key), &fdnum))
        return 0;
    node = fdnum >= 0 ? NULL : find(key);
    if (fdnum < 0 && !node) {
        char *end;
        // /sys/devices/system/cpu/cpuN
        if (!strncmp(key, "sys/devices/system/cpu/cpu", 26) && strtol(key + 26, &end, 10) < ncpu() && !*end &&
            end != key + 26) {
            kind = F_DIR;
        } else {
            *result = -2;
            return 1;
        }
    } else {
        kind = fdnum >= 0 ? F_LINK : node->kind;
    }
    if (kind == F_LINK && follow) {
        long rc = link_target(key, fdnum, target, sizeof(target));

        if (!rc && !strcmp(key, "proc/self"))
            kind = F_DIR;
        else if (rc || stat(target, st)) {
            *result = rc ? rc : -linux_error(errno);
            return 1;
        } else {
            *result = 0;
            return 1;
        }
    }
    memset(st, 0, sizeof(*st));
    st->st_mode = kind == F_DIR ? S_IFDIR | 0555 : kind == F_LINK ? S_IFLNK | 0777 : S_IFREG | 0444;
    st->st_nlink = kind == F_DIR ? 2 : 1;
    st->st_uid = geteuid(), st->st_gid = getegid();
    st->st_blksize = 1024;
    st->st_ino = 0x10000 + (ino_t)(node ? node - nodes : 0);
    clock_gettime(CLOCK_REALTIME, &st->st_mtimespec);
    st->st_atimespec = st->st_ctimespec = st->st_mtimespec;
    *result = 0;
    return 1;
}

// sysinfo(2): uptime, loads in 1/65536, memory in bytes (mem_unit 1), no swap
long
procfs_sysinfo(void *out)
{
    struct {
        long uptime;
        unsigned long loads[3], totalram, freeram, sharedram, bufferram, totalswap, freeswap;
        unsigned short procs, pad;
        unsigned long totalhigh, freehigh;
        unsigned mem_unit;
        char reserved[4];
    } *si = out;
    vm_statistics64_data_t vm;
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    double l[3] = { 0 };

    _Static_assert(sizeof(*si) == 112, "Linux struct sysinfo");
    memset(si, 0, sizeof(*si));
    si->uptime = (long)uptime();
    getloadavg(l, 3);
    for (int i = 0; i < 3; i++)
        si->loads[i] = (unsigned long)(l[i] * 65536);
    si->totalram = (unsigned long)sysctl_long("hw.memsize");
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vm, &count) == KERN_SUCCESS)
        si->freeram = (unsigned long)vm.free_count * (unsigned long)getpagesize();
    si->procs = 1, si->mem_unit = 1;
    return 0;
}
