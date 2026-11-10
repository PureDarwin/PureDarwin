// syscalls.c - Linux x86-64 syscalls on Darwin: the way in from the guest, translation, the way back
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <time.h>
#include <sys/ucred.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <mach/exception_types.h>
#include <mach/thread_status.h>
#include "mach_exc.h"
#include "elftrans.h"

#define LINUX_MAP_NORESERVE 0x4000UL

uintptr_t guest_library_bias;
static volatile unsigned traced_syscalls;
static unsigned trace_limit;
#define BRK_SIZE (256UL * 1024 * 1024)
static uintptr_t brk_base, brk_current;
static int brk_reserved;
static DIR *guest_dirs[1024];
static pthread_mutex_t futex_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int next_guest_tid = 1000;
static volatile unsigned next_memfd;
static __thread int guest_tid;
static __thread int *guest_clear_tid;
static __thread LinuxRobustHead *guest_robust_head;
static __thread uint64_t guest_fs_base;
static mach_port_t syscall_port;
// this executable, for an exec of another Linux program
static char elftrans_self[PATH_MAX];

typedef struct FutexWaiter {
    struct FutexWaiter *next;
    pthread_cond_t changed;
    int *address;
    uint32_t bitset;
    int notified;
} FutexWaiter;

static FutexWaiter *futex_waiters;

typedef struct {
    GuestRegs regs;
    uintptr_t tls;
    uint64_t signal_mask;
    int tid;
    int *child_tid;
    int *clear_tid;
} GuestClone;

uint64_t xsave_size = 512;
uint8_t elftrans_have_xsave;
static uint64_t signal_reserve;

// Darwin errno values from 35 on to Linux ones, 1-34 are the same on both
static const unsigned char linux_errno[] = {
    [35] = 11, [36] = 115, [37] = 114, [38] = 88, [39] = 89, [40] = 90, [41] = 91, [42] = 92,
    [43] = 93, [44] = 94, [45] = 95, [46] = 96, [47] = 97, [48] = 98, [49] = 99, [50] = 100,
    [51] = 101, [52] = 102, [53] = 103, [54] = 104, [55] = 105, [56] = 106, [57] = 107, [58] = 108,
    [59] = 109, [60] = 110, [61] = 111, [62] = 40, [63] = 36, [64] = 112, [65] = 113, [66] = 39,
    [67] = 11, [68] = 87, [69] = 122, [70] = 116, [71] = 66, [72] = 5, [73] = 5, [74] = 5,
    [75] = 5, [76] = 5, [77] = 37, [78] = 38, [79] = 22, [80] = 13, [81] = 13, [82] = 5,
    [83] = 5, [84] = 75, [85] = 8, [86] = 8, [87] = 8, [88] = 8, [89] = 125, [90] = 43,
    [91] = 42, [92] = 84, [93] = 61, [94] = 74, [95] = 72, [96] = 61, [97] = 67, [98] = 63,
    [99] = 60, [100] = 71, [101] = 62, [102] = 95, [103] = 5, [104] = 131, [105] = 130, [106] = 105,
};

long
linux_error(int error)
{
    if (error > 0 && error <= 34) return error;
    if (error > 0 && error < (int)sizeof(linux_errno) && linux_errno[error]) return linux_errno[error];
    return 5;
}

static long
linux_prlimit64(int pid, int resource, const LinuxRlimit *requested,
    LinuxRlimit *previous)
{
    if (pid && pid != getpid()) return -3;
    int native;
    switch (resource) {
    case 0: native = RLIMIT_CPU; break;
    case 1: native = RLIMIT_FSIZE; break;
    case 2: native = RLIMIT_DATA; break;
    case 3: native = RLIMIT_STACK; break;
    case 4: native = RLIMIT_CORE; break;
    case 7: native = RLIMIT_NOFILE; break;
    case 9: native = RLIMIT_AS; break;
    default: return -22;
    }
    struct rlimit current;
    if (getrlimit(native, &current)) return -linux_error(errno);
    if (previous) {
        previous->current = current.rlim_cur;
        previous->maximum = current.rlim_max;
    }
    if (requested) {
        struct rlimit next = {
            .rlim_cur = (rlim_t)requested->current,
            .rlim_max = (rlim_t)requested->maximum
        };
        if (setrlimit(native, &next)) return -linux_error(errno);
    }
    return 0;
}

static int
open_flags(unsigned long linux_flags)
{
    int flags;
    if (linux_flags & ~0xb8fc3UL) return -1;
    switch (linux_flags & 3) {
    case 0: flags = O_RDONLY; break;
    case 1: flags = O_WRONLY; break;
    case 2: flags = O_RDWR; break;
    default: return -1;
    }
    if (linux_flags & 0x40) flags |= O_CREAT;
    if (linux_flags & 0x80) flags |= O_EXCL;
    if (linux_flags & 0x100) flags |= O_NOCTTY;
    if (linux_flags & 0x200) flags |= O_TRUNC;
    if (linux_flags & 0x400) flags |= O_APPEND;
    if (linux_flags & 0x800) flags |= O_NONBLOCK;
    if (linux_flags & 0x10000) flags |= O_DIRECTORY;
    if (linux_flags & 0x20000) flags |= O_NOFOLLOW;
    if (linux_flags & 0x80000) flags |= O_CLOEXEC;
    return flags;
}

static int
socket_type(int linux_type)
{
    if (linux_type & ~(0xf | 0x800 | 0x80000)) return -1;
    int type = linux_type & 0xf;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -1;
    return type;
}

static int
socket_flags(int fd, int linux_type)
{
    if (fd < 0) return fd;
    if ((linux_type & 0x800) && fcntl(fd, F_SETFL,
        fcntl(fd, F_GETFL) | O_NONBLOCK)) goto fail;
    if ((linux_type & 0x80000) && fcntl(fd, F_SETFD, FD_CLOEXEC))
        goto fail;
    return fd;
fail:;
    int error = errno;
    close(fd);
    errno = error;
    return -1;
}


static long
linux_fcntl(int fd, int command, long argument)
{
    int native_command = command;
    if (command == 1030) native_command = F_DUPFD_CLOEXEC;
    else if (command < 0 || command > 4) return -38;
    if (command == 4) {
        int old = fcntl(fd, F_GETFL);
        if (old < 0) return -linux_error(errno);
        argument = (old & ~(O_NONBLOCK | O_APPEND)) |
            (argument & 0x800 ? O_NONBLOCK : 0) |
            (argument & 0x400 ? O_APPEND : 0);
    }
    int result = fcntl(fd, native_command, argument);
    if (result < 0) return -linux_error(errno);
    if (command == 3) {
        if (result & O_NONBLOCK) result = (result & ~O_NONBLOCK) | 0x800;
        if (result & O_APPEND) result = (result & ~O_APPEND) | 0x400;
    }
    return result;
}

static long
linux_memfd_create(const char *name, unsigned flags)
{
    if (!name || (flags & ~1U)) return -22;
    for (int attempt = 0; attempt < 100; ++attempt) {
        char path[64];
        unsigned number = __sync_fetch_and_add(&next_memfd, 1);
        snprintf(path, sizeof(path), "/elftrans-%ld-%u", (long)getpid(),
            number);
        int fd = shm_open(path, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd < 0) {
            if (errno == EEXIST) continue;
            return -linux_error(errno);
        }
        shm_unlink(path);
        if ((flags & 1) && fcntl(fd, F_SETFD, FD_CLOEXEC)) {
            int error = errno;
            close(fd);
            return -linux_error(error);
        }
        return fd;
    }
    return -17;
}

static int
message_flags(int linux_flags, int receiving)
{
    int allowed = 0x1 | 0x2 | 0x40 | 0x100 | 0x4000;
    if (receiving) allowed |= 0x40000000;
    if (linux_flags & ~allowed) return -1;
    int flags = linux_flags & 0x3;
    if (linux_flags & 0x40) flags |= MSG_DONTWAIT;
    if (linux_flags & 0x100) flags |= MSG_WAITALL;
    if (linux_flags & 0x4000) flags |= MSG_NOSIGNAL;
    return flags;
}

static long
linux_sendmsg(int fd, const LinuxMsgHdr *guest, int linux_flags)
{
    int flags = message_flags(linux_flags, 0);
    struct sockaddr_storage address;
    socklen_t address_length = 0;
    if (guest->name) {
        int error = net_to_native(guest->name, guest->name_length, &address, &address_length);
        if (error) return -linux_error(error);
    }
    if (flags < 0 || guest->iov_count > 1024 ||
        guest->control_length > 4096) {
        fprintf(stderr, "elftrans: sendmsg fd=%d flags=%#x name=%p iov=%u control=%u\n",
            fd, linux_flags, guest->name, guest->iov_count,
            guest->control_length);
        return -22;
    }

    unsigned char control[4096] = {0};
    size_t used = 0;
    const unsigned char *input = guest->control;
    for (size_t offset = 0; offset < guest->control_length;) {
        if (guest->control_length - offset < sizeof(LinuxCmsg)) return -22;
        LinuxCmsg source;
        memcpy(&source, input + offset, sizeof(source));
        if (source.length < sizeof(source) ||
            source.length > guest->control_length - offset ||
            source.level != 1 || source.type != SCM_RIGHTS) return -95;
        size_t payload = source.length - sizeof(source);
        size_t space = CMSG_SPACE(payload);
        if (space > sizeof(control) - used) return -22;
        struct cmsghdr *native = (struct cmsghdr *)(void *)(control + used);
        native->cmsg_len = CMSG_LEN(payload);
        native->cmsg_level = SOL_SOCKET;
        native->cmsg_type = SCM_RIGHTS;
        memcpy(CMSG_DATA(native), input + offset + sizeof(source), payload);
        used += space;
        offset += (source.length + 7) & ~(size_t)7;
    }

    struct msghdr native = {0};
    native.msg_name = guest->name ? &address : NULL;
    native.msg_namelen = address_length;
    native.msg_iov = guest->iov;
    native.msg_iovlen = (int)guest->iov_count;
    native.msg_control = used ? control : NULL;
    native.msg_controllen = (socklen_t)used;
    ssize_t sent = sendmsg(fd, &native, flags);
    if (sent < 0)
        fprintf(stderr, "elftrans: sendmsg fd=%d flags=%#x errno=%d\n",
            fd, linux_flags, errno);
    return sent < 0 ? -linux_error(errno) : sent;
}

