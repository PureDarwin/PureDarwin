// CFRunLoopWeb.c - in-memory eventfds, timerfds and epoll sets for CFRunLoop in the browser
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: Apache-2.0
#if defined(__EMSCRIPTEN__)

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "CFRunLoopWeb.h"

#undef read
#undef close

// dispatch's queue handles are fake descriptors too, ready while their queue has work
extern bool _dispatch_web_handle_ready(int handle);
#define DISPATCH_HANDLE_BASE 0x7f000000

// our descriptors sit far above any the C library hands out
#define FD_BASE 0x40000000
#define FD_MAX 4096

enum { KIND_FREE, KIND_EVENT, KIND_TIMER, KIND_EPOLL };

struct webfd {
    int kind;
    uint64_t count;     // eventfd counter
    uint64_t deadline;  // timerfd expiry on CLOCK_MONOTONIC in ns, 0 when disarmed
    int *members;       // epoll set
    int nmembers;
};

static struct webfd fds[FD_MAX];

static struct webfd *lookup(int fd)
{
    if (fd < FD_BASE || fd >= FD_BASE + FD_MAX || fds[fd - FD_BASE].kind == KIND_FREE)
        return NULL;
    return &fds[fd - FD_BASE];
}

static int allocate(int kind)
{
    for (int i = 0; i < FD_MAX; i++) {
        if (fds[i].kind == KIND_FREE) {
            memset(&fds[i], 0, sizeof(fds[i]));
            fds[i].kind = kind;
            return FD_BASE + i;
        }
    }
    errno = EMFILE;
    return -1;
}

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static bool ready(int fd)
{
    if (fd >= DISPATCH_HANDLE_BASE)
        return _dispatch_web_handle_ready(fd);
    struct webfd *f = lookup(fd);
    if (f == NULL)
        return false;
    if (f->kind == KIND_EVENT)
        return f->count != 0;
    if (f->kind == KIND_TIMER)
        return f->deadline != 0 && now_ns() >= f->deadline;
    for (int i = 0; i < f->nmembers; i++)
        if (ready(f->members[i]))
            return true;
    return false;
}

int _CFRunLoopWebEventFD(unsigned int initval, int flags)
{
    (void)flags;
    int fd = allocate(KIND_EVENT);
    if (fd >= 0)
        lookup(fd)->count = initval;
    return fd;
}

int _CFRunLoopWebEventFDWrite(int fd, eventfd_t value)
{
    struct webfd *f = lookup(fd);
    if (f == NULL || f->kind != KIND_EVENT) {
        errno = EBADF;
        return -1;
    }
    f->count += value;
    return 0;
}

int _CFRunLoopWebTimerFD(int clockid, int flags)
{
    (void)clockid;
    (void)flags;
    return allocate(KIND_TIMER);
}

int _CFRunLoopWebTimerFDSet(int fd, int flags, const struct itimerspec *value, struct itimerspec *old)
{
    struct webfd *f = lookup(fd);
    if (f == NULL || f->kind != KIND_TIMER) {
        errno = EBADF;
        return -1;
    }
    if (old)
        memset(old, 0, sizeof(*old));
    uint64_t when = (uint64_t)value->it_value.tv_sec * 1000000000ull + value->it_value.tv_nsec;
    if (when != 0 && !(flags & TFD_TIMER_ABSTIME))
        when += now_ns();
    f->deadline = when;
    return 0;
}

int _CFRunLoopWebEpoll(int flags)
{
    (void)flags;
    return allocate(KIND_EPOLL);
}

int _CFRunLoopWebEpollCtl(int epfd, int op, int fd, struct epoll_event *event)
{
    (void)event;
    struct webfd *e = lookup(epfd);
    if (e == NULL || e->kind != KIND_EPOLL) {
        errno = EBADF;
        return -1;
    }
    for (int i = 0; i < e->nmembers; i++) {
        if (e->members[i] != fd)
            continue;
        if (op == EPOLL_CTL_ADD) {
            errno = EEXIST;
            return -1;
        }
        e->members[i] = e->members[--e->nmembers];
        return 0;
    }
    if (op != EPOLL_CTL_ADD) {
        errno = ENOENT;
        return -1;
    }
    e->members = realloc(e->members, (e->nmembers + 1) * sizeof(int));
    e->members[e->nmembers++] = fd;
    return 0;
}

int _CFRunLoopWebEpollWait(int epfd, struct epoll_event *events, int max, int timeout)
{
    (void)timeout;
    struct webfd *e = lookup(epfd);
    if (e == NULL || e->kind != KIND_EPOLL) {
        errno = EBADF;
        return -1;
    }
    int n = 0;
    for (int i = 0; i < e->nmembers && n < max; i++) {
        if (ready(e->members[i])) {
            events[n].events = EPOLLIN;
            events[n].data.fd = e->members[i];
            n++;
        }
    }
    return n;
}

// never blocks, whatever the timeout: the page's event loop is the only place to wait
int _CFRunLoopWebPoll(struct pollfd *p, nfds_t n, const struct timespec *timeout, const void *sigmask)
{
    (void)timeout;
    (void)sigmask;
    int count = 0;
    for (nfds_t i = 0; i < n; i++) {
        p[i].revents = ready(p[i].fd) ? POLLIN : 0;
        count += p[i].revents != 0;
    }
    return count;
}

ssize_t _CFRunLoopWebRead(int fd, void *buf, size_t n)
{
    if (fd >= DISPATCH_HANDLE_BASE) {
        // the queue itself is the state, there is nothing to acknowledge
        if (n >= sizeof(uint64_t))
            *(uint64_t *)buf = 1;
        return sizeof(uint64_t);
    }
    struct webfd *f = lookup(fd);
    if (f == NULL)
        return read(fd, buf, n);
    if (n < sizeof(uint64_t)) {
        errno = EINVAL;
        return -1;
    }
    uint64_t v;
    if (f->kind == KIND_EVENT) {
        v = f->count;
        f->count = 0;
    } else if (f->kind == KIND_TIMER && ready(fd)) {
        v = 1;
        f->deadline = 0;
    } else {
        v = 0;
    }
    if (v == 0) {
        errno = EAGAIN;
        return -1;
    }
    memcpy(buf, &v, sizeof(v));
    return sizeof(v);
}

int _CFRunLoopWebClose(int fd)
{
    if (fd >= DISPATCH_HANDLE_BASE)
        return 0;
    struct webfd *f = lookup(fd);
    if (f == NULL)
        return close(fd);
    free(f->members);
    memset(f, 0, sizeof(*f));
    return 0;
}

#endif
