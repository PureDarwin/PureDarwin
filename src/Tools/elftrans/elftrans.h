// elftrans.h - Linux x86-64 types and what the loader and syscall translation share
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef ELFTRANS_H
#define ELFTRANS_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

#define PAGE 4096UL
#define GUEST_LO 0x10000UL
#define GUEST_HI 0x100000000UL

// the guest's registers at a syscall or a signal, saved on its stack on the way in, loaded back on the way out:
// xsave is where the stub saved the FP and vector state, kind says why the thread came in
typedef struct {
    uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip, rflags;
    uint64_t xsave, kind, orig_rax;
} GuestRegs;

#define FRAME_SYSCALL 0
#define FRAME_SIGNAL 1
// a syscall interrupted by a signal: restarted, or EINTR when the handler has no SA_RESTART
#define LINUX_ERESTARTSYS (-512)
// rt_sigreturn put back every register itself
#define SYSCALL_RESTORED (-4096)

typedef struct {
    int64_t sec, nsec;
} LinuxTime;

typedef struct {
    uint64_t dev, ino, nlink;
    uint32_t mode, uid, gid, pad;
    uint64_t rdev;
    int64_t size, blksize, blocks;
    LinuxTime atime, mtime, ctime;
    int64_t unused[3];
} LinuxStat;

typedef struct {
    uint64_t type, bsize, blocks, bfree, bavail, files, ffree;
    uint32_t fsid[2];
    uint64_t namelen, frsize, flags, spare[4];
} LinuxStatfs;

typedef struct {
    uint64_t ino;
    int64_t offset;
    uint16_t length;
    uint8_t type;
    char name[];
} LinuxDirent;

typedef struct {
    void *name;
    uint32_t name_length, name_padding;
    struct iovec *iov;
    uint32_t iov_count, iov_padding;
    void *control;
    uint32_t control_length, control_padding;
    int32_t flags;
} LinuxMsgHdr;

typedef struct {
    uint64_t length;
    int32_t level, type;
} LinuxCmsg;

typedef struct {
    uintptr_t handler;
    uint64_t flags;
    uintptr_t restorer;
    uint64_t mask;
} LinuxSigaction;

typedef struct {
    uint64_t current, maximum;
} LinuxRlimit;

typedef struct {
    uintptr_t next;
} LinuxRobustNode;

typedef struct {
    LinuxRobustNode list;
    long futex_offset;
    uintptr_t pending;
} LinuxRobustHead;
static inline uintptr_t
round_down(uintptr_t n)
{
    return n & ~(PAGE - 1);
}

static inline uintptr_t
round_up(uintptr_t n)
{
    return (n + PAGE - 1) & ~(PAGE - 1);
}

// loader side: converted images, their syscall sites and the AOT cache
extern uintptr_t guest_library_bias;
extern pthread_mutex_t sites_lock;
int publish_sites(uintptr_t remove_lo, uintptr_t remove_hi, const uintptr_t *added, size_t add_count);
int is_syscall_site(uintptr_t site);
int register_mapped_aot(int fd, off_t offset, uintptr_t address, size_t length);
int prepare_aot(const char *source, char *out, size_t capacity);
int is_elf_path(const char *path);
int aot_open(int fd);
int in_guest_space(uintptr_t lo, uintptr_t hi);

// syscall side: syscalls from converted sites trap, any other syscall arrives as EXC_SYSCALL
extern uint64_t xsave_size;
uintptr_t push_frame(GuestRegs *g, uint64_t kind);
void elftrans_entry(void);
_Noreturn void elftrans_restore(const GuestRegs *r);
long linux_error(int error);
void elftrans_set_trace(unsigned limit);
void elftrans_set_brk(uintptr_t end);
int elftrans_install(void);

// signals: Linux dispositions, masks and frames, delivered on the way back to the guest
extern __thread int guest_in_host;
int signals_install(void);
void signals_thread_start(int tid, uint64_t mask);
void signals_thread_exit(void);
int procfs_open(const char *path, int linux_flags, long *result);
int procfs_readlink(const char *path, char *buf, size_t size, long *result);
struct stat;
int procfs_stat(const char *path, int follow, struct stat *st, long *result);
long procfs_sysinfo(void *out);
struct sockaddr;
struct sockaddr_storage;
int net_to_native(const void *guest, uint32_t length, struct sockaddr_storage *native, unsigned *native_length);
void net_to_linux(const struct sockaddr *native, unsigned native_length, void *guest, uint32_t *length);
long net_socket(int domain, int linux_type, int protocol);
long net_bind_connect(int fd, const void *address, uint32_t length, int connecting);
long net_accept(int fd, void *address, uint32_t *length, int linux_flags);
long net_name(int fd, void *address, uint32_t *length, int peer);
long net_sendto(int fd, const void *buf, size_t n, int linux_flags, const void *address, uint32_t length);
long net_recvfrom(int fd, void *buf, size_t n, int linux_flags, void *address, uint32_t *length);
int net_option(int level, int option, int *native_level, int *native_option);
int net_option_ignored(int level, int option);
long events_eventfd(unsigned initial, int flags);
long events_timerfd_create(int clock, int flags);
long events_timerfd_settime(int fd, int flags, const void *value, void *old);
long events_timerfd_gettime(int fd, void *out);
long events_epoll_create(int flags);
long events_epoll_ctl(int epfd, int op, int fd, const void *event);
long events_epoll_wait(int epfd, void *out, int max, int64_t timeout_ns);
long events_select(int nfds, uint64_t *rd, uint64_t *wr, uint64_t *ex, int64_t timeout_ns);
int events_read(int fd, void *buf, size_t n, long *result);
int events_write(int fd, const void *buf, size_t n, long *result);
void events_close(int fd);
extern char guest_exe[];
extern const char *const *guest_argv;
extern int guest_argc;
void signals_fork_prepare(void);
void signals_fork_parent(void);
void signals_fork_child(int tid);
uint64_t signals_ignored(void);
void signals_inherit(uint64_t mask, uint64_t ignored);
int signals_linux_number(int native);
uint64_t signals_mask(void);
int signals_pending(void);
void signals_deliver(GuestRegs *r);
long sys_rt_sigaction(GuestRegs *r);
long sys_rt_sigprocmask(GuestRegs *r);
long sys_rt_sigreturn(GuestRegs *r);
long sys_sigaltstack(GuestRegs *r);
long sys_kill(GuestRegs *r);
long sys_tkill(GuestRegs *r);
long sys_tgkill(GuestRegs *r);

#endif