static long
linux_recvmsg(int fd, LinuxMsgHdr *guest, int linux_flags)
{
    int flags = message_flags(linux_flags, 1);
    if (flags < 0 || guest->iov_count > 1024 ||
        guest->control_length > 4096) return -22;

    unsigned char control[4096] = {0};
    struct sockaddr_storage from = {0};
    struct msghdr native = {0};
    native.msg_name = guest->name ? &from : NULL;
    native.msg_namelen = guest->name ? sizeof(from) : 0;
    native.msg_iov = guest->iov;
    native.msg_iovlen = (int)guest->iov_count;
    native.msg_control = guest->control_length ? control : NULL;
    native.msg_controllen = guest->control_length ? sizeof(control) : 0;
    ssize_t received = recvmsg(fd, &native, flags & ~0x40000000);
    if (received < 0) return -linux_error(errno);

    size_t used = 0;
    for (struct cmsghdr *item = CMSG_FIRSTHDR(&native); item;
        item = CMSG_NXTHDR(&native, item)) {
        if (item->cmsg_level != SOL_SOCKET || item->cmsg_type != SCM_RIGHTS)
            continue;
        size_t payload = item->cmsg_len - CMSG_LEN(0);
        size_t space = (sizeof(LinuxCmsg) + payload + 7) & ~(size_t)7;
        int *descriptors = (int *)(void *)CMSG_DATA(item);
        for (size_t i = 0; i < payload / sizeof(int); ++i)
            if (linux_flags & 0x40000000)
                fcntl(descriptors[i], F_SETFD, FD_CLOEXEC);
        if (space > guest->control_length - used) {
            for (size_t i = 0; i < payload / sizeof(int); ++i)
                close(descriptors[i]);
            native.msg_flags |= MSG_CTRUNC;
            continue;
        }
        LinuxCmsg output = {sizeof(output) + payload, 1, SCM_RIGHTS};
        unsigned char *target = (unsigned char *)guest->control + used;
        memcpy(target, &output, sizeof(output));
        memcpy(target + sizeof(output), descriptors, payload);
        memset(target + sizeof(output) + payload, 0,
            space - sizeof(output) - payload);
        used += space;
    }
    guest->control_length = (uint32_t)used;
    if (guest->name && native.msg_namelen)
        net_to_linux((const struct sockaddr *)&from, native.msg_namelen, guest->name, &guest->name_length);
    else
        guest->name_length = 0;
    guest->flags = native.msg_flags;
    return received;
}

static void
copy_stat(LinuxStat *out, const struct stat *in)
{
    memset(out, 0, sizeof(*out));
    out->dev = in->st_dev;
    out->ino = in->st_ino;
    out->nlink = in->st_nlink;
    out->mode = in->st_mode;
    out->uid = in->st_uid;
    out->gid = in->st_gid;
    out->rdev = in->st_rdev;
    out->size = in->st_size;
    out->blksize = in->st_blksize;
    out->blocks = in->st_blocks;
#ifdef __APPLE__
    out->atime = (LinuxTime){in->st_atimespec.tv_sec, in->st_atimespec.tv_nsec};
    out->mtime = (LinuxTime){in->st_mtimespec.tv_sec, in->st_mtimespec.tv_nsec};
    out->ctime = (LinuxTime){in->st_ctimespec.tv_sec, in->st_ctimespec.tv_nsec};
#else
    out->atime = (LinuxTime){in->st_atim.tv_sec, in->st_atim.tv_nsec};
    out->mtime = (LinuxTime){in->st_mtim.tv_sec, in->st_mtim.tv_nsec};
    out->ctime = (LinuxTime){in->st_ctim.tv_sec, in->st_ctim.tv_nsec};
#endif
}

static void
copy_statfs(LinuxStatfs *out, const struct statvfs *in)
{
    memset(out, 0, sizeof(*out));
    out->bsize = in->f_bsize;
    out->blocks = in->f_blocks;
    out->bfree = in->f_bfree;
    out->bavail = in->f_bavail;
    out->files = in->f_files;
    out->ffree = in->f_ffree;
    out->fsid[0] = (uint32_t)in->f_fsid;
    out->fsid[1] = (uint32_t)(in->f_fsid >> 32);
    out->namelen = in->f_namemax;
    out->frsize = in->f_frsize;
    out->flags = in->f_flag & 3;
}

static long
linux_getdents(int fd, void *buffer, size_t size)
{
    if (fd < 0 || fd >= (int)(sizeof(guest_dirs) / sizeof(*guest_dirs)))
        return -9;
    if (!guest_dirs[fd]) {
        int copy = dup(fd);
        if (copy < 0) return -linux_error(errno);
        guest_dirs[fd] = fdopendir(copy);
        if (!guest_dirs[fd]) {
            int error = errno;
            close(copy);
            return -linux_error(error);
        }
    }

    size_t used = 0;
    for (;;) {
        long previous = telldir(guest_dirs[fd]);
        errno = 0;
        struct dirent *entry = readdir(guest_dirs[fd]);
        if (!entry) return errno && !used ? -linux_error(errno) : (long)used;
        size_t name_size = strlen(entry->d_name) + 1;
        size_t length = (sizeof(LinuxDirent) + name_size + 7) & ~(size_t)7;
        if (length > size - used) {
            seekdir(guest_dirs[fd], previous);
            return used ? (long)used : -22;
        }
        LinuxDirent *out = (LinuxDirent *)((char *)buffer + used);
        out->ino = entry->d_ino;
        out->offset = telldir(guest_dirs[fd]);
        out->length = (uint16_t)length;
        out->type = entry->d_type;
        memcpy(out->name, entry->d_name, name_size);
        memset((char *)out + sizeof(*out) + name_size, 0,
            length - sizeof(*out) - name_size);
        used += length;
    }
}

static int
wake_futex_locked(int *address, int limit, uint32_t bitset)
{
    int woken = 0;
    for (FutexWaiter *waiter = futex_waiters; waiter && woken < limit;
        waiter = waiter->next) {
        if (waiter->address != address || waiter->notified ||
            !(waiter->bitset & bitset)) continue;
        waiter->notified = 1;
        pthread_cond_signal(&waiter->changed);
        ++woken;
    }
    return woken;
}

static void
release_robust_node(uintptr_t node, long offset, int tid)
{
    if (!node || offset > 4096 || offset < -4096) return;
    int *futex = (int *)(uintptr_t)((intptr_t)node + offset);
    int value = *futex;
    if ((value & 0x3fffffff) != tid) return;
    *futex = (value & 0x80000000) | 0x40000000;
    wake_futex_locked(futex, 1, UINT32_MAX);
}

static void
release_robust_list(void)
{
    LinuxRobustHead *head = guest_robust_head;
    if (!head) return;
    pthread_mutex_lock(&futex_lock);
    uintptr_t current = head->list.next & ~(uintptr_t)1;
    uintptr_t end = (uintptr_t)&head->list;
    for (unsigned i = 0; current && current != end && i < 2048; ++i) {
        uintptr_t next = ((LinuxRobustNode *)current)->next & ~(uintptr_t)1;
        if (current != (head->pending & ~(uintptr_t)1))
            release_robust_node(current, head->futex_offset, guest_tid);
        current = next;
    }
    release_robust_node(head->pending & ~(uintptr_t)1,
        head->futex_offset, guest_tid);
    pthread_mutex_unlock(&futex_lock);
}

static long
linux_futex(int *address, int operation, int value,
    const struct timespec *timeout, uint32_t bitset)
{
    int command = operation & 0x7f;
    if (operation & ~(0x7f | 0x80 | 0x100)) return -38;
    if (command == 1 || command == 10) {
        if (command == 1) bitset = UINT32_MAX;
        if (!bitset) return -22;
        if (value < 0) return -22;
        pthread_mutex_lock(&futex_lock);
        int woken = wake_futex_locked(address, value, bitset);
        pthread_mutex_unlock(&futex_lock);
        return woken;
    }
    if (command != 0 && command != 9) {
        char line[80];
        int count = snprintf(line, sizeof(line),
            "elftrans: unsupported futex operation %#x\n", operation);
        if (count > 0 && count < (int)sizeof(line))
            write(2, line, (size_t)count);
        return -38;
    }
    if (command == 0) bitset = UINT32_MAX;
    if (!bitset) return -22;

    struct timespec deadline;
    if (timeout) {
        if (timeout->tv_sec < 0 || timeout->tv_nsec < 0 ||
            timeout->tv_nsec >= 1000000000) return -22;
        clock_gettime(CLOCK_REALTIME, &deadline);
        if (command == 9) {
            if (operation & 0x100) deadline = *timeout;
            else {
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);
                deadline.tv_sec += timeout->tv_sec - now.tv_sec;
                deadline.tv_nsec += timeout->tv_nsec - now.tv_nsec;
            }
        } else {
            deadline.tv_sec += timeout->tv_sec;
            deadline.tv_nsec += timeout->tv_nsec;
        }
        if (deadline.tv_nsec < 0) {
            --deadline.tv_sec;
            deadline.tv_nsec += 1000000000;
        }
        if (deadline.tv_nsec >= 1000000000) {
            ++deadline.tv_sec;
            deadline.tv_nsec -= 1000000000;
        }
    }

    FutexWaiter waiter = {.address = address, .bitset = bitset};
    pthread_cond_init(&waiter.changed, NULL);
    pthread_mutex_lock(&futex_lock);
    if (*address != value) {
        pthread_mutex_unlock(&futex_lock);
        pthread_cond_destroy(&waiter.changed);
        return -11;
    }
    waiter.next = futex_waiters;
    futex_waiters = &waiter;
    int error = 0;
    while (!waiter.notified && !error)
        error = timeout ? pthread_cond_timedwait(&waiter.changed,
            &futex_lock, &deadline) :
            pthread_cond_wait(&waiter.changed, &futex_lock);
    FutexWaiter **link = &futex_waiters;
    while (*link != &waiter) link = &(*link)->next;
    *link = waiter.next;
    pthread_mutex_unlock(&futex_lock);
    pthread_cond_destroy(&waiter.changed);
    return waiter.notified ? 0 : error == ETIMEDOUT ? -110 :
        -linux_error(error);
}

// the guest's thread pointer, FS base on x86-64 Linux, through machdep call 2
static int
set_fs_base(uint64_t base)
{
    register uint64_t call __asm__("rax") = 0x03000002;
    register uint64_t value __asm__("rdi") = base;

    __asm__ volatile("syscall" : "+a"(call) : "D"(value) : "rcx", "r11", "memory");
    if (call)
        return -1;
    guest_fs_base = base;
    return 0;
}

