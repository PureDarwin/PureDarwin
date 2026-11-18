// CFRunLoopWeb.h - the eventfd, timerfd and epoll calls CFRunLoop waits with, kept in memory for the browser
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: Apache-2.0
#ifndef __CFRUNLOOPWEB__
#define __CFRUNLOOPWEB__

#include <poll.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

// a page cannot block, so every wait checks what is ready and returns: run a mode with a zero timeout once per tick

#define EFD_CLOEXEC 0
#define EFD_NONBLOCK 0
#define TFD_CLOEXEC 0
#define TFD_NONBLOCK 0
#define TFD_TIMER_ABSTIME 1
#define EPOLL_CLOEXEC 0
#define EPOLLIN 0x1
#define EPOLLET (1u << 31)
#define EPOLL_CTL_ADD 1
#define EPOLL_CTL_DEL 2

typedef uint64_t eventfd_t;

typedef union epoll_data {
    void *ptr;
    int fd;
    uint32_t u32;
    uint64_t u64;
} epoll_data_t;

struct epoll_event {
    uint32_t events;
    epoll_data_t data;
};

int _CFRunLoopWebEventFD(unsigned int initval, int flags);
int _CFRunLoopWebEventFDWrite(int fd, eventfd_t value);
int _CFRunLoopWebTimerFD(int clockid, int flags);
int _CFRunLoopWebTimerFDSet(int fd, int flags, const struct itimerspec *value, struct itimerspec *old);
int _CFRunLoopWebEpoll(int flags);
int _CFRunLoopWebEpollCtl(int epfd, int op, int fd, struct epoll_event *event);
int _CFRunLoopWebEpollWait(int epfd, struct epoll_event *events, int max, int timeout);
int _CFRunLoopWebPoll(struct pollfd *fds, nfds_t n, const struct timespec *timeout, const void *sigmask);
ssize_t _CFRunLoopWebRead(int fd, void *buf, size_t n);
int _CFRunLoopWebClose(int fd);

#define eventfd(v, f) _CFRunLoopWebEventFD(v, f)
#define eventfd_write(fd, v) _CFRunLoopWebEventFDWrite(fd, v)
#define timerfd_create(c, f) _CFRunLoopWebTimerFD(c, f)
#define timerfd_settime(fd, f, v, o) _CFRunLoopWebTimerFDSet(fd, f, v, o)
#define epoll_create1(f) _CFRunLoopWebEpoll(f)
#define epoll_ctl(e, op, fd, ev) _CFRunLoopWebEpollCtl(e, op, fd, ev)
#define epoll_wait(e, ev, m, t) _CFRunLoopWebEpollWait(e, ev, m, t)
#define ppoll(f, n, t, s) _CFRunLoopWebPoll(f, n, t, s)
#define read(fd, b, n) _CFRunLoopWebRead(fd, b, n)
#define close(fd) _CFRunLoopWebClose(fd)

#endif
