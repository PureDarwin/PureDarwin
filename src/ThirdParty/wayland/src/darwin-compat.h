/* PureDarwin event primitives for the vendored Wayland event loop. */
#ifndef WAYLAND_DARWIN_COMPAT_H
#define WAYLAND_DARWIN_COMPAT_H

#include <stdint.h>
#include <signal.h>
#include <time.h>
#include <sys/socket.h>

#define EPOLLIN 0x001
#define EPOLLOUT 0x004
#define EPOLLERR 0x008
#define EPOLLHUP 0x010
#define EPOLL_CLOEXEC 0x80000
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2
#define EPOLL_CTL_MOD 3
#define TFD_CLOEXEC 0x80000
#define TFD_NONBLOCK 0x800
#define TFD_TIMER_ABSTIME 1
#define SFD_CLOEXEC 0x80000
#define SFD_NONBLOCK 0x800
#define EFD_CLOEXEC 0x80000
#define EFD_NONBLOCK 0x800
#ifndef NSIG
#define NSIG 64
#endif
#ifndef NOTE_MSECONDS
#define NOTE_MSECONDS 0x00000002
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif
#ifndef AF_LOCAL
#define AF_LOCAL AF_UNIX
#endif
#ifndef MSG_DONTWAIT
#define MSG_DONTWAIT 0
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
// strict POSIX headers hide it, the value is fixed in xnu's sys/socket.h
#if defined(__APPLE__) && !defined(SO_NOSIGPIPE)
#define SO_NOSIGPIPE 0x1022
#endif
#ifndef SOCK_CLOEXEC
#define SOCK_CLOEXEC 0
#endif
#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif
#ifndef CMSG_LEN
#define CMSG_LEN(length) (CMSG_ALIGN(sizeof(struct cmsghdr)) + (length))
#endif

struct itimerspec {
    struct timespec it_interval;
    struct timespec it_value;
};

struct epoll_event {
    uint32_t events;
    union { void *ptr; uint64_t u64; } data;
};
struct signalfd_siginfo { uint32_t ssi_signo; };

int epoll_create(int size);
int epoll_create1(int flags);
int epoll_ctl(int epfd, int operation, int fd, struct epoll_event *event);
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout);
int timerfd_create(int clockid, int flags);
int timerfd_settime(int fd, int flags, const struct itimerspec *new_value,
                    struct itimerspec *old_value);
int signalfd(int fd, const sigset_t *mask, int flags);
#endif