// a signal stack per guest thread: the trap handler must never run on the stack it writes the frame onto
static int
signal_stack(void)
{
    stack_t ss = {0};

    ss.ss_size = 64 * 1024;
    ss.ss_sp = mmap(NULL, ss.ss_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    if (ss.ss_sp == MAP_FAILED)
        return -1;
    return sigaltstack(&ss, NULL);
}

static void *
run_guest_clone(void *argument)
{
    GuestClone clone = *(GuestClone *)argument;

    free(argument);
    guest_tid = clone.tid;
    guest_clear_tid = clone.clear_tid;
    guest_robust_head = NULL;
    if (clone.child_tid) *clone.child_tid = clone.tid;
    if (signal_stack() || set_fs_base(clone.tls)) _exit(126);
    signals_thread_start(clone.tid, clone.signal_mask);
    guest_in_host = 0;
    elftrans_restore(&clone.regs);
}

// a new thread starts with the parent's registers, as a Linux clone leaves them: 0 in rax, its own stack
static long
linux_clone(const GuestRegs *parent)
{
    uintptr_t flags = parent->rdi, stack = parent->rsi, tls = parent->r8;
    int *parent_tid = (int *)(uintptr_t)parent->rdx, *child_tid = (int *)(uintptr_t)parent->r10;

    if ((flags & ~0x17d0f00UL) || (flags & 0x10f00) != 0x10f00 || !stack || !tls)
        return -22;
    GuestClone *clone = malloc(sizeof(*clone));
    if (!clone) return -12;
    clone->regs = *parent;
    clone->regs.rax = 0, clone->regs.rsp = stack;
    clone->regs.rcx = parent->rip, clone->regs.r11 = parent->rflags;
    clone->tls = tls;
    clone->signal_mask = signals_mask();
    clone->tid = __sync_fetch_and_add(&next_guest_tid, 1);
    clone->child_tid = flags & 0x1000000 ? child_tid : NULL;
    clone->clear_tid = flags & 0x200000 ? child_tid : NULL;
    if (flags & 0x100000) *parent_tid = clone->tid;
    int tid = clone->tid;
    pthread_t thread;
    int error = pthread_create(&thread, NULL, run_guest_clone, clone);
    if (error) {
        free(clone);
        return -linux_error(error);
    }
    pthread_detach(thread);
    return tid;
}

static long
sys_read(GuestRegs *r)
{
    long result = -38;
    if (events_read((int)r->rdi, (void *)(uintptr_t)r->rsi, (size_t)r->rdx, &result))
        return result;
    ssize_t n = read((int)r->rdi, (void *)(uintptr_t)r->rsi,
        (size_t)r->rdx);
    result = n < 0 ? -linux_error(errno) : n;
    return result;
}

static long
sys_write(GuestRegs *r)
{
    long result = -38;
    if (events_write((int)r->rdi, (const void *)(uintptr_t)r->rsi, (size_t)r->rdx, &result))
        return result;
    ssize_t n = write((int)r->rdi, (void *)(uintptr_t)r->rsi,
        (size_t)r->rdx);
    result = n < 0 ? -linux_error(errno) : n;
    return result;
}

static long
sys_pread64(GuestRegs *r)
{
    long result = -38;
    ssize_t n = pread((int)r->rdi, (void *)(uintptr_t)r->rsi,
        (size_t)r->rdx, (off_t)r->r10);
    result = n < 0 ? -linux_error(errno) : n;
    return result;
}

static long
sys_access(GuestRegs *r)
{
    struct stat st;
    long result;

    if (procfs_stat((const char *)(uintptr_t)r->rdi, 1, &st, &result))
        return result ? result : ((r->rsi & 2) ? -13 : 0);
    return access((const char *)(uintptr_t)r->rdi,
        (int)r->rsi) ? -linux_error(errno) : 0;
}

static long
sys_faccessat(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int dirfd = (int)r->rdi;
    int flags = nr == 439 ? (int)r->r10 : 0;
    if (dirfd == -100) dirfd = AT_FDCWD;
    struct stat st;
    if (procfs_stat((const char *)(uintptr_t)r->rsi, 1, &st, &result))
        return result ? result : ((r->rdx & 2) ? -13 : 0);
    if (flags & ~(0x100 | 0x200)) result = -22;
    else result = faccessat(dirfd,
        (const char *)(uintptr_t)r->rsi, (int)r->rdx,
        flags & 0x100 ? AT_SYMLINK_NOFOLLOW : 0) ?
            -linux_error(errno) : 0;
    return result;
}

static long
sys_getuid(GuestRegs *r)
{
    (void)r;
    return getuid();
}

static long
sys_getgid(GuestRegs *r)
{
    (void)r;
    return getgid();
}

static long
sys_geteuid(GuestRegs *r)
{
    (void)r;
    return geteuid();
}

static long
sys_getegid(GuestRegs *r)
{
    (void)r;
    return getegid();
}

static long
sys_readlink(GuestRegs *r)
{
    long result = -38;
    // readlinkat (267) has the directory first
    int at = (long)r->rax == 267;
    int dirfd = at ? ((int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi) : AT_FDCWD;
    const char *path = (const char *)(uintptr_t)(at ? r->rsi : r->rdi);
    char *buf = (char *)(uintptr_t)(at ? r->rdx : r->rsi);
    size_t size = (size_t)(at ? r->r10 : r->rdx);

    if (procfs_readlink(path, buf, size, &result))
        return result;
    ssize_t n = readlinkat(dirfd, path, buf, size);
    result = n < 0 ? -linux_error(errno) : n;
    return result;
}

static long
sys_unlink(GuestRegs *r)
{
    return unlink((const char *)(uintptr_t)r->rdi) ?
        -linux_error(errno) : 0;
}

static long
sys_link(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int oldfd = nr == 86 ? AT_FDCWD : (int)r->rdi;
    int newfd = nr == 86 ? AT_FDCWD : (int)r->rdx;
    const char *oldpath = (const char *)(uintptr_t)(nr == 86 ?
        r->rdi : r->rsi);
    const char *newpath = (const char *)(uintptr_t)(nr == 86 ?
        r->rsi : r->r10);
    int flags = nr == 86 ? 0 : (int)r->r8;
    if (oldfd == -100) oldfd = AT_FDCWD;
    if (newfd == -100) newfd = AT_FDCWD;
    if (flags & ~0x400) result = -22;
    else result = linkat(oldfd, oldpath, newfd, newpath,
        flags & 0x400 ? AT_SYMLINK_FOLLOW : 0) ?
            -linux_error(errno) : 0;
    return result;
}

static long
sys_symlink(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    const char *target = (const char *)(uintptr_t)r->rdi;
    int dirfd = nr == 88 ? AT_FDCWD : (int)r->rsi;
    const char *path = (const char *)(uintptr_t)(nr == 88 ?
        r->rsi : r->rdx);
    if (dirfd == -100) dirfd = AT_FDCWD;
    result = symlinkat(target, dirfd, path) ? -linux_error(errno) : 0;
    return result;
}

static long
sys_fsync(GuestRegs *r)
{
    return fsync((int)r->rdi) ? -linux_error(errno) : 0;
}

static long
sys_fchown(GuestRegs *r)
{
    return fchown((int)r->rdi, (uid_t)r->rsi,
        (gid_t)r->rdx) ? -linux_error(errno) : 0;
}

static long
sys_rename(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int oldfd = nr == 82 ? AT_FDCWD : (int)r->rdi;
    int newfd = nr == 82 ? AT_FDCWD : (int)r->rdx;
    const char *oldpath = (const char *)(uintptr_t)(nr == 82 ?
        r->rdi : r->rsi);
    const char *newpath = (const char *)(uintptr_t)(nr == 82 ?
        r->rsi : r->r10);
    if (oldfd == -100) oldfd = AT_FDCWD;
    if (newfd == -100) newfd = AT_FDCWD;
    result = renameat(oldfd, oldpath, newfd, newpath) ?
        -linux_error(errno) : 0;
    return result;
}

static long
sys_mkdir(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int dirfd = nr == 83 ? AT_FDCWD : (int)r->rdi;
    const char *path = (const char *)(uintptr_t)(nr == 83 ?
        r->rdi : r->rsi);
    mode_t mode = (mode_t)(nr == 83 ? r->rsi : r->rdx);
    if (dirfd == -100) dirfd = AT_FDCWD;
    result = mkdirat(dirfd, path, mode) ? -linux_error(errno) : 0;
    return result;
}

static long
sys_pipe(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int flags = nr == 293 ? (int)r->rsi : 0;
    int pair[2];
    if (flags & ~(0x800 | 0x80000)) result = -22;
    else if (pipe(pair)) result = -linux_error(errno);
    else {
        for (unsigned i = 0; i < 2; ++i) {
            if (flags & 0x800) fcntl(pair[i], F_SETFL, O_NONBLOCK);
            if (flags & 0x80000) fcntl(pair[i], F_SETFD, FD_CLOEXEC);
        }
        int *out = (int *)(uintptr_t)r->rdi;
        out[0] = pair[0];
        out[1] = pair[1];
        result = 0;
    }
    return result;
}

static long
sys_gettimeofday(GuestRegs *r)
{
    return gettimeofday((struct timeval *)(uintptr_t)r->rdi, NULL) ?
        -linux_error(errno) : 0;
}


static long
sys_getrandom(GuestRegs *r)
{
    long result = -38;
    if (r->rdx & ~3UL) result = -22;
    else {
        arc4random_buf((void *)(uintptr_t)r->rdi, (size_t)r->rsi);
        result = r->rsi;
    }
    return result;
}

static long
sys_open(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    unsigned long linux_flags = (unsigned long)(nr == 2 ?
        r->rsi : r->rdx);
    int flags = open_flags(linux_flags);
    if (flags >= 0) {
        int dirfd = nr == 2 ? AT_FDCWD : (int)r->rdi;
        const char *path = (const char *)(uintptr_t)(nr == 2 ?
            r->rdi : r->rsi);
        if (dirfd == -100) dirfd = AT_FDCWD;
        if (procfs_open(path, (int)linux_flags, &result))
            return result;
        int fd = openat(dirfd, path, flags,
            (mode_t)(nr == 2 ? r->rdx : r->r10));
        result = fd < 0 ? -linux_error(errno) : fd;
    } else {
        result = -22;
    }
    return result;
}

static long
sys_close(GuestRegs *r)
{
    long result = -38;
    int fd = (int)r->rdi;
    if (fd >= 0 && fd < (int)(sizeof(guest_dirs) / sizeof(*guest_dirs)) &&
        guest_dirs[fd]) {
        closedir(guest_dirs[fd]);
        guest_dirs[fd] = NULL;
    }
    events_close(fd);
    result = close(fd) ? -linux_error(errno) : 0;
    return result;
}

static long
sys_dup(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int oldfd = (int)r->rdi;
    int newfd = (int)r->rsi;
    int flags = nr == 292 ? (int)r->rdx : 0;
    if (flags & ~0x80000 || (nr == 292 && oldfd == newfd))
        result = -22;
    else {
        int fd = nr == 32 ? dup(oldfd) : dup2(oldfd, newfd);
        if (fd >= 0 && flags & 0x80000 &&
            fcntl(fd, F_SETFD, FD_CLOEXEC)) {
            close(fd);
            fd = -1;
        }
        result = fd < 0 ? -linux_error(errno) : fd;
    }
    return result;
}

static long
sys_stat(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    struct stat st;
    int rc;
    if (nr == 4 || nr == 6) {
        if (procfs_stat((const char *)(uintptr_t)r->rdi, nr == 4, &st, &result)) {
            if (!result) copy_stat((LinuxStat *)(uintptr_t)r->rsi, &st);
            return result;
        }
    } else if (nr == 262 && !(r->r10 & 0x1000) &&
        procfs_stat((const char *)(uintptr_t)r->rsi, !(r->r10 & 0x100), &st, &result)) {
        if (!result) copy_stat((LinuxStat *)(uintptr_t)r->rdx, &st);
        return result;
    }
    if (nr == 5) rc = fstat((int)r->rdi, &st);
    else if (nr == 4) rc = stat((const char *)(uintptr_t)r->rdi, &st);
    else if (nr == 6) rc = lstat((const char *)(uintptr_t)r->rdi, &st);
    else {
        int flags = (int)r->r10;
        int dirfd = (int)r->rdi;
        const char *path = (const char *)(uintptr_t)r->rsi;
        if (dirfd == -100) dirfd = AT_FDCWD;
        if (flags & ~(0x100 | 0x800 | 0x1000)) rc = -1, errno = EINVAL;
        else if (!*path && flags & 0x1000)
            rc = dirfd == AT_FDCWD ? stat(".", &st) : fstat(dirfd, &st);
        else rc = fstatat(dirfd, path, &st,
            flags & 0x100 ? AT_SYMLINK_NOFOLLOW : 0);
    }
    if (rc) result = -linux_error(errno);
    else {
        LinuxStat *out = (LinuxStat *)(uintptr_t)(nr == 262 ?
            r->rdx : r->rsi);
        copy_stat(out, &st);
        result = 0;
    }
    return result;
}

static long
sys_statfs(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    struct statvfs st;
    int rc = nr == 137 ?
        statvfs((const char *)(uintptr_t)r->rdi, &st) :
        fstatvfs((int)r->rdi, &st);
    if (rc) result = -linux_error(errno);
    else {
        copy_statfs((LinuxStatfs *)(uintptr_t)r->rsi, &st);
        result = 0;
    }
    return result;
}

static long
sys_getdents64(GuestRegs *r)
{
    return linux_getdents((int)r->rdi,
        (void *)(uintptr_t)r->rsi, (size_t)r->rdx);
}

static int syscall_port_start(void);

// a new process: the host's fork on the guest's own thread, the child returns 0 here with the parent's registers.
// CLONE_VM with CLONE_VFORK (glibc's vfork and posix_spawn) is a fork too: the child only execs or exits
static long
linux_fork(GuestRegs *r, uintptr_t flags, uintptr_t stack, int *parent_tid, int *child_tid)
{
    const uintptr_t allowed = 0xffUL | 0x100 | 0x4000 | 0x100000 | 0x200000 | 0x1000000;
    pid_t pid;

    if ((flags & ~allowed) || ((flags & 0xff) && (flags & 0xff) != 17) || ((flags & 0x100) && !(flags & 0x4000)))
        return -22;
    pthread_mutex_lock(&sites_lock);
    pthread_mutex_lock(&futex_lock);
    signals_fork_prepare();
    pid = fork();
    if (pid) {
        int error = errno;

        signals_fork_parent();
        pthread_mutex_unlock(&futex_lock);
        pthread_mutex_unlock(&sites_lock);
        if (pid < 0)
            return -linux_error(error);
        if ((flags & 0x100000) && parent_tid)
            *parent_tid = pid;
        return pid;
    }
    // the child: the only thread, its waiters and the syscall server stayed with the parent
    guest_tid = getpid();
    signals_fork_child(guest_tid);
    futex_waiters = NULL;
    pthread_mutex_unlock(&futex_lock);
    pthread_mutex_unlock(&sites_lock);
    guest_robust_head = NULL;
    guest_clear_tid = (flags & 0x200000) ? child_tid : NULL;
    if ((flags & 0x1000000) && child_tid)
        *child_tid = guest_tid;
    if (syscall_port_start() || signal_stack() || set_fs_base(guest_fs_base))
        _exit(126);
    if (stack)
        r->rsp = stack;
    return 0;
}

static long
sys_clone(GuestRegs *r)
{
    uintptr_t flags = r->rdi;

    if (!(flags & 0x100) || (flags & 0x4000))
        return linux_fork(r, flags, r->rsi, (int *)(uintptr_t)r->rdx, (int *)(uintptr_t)r->r10);
    return linux_clone(r);
}

static long
sys_fork(GuestRegs *r)
{
    return linux_fork(r, 17, 0, NULL, NULL);
}

// a wait status as Linux encodes it: the same layout, Linux's signal numbers
static int
linux_wait_status(int status)
{
    if (WIFEXITED(status))
        return (WEXITSTATUS(status) & 0xff) << 8;
    if (WIFSIGNALED(status))
        return signals_linux_number(WTERMSIG(status)) | (WCOREDUMP(status) ? 0x80 : 0);
    if (WIFSTOPPED(status))
        return (signals_linux_number(WSTOPSIG(status)) << 8) | 0x7f;
    return 0xffff;
}

// Linux's rusage: timevals of two longs, where Darwin's microseconds are an int
static void
copy_rusage(void *out, const struct rusage *in)
{
    long *w = out;

    w[0] = in->ru_utime.tv_sec, w[1] = in->ru_utime.tv_usec;
    w[2] = in->ru_stime.tv_sec, w[3] = in->ru_stime.tv_usec;
    w[4] = in->ru_maxrss, w[5] = in->ru_ixrss, w[6] = in->ru_idrss, w[7] = in->ru_isrss;
    w[8] = in->ru_minflt, w[9] = in->ru_majflt, w[10] = in->ru_nswap, w[11] = in->ru_inblock;
    w[12] = in->ru_oublock, w[13] = in->ru_msgsnd, w[14] = in->ru_msgrcv, w[15] = in->ru_nsignals;
    w[16] = in->ru_nvcsw, w[17] = in->ru_nivcsw;
}

static long
sys_wait4(GuestRegs *r)
{
    int linux_options = (int)r->rdx, options = 0, status = 0;
    int *out = (int *)(uintptr_t)r->rsi;
    struct rusage ru;
    pid_t pid;

    // WNOHANG, WUNTRACED and WCONTINUED, __WNOTHREAD, __WALL and __WCLONE mean nothing without thread processes
    if (linux_options & ~(1 | 2 | 8 | 0x20000000 | 0x40000000 | (int)0x80000000))
        return -22;
    options = (linux_options & 1 ? WNOHANG : 0) | (linux_options & 2 ? WUNTRACED : 0) |
        (linux_options & 8 ? WCONTINUED : 0);
    pid = wait4((pid_t)r->rdi, &status, options, &ru);
    if (pid < 0)
        return -linux_error(errno);
    if (pid && out)
        *out = linux_wait_status(status);
    if (pid && r->r10)
        copy_rusage((void *)(uintptr_t)r->r10, &ru);
    return pid;
}

static long
sys_waitid(GuestRegs *r)
{
    int linux_options = (int)r->r10, options;
    int32_t *info = (int32_t *)(uintptr_t)r->rdx;
    siginfo_t si;

    // P_ALL, P_PID, P_PGID have the same numbers, a pidfd (3) is not a thing here
    if (r->rdi > 2 || (linux_options & ~(1 | 2 | 4 | 8 | 0x1000000)) || !(linux_options & (2 | 4 | 8)))
        return -22;
    options = (linux_options & 1 ? WNOHANG : 0) | (linux_options & 2 ? WSTOPPED : 0) |
        (linux_options & 4 ? WEXITED : 0) | (linux_options & 8 ? WCONTINUED : 0) |
        (linux_options & 0x1000000 ? WNOWAIT : 0);
    memset(&si, 0, sizeof(si));
    if (waitid((idtype_t)r->rdi, (id_t)r->rsi, &si, options))
        return -linux_error(errno);
    if (info) {
        // signo, errno, code, then the child's pid, uid and status, nothing waited for (WNOHANG) leaves it zero
        memset(info, 0, 128);
        if (si.si_pid) {
            info[0] = 17, info[2] = si.si_code, info[4] = si.si_pid, info[5] = (int32_t)si.si_uid;
            info[6] = si.si_code == CLD_EXITED ? si.si_status : signals_linux_number(si.si_status);
        }
    }
    if (r->r8)
        memset((void *)(uintptr_t)r->r8, 0, 144);
    return 0;
}

// a Linux program goes through elftrans again with the guest's argv[0], mask and ignored signals, the rest runs
// as it is, and the guest's thread pointer and syscall port come back when an exec fails
static long
linux_execve(int dirfd, const char *path, char *const *argv, char *const *envp)
{
    char full[PATH_MAX], magic[4];
    int fd, elf, argc = 0;
    uint64_t fs = guest_fs_base;
    mach_port_t self = mach_task_self();

    if (!path)
        return -14;
    if (path[0] != '/' && dirfd != AT_FDCWD) {
        char dir[PATH_MAX];

        if (fcntl(dirfd, F_GETPATH, dir) < 0 || snprintf(full, sizeof(full), "%s/%s", dir, path) >= (int)sizeof(full))
            return -9;
        path = full;
    }
    if ((fd = open(path, O_RDONLY | O_CLOEXEC)) < 0)
        return -linux_error(errno);
    elf = read(fd, magic, 4) == 4 && !memcmp(magic, "\177ELF", 4);
    close(fd);
    while (argv && argv[argc])
        argc++;

    char **args = NULL;
    if (elf) {
        char mask[24], ignored[24];

        if (!(args = calloc((size_t)argc + 10, sizeof(*args))))
            return -12;
        snprintf(mask, sizeof(mask), "%llx", (unsigned long long)signals_mask());
        snprintf(ignored, sizeof(ignored), "%llx", (unsigned long long)signals_ignored());
        int n = 0;
        args[n++] = elftrans_self, args[n++] = "--exec";
        args[n++] = "--argv0", args[n++] = argc ? argv[0] : (char *)path;
        args[n++] = "--sigmask", args[n++] = strdup(mask), args[n++] = "--sigign", args[n++] = strdup(ignored);
        args[n++] = (char *)path;
        for (int i = 1; i < argc; i++)
            args[n++] = argv[i];
        args[n] = NULL;
    }
    set_fs_base(0);
    task_set_exception_ports(self, EXC_MASK_SYSCALL, MACH_PORT_NULL, EXCEPTION_DEFAULT, THREAD_STATE_NONE);
    execve(elf ? elftrans_self : path, elf ? args : argv, envp);

    int error = errno;
    task_set_exception_ports(self, EXC_MASK_SYSCALL, syscall_port, EXCEPTION_STATE | MACH_EXCEPTION_CODES,
        x86_THREAD_STATE64);
    set_fs_base(fs);
    free(args);
    return -linux_error(error);
}

static long
sys_execve(GuestRegs *r)
{
    long nr = (long)r->rax;

    if (nr == 59)
        return linux_execve(AT_FDCWD, (const char *)(uintptr_t)r->rdi, (char *const *)(uintptr_t)r->rsi,
            (char *const *)(uintptr_t)r->rdx);
    // execveat with an empty path and AT_EMPTY_PATH means the fd itself, which is unsupported
    if (r->r8 & 0x1000)
        return -38;
    return linux_execve((int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi, (const char *)(uintptr_t)r->rsi,
        (char *const *)(uintptr_t)r->rdx, (char *const *)(uintptr_t)r->r10);
}

static long
sys_futex(GuestRegs *r)
{
    return linux_futex((int *)(uintptr_t)r->rdi,
        (int)r->rsi, (int)r->rdx,
        (const struct timespec *)(uintptr_t)r->r10,
        (uint32_t)r->r9);
}



static long
sys_set_robust_list(GuestRegs *r)
{
    long result = -38;
    if (!r->rdi || r->rsi != sizeof(LinuxRobustHead))
        result = -22;
    else {
        guest_robust_head = (LinuxRobustHead *)(uintptr_t)r->rdi;
        result = 0;
    }
    return result;
}

static long
sys_get_robust_list(GuestRegs *r)
{
    long result = -38;
    int pid = (int)r->rdi;
    if (pid && pid != guest_tid) result = -3;
    else if (!r->rsi || !r->rdx) result = -14;
    else {
        *(uintptr_t *)(uintptr_t)r->rsi =
            (uintptr_t)guest_robust_head;
        *(size_t *)(uintptr_t)r->rdx = sizeof(LinuxRobustHead);
        result = 0;
    }
    return result;
}

static long
sys_prlimit64(GuestRegs *r)
{
    return linux_prlimit64((int)r->rdi, (int)r->rsi,
        (const LinuxRlimit *)(uintptr_t)r->rdx,
        (LinuxRlimit *)(uintptr_t)r->r10);
}

static long
sys_rseq(GuestRegs *r)
{
    (void)r;
    return -38;
}

static long
sys_socket(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    int linux_type = (int)r->rsi;
    int type = socket_type(linux_type);
    if (nr == 41)
        return net_socket((int)r->rdi, linux_type, (int)r->rdx);
    if ((int)r->rdi != 1 || type < 0 || r->rdx)
        result = -97;
    else {
        int pair[2];
        if (socketpair(AF_UNIX, type, 0, pair))
            result = -linux_error(errno);
        else if (socket_flags(pair[0], linux_type) < 0) {
            close(pair[1]);
            result = -linux_error(errno);
        } else if (socket_flags(pair[1], linux_type) < 0) {
            close(pair[0]);
            result = -linux_error(errno);
        } else {
            int *output = (int *)(uintptr_t)r->r10;
            output[0] = pair[0];
            output[1] = pair[1];
            result = 0;
        }
    }
    return result;
}

static long
sys_connect(GuestRegs *r)
{
    // connect (42) and bind (49) take the same arguments
    return net_bind_connect((int)r->rdi, (const void *)(uintptr_t)r->rsi, (uint32_t)r->rdx, (long)r->rax == 42);
}

static long
sys_listen(GuestRegs *r)
{
    return listen((int)r->rdi, (int)r->rsi) ? -linux_error(errno) : 0;
}

static long
sys_accept(GuestRegs *r)
{
    return net_accept((int)r->rdi, (void *)(uintptr_t)r->rsi, (uint32_t *)(uintptr_t)r->rdx,
        (long)r->rax == 288 ? (int)r->r10 : 0);
}

static long
sys_sockname(GuestRegs *r)
{
    return net_name((int)r->rdi, (void *)(uintptr_t)r->rsi, (uint32_t *)(uintptr_t)r->rdx, (long)r->rax == 52);
}

static long
sys_sendto(GuestRegs *r)
{
    return net_sendto((int)r->rdi, (const void *)(uintptr_t)r->rsi, (size_t)r->rdx, (int)r->r10,
        (const void *)(uintptr_t)r->r8, (uint32_t)r->r9);
}

static long
sys_recvfrom(GuestRegs *r)
{
    return net_recvfrom((int)r->rdi, (void *)(uintptr_t)r->rsi, (size_t)r->rdx, (int)r->r10,
        (void *)(uintptr_t)r->r8, (uint32_t *)(uintptr_t)r->r9);
}

static long
sys_shutdown(GuestRegs *r)
{
    return shutdown((int)r->rdi, (int)r->rsi) ? -linux_error(errno) : 0;
}

// a guest timespec as nanoseconds, -1 for none and -22 for one out of range
static int64_t
timespec_ns(const struct timespec *ts)
{
    if (!ts)
        return -1;
    if (ts->tv_sec < 0 || ts->tv_nsec < 0 || ts->tv_nsec >= 1000000000)
        return -22;
    return (int64_t)ts->tv_sec * 1000000000 + ts->tv_nsec;
}

static long
sys_epoll(GuestRegs *r)
{
    long nr = (long)r->rax;
    int64_t timeout;

    switch (nr) {
    case 213:
        return (int)r->rdi <= 0 ? -22 : events_epoll_create(0);
    case 291:
        return events_epoll_create((int)r->rdi);
    case 233:
        return events_epoll_ctl((int)r->rdi, (int)r->rsi, (int)r->rdx, (const void *)(uintptr_t)r->r10);
    case 441:
        // epoll_pwait2: a timespec, and a signal mask the wait does not apply
        timeout = timespec_ns((const struct timespec *)(uintptr_t)r->r10);
        if (timeout == -22) return -22;
        return events_epoll_wait((int)r->rdi, (void *)(uintptr_t)r->rsi, (int)r->rdx, timeout);
    default:
        // epoll_wait and epoll_pwait: milliseconds, -1 for none
        timeout = (int)r->r10 < 0 ? -1 : (int64_t)(int)r->r10 * 1000000;
        return events_epoll_wait((int)r->rdi, (void *)(uintptr_t)r->rsi, (int)r->rdx, timeout);
    }
}

static long
sys_eventfd(GuestRegs *r)
{
    return events_eventfd((unsigned)r->rdi, (long)r->rax == 290 ? (int)r->rsi : 0);
}

static long
sys_timerfd(GuestRegs *r)
{
    long nr = (long)r->rax;

    if (nr == 283)
        return events_timerfd_create((int)r->rdi, (int)r->rsi);
    if (nr == 286)
        return events_timerfd_settime((int)r->rdi, (int)r->rsi, (const void *)(uintptr_t)r->rdx,
            (void *)(uintptr_t)r->r10);
    return events_timerfd_gettime((int)r->rdi, (void *)(uintptr_t)r->rsi);
}

static long
sys_select(GuestRegs *r)
{
    int64_t timeout;

    if ((long)r->rax == 23) {
        struct { long sec, usec; } *tv = (void *)(uintptr_t)r->r8;

        if (tv && (tv->sec < 0 || tv->usec < 0 || tv->usec >= 1000000))
            return -22;
        timeout = tv ? (int64_t)tv->sec * 1000000000 + tv->usec * 1000 : -1;
    } else {
        // pselect6: a timespec, then the signal mask the wait does not apply
        timeout = timespec_ns((const struct timespec *)(uintptr_t)r->r8);
        if (timeout == -22) return -22;
    }
    return events_select((int)r->rdi, (uint64_t *)(uintptr_t)r->rsi, (uint64_t *)(uintptr_t)r->rdx,
        (uint64_t *)(uintptr_t)r->r10, timeout);
}

static long
sys_sendmsg(GuestRegs *r)
{
    return linux_sendmsg((int)r->rdi,
        (const LinuxMsgHdr *)(uintptr_t)r->rsi, (int)r->rdx);
}

static long
sys_recvmsg(GuestRegs *r)
{
    return linux_recvmsg((int)r->rdi,
        (LinuxMsgHdr *)(uintptr_t)r->rsi, (int)r->rdx);
}

// Linux's struct mmsghdr: a message header, then the byte count, padded to 64
typedef struct {
    LinuxMsgHdr header;
    uint32_t length, padding;
} LinuxMmsgHdr;

_Static_assert(sizeof(LinuxMmsgHdr) == 64, "Linux struct mmsghdr");

// sendmmsg and recvmmsg one message at a time: what went through before an error counts, as Linux reports it
static long
sys_mmsg(GuestRegs *r)
{
    int receiving = (long)r->rax == 299, flags = (int)r->r10;
    LinuxMmsgHdr *v = (LinuxMmsgHdr *)(uintptr_t)r->rsi;
    unsigned n = (unsigned)r->rdx, done = 0;

    if (n > 1024)
        n = 1024;
    for (; done < n; done++) {
        long got = receiving ? linux_recvmsg((int)r->rdi, &v[done].header, flags & ~0x10000) :
                               linux_sendmsg((int)r->rdi, &v[done].header, flags);

        if (got < 0)
            return done ? (long)done : got;
        v[done].length = (uint32_t)got;
        // MSG_WAITFORONE: blocking only for the first message
        if (receiving && (flags & 0x10000))
            flags |= 0x40;
    }
    return done;
}

static long
sys_poll(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    nfds_t count = (nfds_t)r->rsi;
    if (count > 1024) result = -22;
    else {
        int timeout = (int)r->rdx;
        int valid = 1;
        if (nr == 271) {
            const struct timespec *limit =
                (const struct timespec *)(uintptr_t)r->rdx;
            if (r->r10) result = -38, valid = 0;
            else if (!limit) timeout = -1;
            else if (limit->tv_sec < 0 || limit->tv_nsec < 0 ||
                limit->tv_nsec >= 1000000000)
                result = -22, valid = 0;
            else if (limit->tv_sec >= INT_MAX / 1000)
                timeout = INT_MAX;
            else timeout = (int)(limit->tv_sec * 1000 +
                (limit->tv_nsec + 999999) / 1000000);
        }
        if (valid) {
            int ready = poll((struct pollfd *)(uintptr_t)r->rdi,
                count, timeout);
            result = ready < 0 ? -linux_error(errno) : ready;
        }
    }
    return result;
}

static long
sys_fcntl(GuestRegs *r)
{
    return linux_fcntl((int)r->rdi, (int)r->rsi,
        (long)r->rdx);
}

static long
sys_ftruncate(GuestRegs *r)
{
    return ftruncate((int)r->rdi, (off_t)r->rsi) ?
        -linux_error(errno) : 0;
}

static long
sys_fallocate(GuestRegs *r)
{
    (void)r;
    return -22;
}

static long
sys_memfd_create(GuestRegs *r)
{
    return linux_memfd_create((const char *)(uintptr_t)r->rdi,
        (unsigned)r->rsi);
}

static long
sys_membarrier(GuestRegs *r)
{
    long result = -38;
    if (!r->rdi && !r->rsi) result = 0;
    else result = -22;
    return result;
}

static long
sys_brk(GuestRegs *r)
{
    // the heap starts where the program ends when that range is free, as a static program's malloc expects
    if (!brk_reserved) {
        int fixed = in_guest_space(brk_base, brk_base + BRK_SIZE);
        void *region = mmap((void *)brk_base, BRK_SIZE, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANON | (fixed ? MAP_FIXED : 0), -1, 0);
        if (region == MAP_FAILED)
            return brk_current;
        brk_base = brk_current = (uintptr_t)region;
        brk_reserved = 1;
    }
    uintptr_t requested = (uintptr_t)r->rdi;
    if (requested >= brk_base && requested <= brk_base + BRK_SIZE)
        brk_current = requested;
    return brk_current;
}

static long
sys_writev(GuestRegs *r)
{
    long result = -38;
    ssize_t n = writev((int)r->rdi,
        (const struct iovec *)(uintptr_t)r->rsi,
        (int)r->rdx);
    result = n < 0 ? -linux_error(errno) : n;
    return result;
}

static long
sys_mmap(GuestRegs *r)
{
    long result = -38;
    unsigned long linux_flags = (unsigned long)r->r10;
    int flags = 0;
    if (!(linux_flags & ~(0x20833UL | LINUX_MAP_NORESERVE)) &&
        ((linux_flags & 3) == 1 || (linux_flags & 3) == 2)) {
        flags |= (linux_flags & 3) == 1 ? MAP_SHARED : MAP_PRIVATE;
        if (linux_flags & 0x10) flags |= MAP_FIXED;
        if (linux_flags & 0x20) flags |= MAP_ANON;
        pthread_mutex_lock(&sites_lock);
        if ((flags & MAP_FIXED) &&
            ((size_t)r->rsi > UINTPTR_MAX - (uintptr_t)r->rdi ||
            publish_sites((uintptr_t)r->rdi,
                (uintptr_t)r->rdi + (size_t)r->rsi, NULL, 0))) {
            pthread_mutex_unlock(&sites_lock);
            result = -12;
            goto mmap_done;
        }
        // code mapped from a file comes from its converted copy, at the same offsets, and its sites are
        // registered, with no copy the EXC_SYSCALL fallback serves its syscalls
        int fd = (int)r->r8, converted = -1;
        if (!(flags & MAP_ANON) && (r->rdx & PROT_EXEC) && (converted = aot_open(fd)) >= 0)
            fd = converted;
        void *mapped = mmap((void *)(uintptr_t)r->rdi, (size_t)r->rsi, (int)r->rdx, flags, fd, (off_t)r->r9);
        result = mapped == MAP_FAILED ? -linux_error(errno) : (long)(uintptr_t)mapped;
        if (mapped != MAP_FAILED && converted >= 0)
            register_mapped_aot(converted, (off_t)r->r9, (uintptr_t)mapped, (size_t)r->rsi);
        if (converted >= 0)
            close(converted);
        pthread_mutex_unlock(&sites_lock);
    mmap_done:;
    } else {
        char line[160];
        int count = snprintf(line, sizeof(line),
            "elftrans: mmap flags=%#lx prot=%#lx fd=%ld off=%#lx\n",
            linux_flags, (unsigned long)r->rdx, (long)r->r8,
            (unsigned long)r->r9);
        if (count > 0 && count < (int)sizeof(line))
            write(2, line, (size_t)count);
        result = -22;
    }
    return result;
}

static long
sys_mprotect(GuestRegs *r)
{
    return mprotect((void *)(uintptr_t)r->rdi,
        (size_t)r->rsi, (int)r->rdx) ?
            -linux_error(errno) : 0;
}

static long
sys_munmap(GuestRegs *r)
{
    long result = -38;
    pthread_mutex_lock(&sites_lock);
    uintptr_t unmap_lo = (uintptr_t)r->rdi;
    size_t unmap_size = (size_t)r->rsi;
    if (unmap_size && unmap_lo <= UINTPTR_MAX - unmap_size &&
        !publish_sites(unmap_lo, unmap_lo + unmap_size, NULL, 0))
        result = munmap((void *)unmap_lo, unmap_size) ?
            -linux_error(errno) : 0;
    else
        result = -22;
    pthread_mutex_unlock(&sites_lock);
    return result;
}

static long
sys_getpid(GuestRegs *r)
{
    (void)r;
    return getpid();
}


static long
sys_gettid(GuestRegs *r)
{
    (void)r;
    long result = -38;
    if (!guest_tid) guest_tid = getpid();
    result = guest_tid;
    return result;
}

static long
sys_set_tid_address(GuestRegs *r)
{
    long result = -38;
    guest_clear_tid = (int *)(uintptr_t)r->rdi;
    if (!guest_tid) guest_tid = getpid();
    result = guest_tid;
    return result;
}

static long
sys_exit(GuestRegs *r)
{
    long nr = (long)r->rax;
    long result = -38;
    if (guest_tid >= 1000 && nr == 60) {
        release_robust_list();
        if (guest_clear_tid) {
            pthread_mutex_lock(&futex_lock);
            *guest_clear_tid = 0;
            wake_futex_locked(guest_clear_tid, 1, UINT32_MAX);
            pthread_mutex_unlock(&futex_lock);
        }
        signals_thread_exit();
        pthread_exit(NULL);
    }
    _exit((int)r->rdi);
    return result;
}

static long
sys_arch_prctl(GuestRegs *r)
{
    long result = -38;
    if (r->rdi == 0x1002) {
        result = set_fs_base(r->rsi) ? -22 : 0;
    }
    return result;
}

// Linux clock ids to Darwin ones: the coarse clocks are the precise ones, boot time is the monotonic clock
static int
native_clock(long clock, clockid_t *out)
{
    switch (clock) {
    case 0: case 5: *out = CLOCK_REALTIME; return 0;
    case 1: case 6: case 7: *out = CLOCK_MONOTONIC; return 0;
    case 2: *out = CLOCK_PROCESS_CPUTIME_ID; return 0;
    case 3: *out = CLOCK_THREAD_CPUTIME_ID; return 0;
    case 4: *out = CLOCK_MONOTONIC_RAW; return 0;
    default: return -1;
    }
}

static long
sys_clock_gettime(GuestRegs *r)
{
    clockid_t clock;

    if (native_clock((long)r->rdi, &clock)) return -22;
    return clock_gettime(clock, (struct timespec *)(uintptr_t)r->rsi) ? -linux_error(errno) : 0;
}

static long
sys_clock_getres(GuestRegs *r)
{
    clockid_t clock;

    if (native_clock((long)r->rdi, &clock)) return -22;
    return clock_getres(clock, (struct timespec *)(uintptr_t)r->rsi) ? -linux_error(errno) : 0;
}

static long
sys_time(GuestRegs *r)
{
    time_t now = time(NULL);

    if (r->rdi) *(int64_t *)(uintptr_t)r->rdi = now;
    return now;
}

// SEEK_DATA and SEEK_HOLE are 3 and 4 on Linux, the other way round on Darwin
static long
sys_lseek(GuestRegs *r)
{
    int whence = (int)r->rdx;

    if (whence == 3) whence = SEEK_DATA;
    else if (whence == 4) whence = SEEK_HOLE;
    else if (whence > 2) return -22;
    off_t at = lseek((int)r->rdi, (off_t)r->rsi, whence);
    return at < 0 ? -linux_error(errno) : at;
}

// the syscall returns the length with its NUL, the C library's getcwd a pointer
static long
sys_getcwd(GuestRegs *r)
{
    char *buffer = (char *)(uintptr_t)r->rdi;

    if (!getcwd(buffer, (size_t)r->rsi)) return -linux_error(errno);
    return (long)strlen(buffer) + 1;
}

typedef struct {
    int64_t sec;
    uint32_t nsec;
    int32_t reserved;
} LinuxStatxTime;

typedef struct {
    uint32_t mask, blksize;
    uint64_t attributes;
    uint32_t nlink, uid, gid;
    uint16_t mode, spare0;
    uint64_t ino, size, blocks, attributes_mask;
    LinuxStatxTime atime, btime, ctime, mtime;
    uint32_t rdev_major, rdev_minor, dev_major, dev_minor;
    uint64_t spare[14];
} LinuxStatx;

_Static_assert(sizeof(LinuxStatx) == 256, "struct statx");

static long
sys_statx(GuestRegs *r)
{
    int dirfd = (int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi;
    const char *path = (const char *)(uintptr_t)r->rsi;
    int flags = (int)r->rdx;
    LinuxStatx *out = (LinuxStatx *)(uintptr_t)r->r8;
    struct stat st;
    int rc;

    // AT_EMPTY_PATH stats dirfd itself, the sync flags have nothing to ask of a local filesystem
    if (flags & ~(0x100 | 0x800 | 0x1000 | 0x6000)) return -22;
    long proc_result;
    if (!(flags & 0x1000) && procfs_stat(path, !(flags & 0x100), &st, &proc_result)) {
        if (proc_result) return proc_result;
        rc = 0;
    } else if (!*path && (flags & 0x1000))
        rc = dirfd == AT_FDCWD ? stat(".", &st) : fstat(dirfd, &st);
    else
        rc = fstatat(dirfd, path, &st, flags & 0x100 ? AT_SYMLINK_NOFOLLOW : 0);
    if (rc) return -linux_error(errno);
    memset(out, 0, sizeof(*out));
    out->mask = 0x7ff | 0x800;
    out->blksize = (uint32_t)st.st_blksize, out->nlink = st.st_nlink;
    out->uid = st.st_uid, out->gid = st.st_gid, out->mode = st.st_mode;
    out->ino = st.st_ino, out->size = (uint64_t)st.st_size, out->blocks = (uint64_t)st.st_blocks;
    out->atime = (LinuxStatxTime){ st.st_atimespec.tv_sec, (uint32_t)st.st_atimespec.tv_nsec, 0 };
    out->btime = (LinuxStatxTime){ st.st_birthtimespec.tv_sec, (uint32_t)st.st_birthtimespec.tv_nsec, 0 };
    out->ctime = (LinuxStatxTime){ st.st_ctimespec.tv_sec, (uint32_t)st.st_ctimespec.tv_nsec, 0 };
    out->mtime = (LinuxStatxTime){ st.st_mtimespec.tv_sec, (uint32_t)st.st_mtimespec.tv_nsec, 0 };
    out->rdev_major = major(st.st_rdev), out->rdev_minor = minor(st.st_rdev);
    out->dev_major = major(st.st_dev), out->dev_minor = minor(st.st_dev);
    return 0;
}

// advice the kernel may ignore: Darwin's madvise for the hints it shares, success for the rest
static long
sys_madvise(GuestRegs *r)
{
    int advice = (int)r->rdx;

    if (advice == 4)
        madvise((void *)(uintptr_t)r->rdi, (size_t)r->rsi, MADV_DONTNEED);
    else if (advice == 1 || advice == 2 || advice == 3)
        madvise((void *)(uintptr_t)r->rdi, (size_t)r->rsi, advice);
    return 0;
}

static long
sys_fadvise64(GuestRegs *r)
{
    (void)r;
    return 0;
}

// every CPU, a one-word mask as the kernel's
static long
sys_sched_getaffinity(GuestRegs *r)
{
    size_t size = (size_t)r->rsi;
    uint64_t *mask = (uint64_t *)(uintptr_t)r->rdx;
    long cpus = sysconf(_SC_NPROCESSORS_ONLN);

    if (size < sizeof(uint64_t)) return -22;
    memset(mask, 0, size);
    *mask = cpus >= 64 ? ~0ULL : (1ULL << (cpus > 0 ? cpus : 1)) - 1;
    return sizeof(uint64_t);
}

static long
sys_sched_yield(GuestRegs *r)
{
    (void)r;
    sched_yield();
    return 0;
}

// thread names, and the process flags a program sets for itself and reads back
static long
sys_prctl(GuestRegs *r)
{
    switch ((int)r->rdi) {
    case 15:
        pthread_setname_np((const char *)(uintptr_t)r->rsi);
        return 0;
    case 16:
        return pthread_getname_np(pthread_self(), (char *)(uintptr_t)r->rsi, 16) ? -22 : 0;
    case 1: case 4: case 36: case 38:
        return 0;
    case 3:
        return 1;
    default:
        return -22;
    }
}

// socket options: Linux SOL_SOCKET names to Darwin's, SO_PEERCRED from the local peer's credentials

static long
sys_getsockopt(GuestRegs *r)
{
    int fd = (int)r->rdi, level = (int)r->rsi, option = (int)r->rdx;
    void *value = (void *)(uintptr_t)r->r10;
    uint32_t *length = (uint32_t *)(uintptr_t)r->r8;

    if (level == 1 && option == 17) {
        struct xucred cred;
        pid_t pid;
        socklen_t size = sizeof(cred), pid_size = sizeof(pid);
        if (*length < 12) return -22;
        if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERCRED, &cred, &size) ||
            getsockopt(fd, SOL_LOCAL, LOCAL_PEERPID, &pid, &pid_size))
            return -linux_error(errno);
        int32_t *out = value;
        out[0] = pid, out[1] = (int32_t)cred.cr_uid, out[2] = cred.cr_ngroups ? (int32_t)cred.cr_groups[0] : 0;
        *length = 12;
        return 0;
    }
    int native_level, native;
    if (net_option_ignored(level, option)) {
        if (*length >= 4) memset(value, 0, 4), *length = 4;
        return 0;
    }
    if (!net_option(level, option, &native_level, &native)) return -92;
    socklen_t size = *length;
    if (getsockopt(fd, native_level, native, value, &size)) return -linux_error(errno);
    // SO_ERROR holds an errno, which Linux numbers its own way
    if (level == 1 && option == 4 && size >= 4 && *(int *)value)
        *(int *)value = (int)linux_error(*(int *)value);
    *length = size;
    return 0;
}

static long
sys_setsockopt(GuestRegs *r)
{
    int fd = (int)r->rdi, level = (int)r->rsi, option = (int)r->rdx;

    int native_level, native;
    // SO_PASSCRED and the others Darwin has no counterpart for are accepted
    if (net_option_ignored(level, option)) return 0;
    if (!net_option(level, option, &native_level, &native)) return -92;
    return setsockopt(fd, native_level, native, (const void *)(uintptr_t)r->r10, (socklen_t)r->r8) ?
        -linux_error(errno) : 0;
}

// the terminal and descriptor ioctls programs use to probe a fd, ENOTTY for the rest as Linux answers
static long
sys_ioctl(GuestRegs *r)
{
    int fd = (int)r->rdi;
    unsigned long request = (unsigned long)r->rsi;
    void *argument = (void *)(uintptr_t)r->rdx;

    switch (request) {
    case 0x5401:
        // TCGETS: the flags differ between the two, a tty answers with a zeroed termios
        if (!isatty(fd)) return -25;
        memset(argument, 0, 60);
        return 0;
    case 0x5402: case 0x5403: case 0x5404:
        return isatty(fd) ? 0 : -25;
    case 0x5413:
        return ioctl(fd, TIOCGWINSZ, argument) ? -linux_error(errno) : 0;
    case 0x5414:
        return ioctl(fd, TIOCSWINSZ, argument) ? -linux_error(errno) : 0;
    case 0x541b:
        return ioctl(fd, FIONREAD, argument) ? -linux_error(errno) : 0;
    case 0x5421: {
        int flags = fcntl(fd, F_GETFL);
        if (flags < 0) return -linux_error(errno);
        flags = *(int *)argument ? flags | O_NONBLOCK : flags & ~O_NONBLOCK;
        return fcntl(fd, F_SETFL, flags) ? -linux_error(errno) : 0;
    }
    case 0x5450: case 0x5451:
        return fcntl(fd, F_SETFD, request == 0x5451 ? FD_CLOEXEC : 0) ? -linux_error(errno) : 0;
    default:
        return -25;
    }
}

static long
sys_sysinfo(GuestRegs *r)
{
    return procfs_sysinfo((void *)(uintptr_t)r->rdi);
}

// what a Linux program expects uname to say about the system it runs on
static long
sys_uname(GuestRegs *r)
{
    char (*u)[65] = (char (*)[65])(uintptr_t)r->rdi;

    memset(u, 0, 6 * 65);
    strcpy(u[0], "Linux");
    if (gethostname(u[1], 65)) strcpy(u[1], "localhost");
    strcpy(u[2], "6.6.0-puredarwin");
    strcpy(u[3], "#1 SMP PureDarwin elftrans");
    strcpy(u[4], "x86_64");
    strcpy(u[5], "(none)");
    return 0;
}

// real, effective and saved IDs, the saved one is the effective one
static long
sys_getresid(GuestRegs *r)
{
    uint32_t *real = (uint32_t *)(uintptr_t)r->rdi, *effective = (uint32_t *)(uintptr_t)r->rsi;
    uint32_t *saved = (uint32_t *)(uintptr_t)r->rdx;

    if ((long)r->rax == 118)
        *real = getuid(), *effective = *saved = geteuid();
    else
        *real = getgid(), *effective = *saved = getegid();
    return 0;
}

static long
sys_getppid(GuestRegs *r)
{
    (void)r;
    return getppid();
}

static long
sys_umask(GuestRegs *r)
{
    return umask((mode_t)r->rdi);
}

static long
sys_chdir(GuestRegs *r)
{
    long nr = (long)r->rax;

    if (nr == 80) return chdir((const char *)(uintptr_t)r->rdi) ? -linux_error(errno) : 0;
    return fchdir((int)r->rdi) ? -linux_error(errno) : 0;
}

static long
sys_chmod(GuestRegs *r)
{
    long nr = (long)r->rax;

    if (nr == 90) return chmod((const char *)(uintptr_t)r->rdi, (mode_t)r->rsi) ? -linux_error(errno) : 0;
    if (nr == 91) return fchmod((int)r->rdi, (mode_t)r->rsi) ? -linux_error(errno) : 0;
    // fchmodat
    int dirfd = (int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi;
    return fchmodat(dirfd, (const char *)(uintptr_t)r->rsi, (mode_t)r->rdx, 0) ? -linux_error(errno) : 0;
}

static long
sys_readv(GuestRegs *r)
{
    ssize_t n = readv((int)r->rdi, (const struct iovec *)(uintptr_t)r->rsi, (int)r->rdx);
    return n < 0 ? -linux_error(errno) : n;
}

static long
sys_pwrite64(GuestRegs *r)
{
    ssize_t n = pwrite((int)r->rdi, (const void *)(uintptr_t)r->rsi, (size_t)r->rdx, (off_t)r->r10);
    return n < 0 ? -linux_error(errno) : n;
}

static long
sys_truncate(GuestRegs *r)
{
    return truncate((const char *)(uintptr_t)r->rdi, (off_t)r->rsi) ? -linux_error(errno) : 0;
}

static long
sys_rmdir(GuestRegs *r)
{
    return rmdir((const char *)(uintptr_t)r->rdi) ? -linux_error(errno) : 0;
}

static long
sys_unlinkat(GuestRegs *r)
{
    int dirfd = (int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi, flags = (int)r->rdx;

    if (flags & ~0x200) return -22;
    return unlinkat(dirfd, (const char *)(uintptr_t)r->rsi, flags ? AT_REMOVEDIR : 0) ? -linux_error(errno) : 0;
}

// UTIME_NOW and UTIME_OMIT are (1 << 30) - 1 and - 2 on Linux, -1 and -2 on Darwin
static long
sys_utimensat(GuestRegs *r)
{
    int dirfd = (int)r->rdi == -100 ? AT_FDCWD : (int)r->rdi, flags = (int)r->r10;
    const char *path = (const char *)(uintptr_t)r->rsi;
    const struct timespec *guest = (const struct timespec *)(uintptr_t)r->rdx;
    struct timespec times[2];

    if (flags & ~(0x100 | 0x1000)) return -22;
    for (int i = 0; guest && i < 2; ++i) {
        times[i] = guest[i];
        if (times[i].tv_nsec == (1L << 30) - 1) times[i].tv_nsec = UTIME_NOW;
        else if (times[i].tv_nsec == (1L << 30) - 2) times[i].tv_nsec = UTIME_OMIT;
    }
    // no path, or AT_EMPTY_PATH: the times of dirfd itself
    if (!path || (!*path && (flags & 0x1000)))
        return futimens(dirfd, guest ? times : NULL) ? -linux_error(errno) : 0;
    return utimensat(dirfd, path, guest ? times : NULL, flags & 0x100 ? AT_SYMLINK_NOFOLLOW : 0) ?
        -linux_error(errno) : 0;
}

// nanosleep and clock_nanosleep: an interrupted sleep reports what is left, as Linux does
static long
sys_nanosleep(GuestRegs *r)
{
    long nr = (long)r->rax;
    const struct timespec *request = (const struct timespec *)(uintptr_t)(nr == 35 ? r->rdi : r->rdx);
    struct timespec *remain = (struct timespec *)(uintptr_t)(nr == 35 ? r->rsi : r->r10);

    if (nr == 230 && (r->rsi & 1)) {
        struct timespec now, left;
        clockid_t clock;
        if (native_clock((long)r->rdi, &clock)) return -22;
        clock_gettime(clock, &now);
        left.tv_sec = request->tv_sec - now.tv_sec, left.tv_nsec = request->tv_nsec - now.tv_nsec;
        if (left.tv_nsec < 0) --left.tv_sec, left.tv_nsec += 1000000000;
        if (left.tv_sec < 0) return 0;
        return nanosleep(&left, NULL) ? -linux_error(errno) : 0;
    }
    if (nanosleep(request, remain))
        return -linux_error(errno);
    return 0;
}

#define NSYSCALLS 512
static long (*const syscalls[NSYSCALLS])(GuestRegs *) = {
    [13] = sys_rt_sigaction,
    [14] = sys_rt_sigprocmask,
    [15] = sys_rt_sigreturn,
    [62] = sys_kill,
    [131] = sys_sigaltstack,
    [200] = sys_tkill,
    [35] = sys_nanosleep,
    [8] = sys_lseek,
    [16] = sys_ioctl,
    [18] = sys_pwrite64,
    [19] = sys_readv,
    [24] = sys_sched_yield,
    [28] = sys_madvise,
    [54] = sys_setsockopt,
    [55] = sys_getsockopt,
    [63] = sys_uname,
    [99] = sys_sysinfo,
    [76] = sys_truncate,
    [79] = sys_getcwd,
    [80] = sys_chdir,
    [81] = sys_chdir,
    [84] = sys_rmdir,
    [90] = sys_chmod,
    [91] = sys_chmod,
    [95] = sys_umask,
    [110] = sys_getppid,
    [118] = sys_getresid,
    [120] = sys_getresid,
    [157] = sys_prctl,
    [201] = sys_time,
    [204] = sys_sched_getaffinity,
    [221] = sys_fadvise64,
    [229] = sys_clock_getres,
    [263] = sys_unlinkat,
    [268] = sys_chmod,
    [280] = sys_utimensat,
    [332] = sys_statx,
    [230] = sys_nanosleep,
    [234] = sys_tgkill,
    [0] = sys_read,
    [1] = sys_write,
    [2] = sys_open,
    [3] = sys_close,
    [4] = sys_stat,
    [5] = sys_stat,
    [6] = sys_stat,
    [7] = sys_poll,
    [9] = sys_mmap,
    [10] = sys_mprotect,
    [11] = sys_munmap,
    [12] = sys_brk,
    [17] = sys_pread64,
    [20] = sys_writev,
    [21] = sys_access,
    [22] = sys_pipe,
    [32] = sys_dup,
    [33] = sys_dup,
    [39] = sys_getpid,
    [41] = sys_socket,
    [42] = sys_connect,
    [299] = sys_mmsg,
    [307] = sys_mmsg,
    [23] = sys_select,
    [43] = sys_accept,
    [44] = sys_sendto,
    [45] = sys_recvfrom,
    [48] = sys_shutdown,
    [49] = sys_connect,
    [50] = sys_listen,
    [51] = sys_sockname,
    [52] = sys_sockname,
    [213] = sys_epoll,
    [232] = sys_epoll,
    [233] = sys_epoll,
    [270] = sys_select,
    [281] = sys_epoll,
    [283] = sys_timerfd,
    [284] = sys_eventfd,
    [286] = sys_timerfd,
    [287] = sys_timerfd,
    [288] = sys_accept,
    [290] = sys_eventfd,
    [291] = sys_epoll,
    [441] = sys_epoll,
    [46] = sys_sendmsg,
    [47] = sys_recvmsg,
    [53] = sys_socket,
    [56] = sys_clone,
    [57] = sys_fork,
    [58] = sys_fork,
    [59] = sys_execve,
    [61] = sys_wait4,
    [247] = sys_waitid,
    [322] = sys_execve,
    [60] = sys_exit,
    [72] = sys_fcntl,
    [74] = sys_fsync,
    [75] = sys_fsync,
    [77] = sys_ftruncate,
    [82] = sys_rename,
    [83] = sys_mkdir,
    [86] = sys_link,
    [87] = sys_unlink,
    [88] = sys_symlink,
    [89] = sys_readlink,
    [267] = sys_readlink,
    [93] = sys_fchown,
    [96] = sys_gettimeofday,
    [102] = sys_getuid,
    [104] = sys_getgid,
    [107] = sys_geteuid,
    [108] = sys_getegid,
    [137] = sys_statfs,
    [138] = sys_statfs,
    [158] = sys_arch_prctl,
    [186] = sys_gettid,
    [202] = sys_futex,
    [217] = sys_getdents64,
    [218] = sys_set_tid_address,
    [228] = sys_clock_gettime,
    [231] = sys_exit,
    [257] = sys_open,
    [258] = sys_mkdir,
    [262] = sys_stat,
    [264] = sys_rename,
    [265] = sys_link,
    [266] = sys_symlink,
    [269] = sys_faccessat,
    [271] = sys_poll,
    [273] = sys_set_robust_list,
    [274] = sys_get_robust_list,
    [285] = sys_fallocate,
    [292] = sys_dup,
    [293] = sys_pipe,
    [302] = sys_prlimit64,
    [318] = sys_getrandom,
    [319] = sys_memfd_create,
    [324] = sys_membarrier,
    [334] = sys_rseq,
    [439] = sys_faccessat,
};

// one Linux syscall, or a signal, on the guest's own thread: a pending signal, or one that interrupts a blocking
// call, makes the syscall restart or fail as SA_RESTART says once the signal is delivered on the way out
void
elftrans_dispatch(GuestRegs *r)
{
    guest_in_host = 1;
    if (r->kind == FRAME_SYSCALL) {
        long nr = (long)r->rax;
        long result = signals_pending() ? LINUX_ERESTARTSYS :
            nr >= 0 && nr < NSYSCALLS && syscalls[nr] ? syscalls[nr](r) : -38;

        if (result == -4 && signals_pending())
            result = LINUX_ERESTARTSYS;
        unsigned trace_index = __sync_fetch_and_add(&traced_syscalls, 1);
        if (result == -38 || trace_index < trace_limit) {
            char line[96];
            int size = snprintf(line, sizeof(line), "elftrans: syscall %ld -> %ld\n", nr, result);
            if (size > 0 && size < (int)sizeof(line))
                write(2, line, (size_t)size);
        }
        if (result != SYSCALL_RESTORED)
            r->rax = (uint64_t)result, r->rcx = r->rip, r->r11 = r->rflags;
    }
    signals_deliver(r);
    guest_in_host = 0;
}

_Static_assert(offsetof(GuestRegs, rip) == 128 && offsetof(GuestRegs, rflags) == 136 &&
    offsetof(GuestRegs, xsave) == 144, "elftrans_entry offsets");

// rbx holds the saved frame, rsp its FP area: the guest's FP and vector state is saved there, then the
// translation runs, then everything goes back, the vector state with xrstor (fxrstor without XSAVE)
__asm__(
    ".text\n"
    ".globl _elftrans_entry\n"
    "_elftrans_entry:\n"
    "    movq 144(%rbx), %rdi\n"
    "    movl $-1, %eax\n"
    "    movl $-1, %edx\n"
    "    cmpb $0, _elftrans_have_xsave(%rip)\n"
    "    je 1f\n"
    "    xsave64 (%rdi)\n"
    "    jmp 2f\n"
    "1:  fxsave64 (%rdi)\n"
    "2:  movq %rbx, %rdi\n"
    "    callq _elftrans_dispatch\n"
    "    movq 144(%rbx), %rsi\n"
    "    movl $-1, %eax\n"
    "    movl $-1, %edx\n"
    "    cmpb $0, _elftrans_have_xsave(%rip)\n"
    "    je 3f\n"
    "    xrstor64 (%rsi)\n"
    "    jmp 4f\n"
    "3:  fxrstor64 (%rsi)\n"
    "4:  movq %rbx, %rdi\n"
    "    jmp _elftrans_restore\n"
    ".globl _elftrans_restore\n"
    "_elftrans_restore:\n"
    "    pushq 136(%rdi)\n"
    "    popfq\n"
    "    movq 0(%rdi), %rax\n"
    "    movq 8(%rdi), %rbx\n"
    "    movq 24(%rdi), %rdx\n"
    "    movq 32(%rdi), %rsi\n"
    "    movq 48(%rdi), %rbp\n"
    "    movq 64(%rdi), %r8\n"
    "    movq 72(%rdi), %r9\n"
    "    movq 80(%rdi), %r10\n"
    "    movq 96(%rdi), %r12\n"
    "    movq 104(%rdi), %r13\n"
    "    movq 112(%rdi), %r14\n"
    "    movq 120(%rdi), %r15\n"
    "    movq 128(%rdi), %rcx\n"
    "    movq 136(%rdi), %r11\n"
    "    movq 56(%rdi), %rsp\n"
    "    movq 40(%rdi), %rdi\n"
    "    jmpq *%rcx\n");

// below the red zone, room for a Linux signal frame, then the saved registers and their FP area: the caller
// starts the thread at elftrans_entry with rbx at the registers and rsp at the FP area
uintptr_t
push_frame(GuestRegs *g, uint64_t kind)
{
    uintptr_t regs = (g->rsp - 128 - signal_reserve - sizeof(GuestRegs)) & ~(uintptr_t)63;
    uintptr_t area = (regs - xsave_size) & ~(uintptr_t)63;

    // XRSTOR wants the reserved header bytes zero, XSAVE leaves them
    memset((void *)(area + 512), 0, 64);
    g->xsave = area, g->kind = kind, g->orig_rax = g->rax;
    *(GuestRegs *)regs = *g;
    return regs;
}

#define FROM_STATE(g, s) do { \
    (g)->rax = (s)->__rax, (g)->rbx = (s)->__rbx, (g)->rcx = (s)->__rcx, (g)->rdx = (s)->__rdx; \
    (g)->rsi = (s)->__rsi, (g)->rdi = (s)->__rdi, (g)->rbp = (s)->__rbp, (g)->rsp = (s)->__rsp; \
    (g)->r8 = (s)->__r8, (g)->r9 = (s)->__r9, (g)->r10 = (s)->__r10, (g)->r11 = (s)->__r11; \
    (g)->r12 = (s)->__r12, (g)->r13 = (s)->__r13, (g)->r14 = (s)->__r14, (g)->r15 = (s)->__r15; \
    (g)->rip = (s)->__rip, (g)->rflags = (s)->__rflags; \
} while (0)

// a converted site: int3 then nop where the syscall was, the guest resumes after the nop
static void
trap(int sig, siginfo_t *info, void *context)
{
    ucontext_t *uc = context;
    x86_thread_state64_t *s = &uc->uc_mcontext->__ss;
    uintptr_t rip = (uintptr_t)s->__rip;
    GuestRegs g;

    (void)sig, (void)info;
    if (!is_syscall_site(rip - 1) || ((unsigned char *)rip)[-1] != 0xcc || ((unsigned char *)rip)[0] != 0x90)
        _exit(127);
    FROM_STATE(&g, s);
    g.rip = rip + 1;
    s->__rbx = push_frame(&g, FRAME_SYSCALL);
    s->__rsp = g.xsave;
    s->__rip = (uint64_t)(uintptr_t)elftrans_entry;
}

// anything the converter never saw, JIT code included, issues a real syscall: Linux numbers have no Darwin
// class, so the kernel raises EXC_SYSCALL and the thread is sent to elftrans_entry like a converted site
kern_return_t
catch_mach_exception_raise_state(mach_port_t port, exception_type_t exception, const mach_exception_data_t code,
    mach_msg_type_number_t code_count, int *flavor, const thread_state_t old_state, mach_msg_type_number_t old_count,
    thread_state_t new_state, mach_msg_type_number_t *new_count)
{
    x86_thread_state64_t s;
    GuestRegs g;

    (void)port, (void)code, (void)code_count;
    if (exception != EXC_SYSCALL || *flavor != x86_THREAD_STATE64 || old_count < x86_THREAD_STATE64_COUNT)
        return KERN_FAILURE;
    memcpy(&s, old_state, sizeof(s));
    FROM_STATE(&g, &s);
    // the saved rip is the instruction after the syscall, the one the guest resumes at
    if (((unsigned char *)(uintptr_t)s.__rip)[-2] != 0x0f || ((unsigned char *)(uintptr_t)s.__rip)[-1] != 0x05)
        return KERN_FAILURE;
    s.__rbx = push_frame(&g, FRAME_SYSCALL);
    s.__rsp = g.xsave;
    s.__rip = (uint64_t)(uintptr_t)elftrans_entry;
    memcpy(new_state, &s, sizeof(s));
    *new_count = x86_THREAD_STATE64_COUNT;
    return KERN_SUCCESS;
}

kern_return_t
catch_mach_exception_raise(mach_port_t port, mach_port_t thread, mach_port_t task, exception_type_t exception,
    mach_exception_data_t code, mach_msg_type_number_t code_count)
{
    (void)port, (void)thread, (void)task, (void)exception, (void)code, (void)code_count;
    return KERN_FAILURE;
}

kern_return_t
catch_mach_exception_raise_state_identity(mach_port_t port, mach_port_t thread, mach_port_t task,
    exception_type_t exception, mach_exception_data_t code, mach_msg_type_number_t code_count, int *flavor,
    thread_state_t old_state, mach_msg_type_number_t old_count, thread_state_t new_state,
    mach_msg_type_number_t *new_count)
{
    (void)port, (void)thread, (void)task, (void)exception, (void)code, (void)code_count;
    (void)flavor, (void)old_state, (void)old_count, (void)new_state, (void)new_count;
    return KERN_FAILURE;
}

extern boolean_t mach_exc_server(mach_msg_header_t *request, mach_msg_header_t *reply);

static void *
syscall_server(void *unused)
{
    sigset_t all;

    (void)unused;
    // a guest's signals go to guest threads, never this one
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, NULL);
    for (;;) {
        union {
            mach_msg_header_t head;
            uint8_t bytes[4096];
        } request, reply;

        if (mach_msg(&request.head, MACH_RCV_MSG, 0, sizeof(request), syscall_port, MACH_MSG_TIMEOUT_NONE,
                MACH_PORT_NULL) != KERN_SUCCESS)
            _exit(125);
        memset(&reply, 0, sizeof(reply));
        if (!mach_exc_server(&request.head, &reply.head) || reply.head.msgh_remote_port == MACH_PORT_NULL)
            continue;
        mach_msg(&reply.head, MACH_SEND_MSG, reply.head.msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE,
            MACH_PORT_NULL);
    }
    return NULL;
}

void
elftrans_set_trace(unsigned limit)
{
    trace_limit = limit;
}

void
elftrans_set_brk(uintptr_t end)
{
    brk_base = brk_current = round_up(end);
}

// the FP and vector state the entry path saves: XSAVE's size for the enabled features, else FXSAVE's 512 bytes
static void
fp_state_size(void)
{
    unsigned eax, ebx, ecx, edx;

    __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1), "c"(0));
    if (ecx & (1U << 27)) {
        __asm__("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0xd), "c"(0));
        xsave_size = (ebx + 63) & ~63U, elftrans_have_xsave = 1;
    }
    // a Linux signal frame and its FP state, kept free below the red zone
    signal_reserve = 512 + xsave_size + 64;
}

// the trap handler, the signal handlers, the main thread's signal stack, and the EXC_SYSCALL port every
// thread inherits
int
elftrans_install(void)
{
    struct sigaction sa = {0};
    mach_port_t self = mach_task_self();
    pthread_t server;

    fp_state_size();
    if (signal_stack())
        return -1;
    sa.sa_sigaction = trap;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigfillset(&sa.sa_mask);
    if (sigaction(SIGTRAP, &sa, NULL) || signals_install())
        return -1;
    {
        uint32_t size = sizeof(elftrans_self);
        char path[PATH_MAX];

        if (_NSGetExecutablePath(path, &size) || !realpath(path, elftrans_self))
            elftrans_self[0] = 0;
    }
    (void)self, (void)server;
    return syscall_port_start();
}

// the EXC_SYSCALL port and the thread serving it: a fork child has neither, its port name is the parent's
static int
syscall_port_start(void)
{
    mach_port_t self = mach_task_self();
    pthread_t server;

    if (mach_port_allocate(self, MACH_PORT_RIGHT_RECEIVE, &syscall_port) != KERN_SUCCESS ||
        mach_port_insert_right(self, syscall_port, syscall_port, MACH_MSG_TYPE_MAKE_SEND) != KERN_SUCCESS ||
        task_set_exception_ports(self, EXC_MASK_SYSCALL, syscall_port, EXCEPTION_STATE | MACH_EXCEPTION_CODES,
            x86_THREAD_STATE64) != KERN_SUCCESS ||
        pthread_create(&server, NULL, syscall_server, NULL))
        return -1;
    pthread_detach(server);
    return 0;
}
