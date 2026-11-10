// events.c - epoll, eventfd, timerfd and select for an elftrans guest, on kqueue, socket pairs and poll
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#define _DARWIN_C_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "elftrans.h"

#define EPOLLIN 0x001u
#define EPOLLPRI 0x002u
#define EPOLLOUT 0x004u
#define EPOLLERR 0x008u
#define EPOLLHUP 0x010u
#define EPOLLRDHUP 0x2000u
#define EPOLLONESHOT (1u << 30)
#define EPOLLET (1u << 31)

// Linux's struct epoll_event is packed on x86-64: 12 bytes
typedef struct __attribute__((packed)) {
    uint32_t events;
    uint64_t data;
} LinuxEpollEvent;

enum { SPECIAL_NONE, SPECIAL_EPOLL, SPECIAL_EVENT, SPECIAL_TIMER };

typedef struct {
    int fd;
    uint32_t events;
    uint64_t data;
} Watch;

typedef struct {
    int kind;
    // eventfd and timerfd: the end elftrans writes a byte to while the count is nonzero
    int other;
    uint64_t count;
    int semaphore;
    // timerfd
    int clock;
    uint64_t interval_ns, deadline_ns;
    // armed one-shot for a first expiry unlike the interval: periodic from the first time it fires
    int rearm;
    // epoll: what is watched, with the guest's data
    Watch *watches;
    unsigned nwatches, cap;
} Special;

#define MAX_SPECIAL 65536
static Special *specials[MAX_SPECIAL];
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static int timer_queue = -1;

static Special *
special(int fd, int kind)
{
    Special *s = fd >= 0 && fd < MAX_SPECIAL ? specials[fd] : NULL;

    return s && (!kind || s->kind == kind) ? s : NULL;
}

static uint64_t
clock_ns(int linux_clock)
{
    struct timespec ts;

    clock_gettime(linux_clock == 0 ? CLOCK_REALTIME : CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

// the counted fds: one end of a socket pair for the guest, a byte waits on it while the count is nonzero
static long
counted_new(int kind, int flags, uint64_t initial)
{
    int pair[2];
    Special *s;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair))
        return -linux_error(errno);
    if (pair[0] >= MAX_SPECIAL || !(s = calloc(1, sizeof(*s)))) {
        close(pair[0]), close(pair[1]);
        return -24;
    }
    fcntl(pair[1], F_SETFD, FD_CLOEXEC);
    fcntl(pair[1], F_SETFL, O_NONBLOCK);
    if (flags & 0x80000)
        fcntl(pair[0], F_SETFD, FD_CLOEXEC);
    if (flags & 0x800)
        fcntl(pair[0], F_SETFL, O_NONBLOCK);
    s->kind = kind, s->other = pair[1], s->count = initial;
    if (initial)
        send(pair[1], "", 1, 0);
    pthread_mutex_lock(&lock);
    specials[pair[0]] = s;
    pthread_mutex_unlock(&lock);
    return pair[0];
}

long
events_eventfd(unsigned initial, int flags)
{
    long fd;

    if (flags & ~(1 | 0x800 | 0x80000))
        return -22;
    fd = counted_new(SPECIAL_EVENT, flags, initial);
    if (fd >= 0)
        specials[fd]->semaphore = flags & 1;
    return fd;
}

// a read of a counted fd: 8 bytes of count, blocking (unless O_NONBLOCK) while it is zero
static long
counted_read(int fd, Special *s, void *buf, size_t n)
{
    uint64_t value;

    if (n < 8)
        return -22;
    for (;;) {
        char byte;
        int flags;

        pthread_mutex_lock(&lock);
        if (s->count) {
            value = s->kind == SPECIAL_EVENT && s->semaphore ? 1 : s->count;
            s->count -= value;
            if (!s->count) {
                while (recv(fd, &byte, 1, MSG_DONTWAIT) == 1) {
                }
            }
            pthread_mutex_unlock(&lock);
            memcpy(buf, &value, 8);
            return 8;
        }
        pthread_mutex_unlock(&lock);
        flags = fcntl(fd, F_GETFL);
        if (flags >= 0 && (flags & O_NONBLOCK))
            return -11;
        // waits for the byte, without taking it
        if (recv(fd, &byte, 1, MSG_PEEK) < 0)
            return -linux_error(errno);
    }
}

static long
counted_add(Special *s, uint64_t add)
{
    pthread_mutex_lock(&lock);
    if (add > UINT64_MAX - 1 - s->count) {
        pthread_mutex_unlock(&lock);
        return -11;
    }
    if (!s->count && add)
        send(s->other, "", 1, 0);
    s->count += add;
    pthread_mutex_unlock(&lock);
    return 0;
}

int
events_read(int fd, void *buf, size_t n, long *result)
{
    Special *s = special(fd, 0);

    if (!s || (s->kind != SPECIAL_EVENT && s->kind != SPECIAL_TIMER))
        return 0;
    *result = counted_read(fd, s, buf, n);
    return 1;
}

int
events_write(int fd, const void *buf, size_t n, long *result)
{
    Special *s = special(fd, SPECIAL_EVENT);
    uint64_t add;

    if (!s)
        return special(fd, 0) ? (*result = -22, 1) : 0;
    if (n < 8) {
        *result = -22;
        return 1;
    }
    memcpy(&add, buf, 8);
    if (add == UINT64_MAX) {
        *result = -22;
        return 1;
    }
    *result = counted_add(s, add);
    if (!*result)
        *result = 8;
    return 1;
}

// a one-shot first expiry of a periodic timer re-arms it periodic, from the timer thread's point of view
static void
timer_rearm(int fd, Special *s)
{
    struct kevent ev;

    EV_SET(&ev, (uintptr_t)fd, EVFILT_TIMER, EV_ADD, NOTE_NSECONDS, (int64_t)s->interval_ns, NULL);
    kevent(timer_queue, &ev, 1, NULL, 0, NULL);
}

// timers fire on one thread, through a kqueue
static void *
timer_thread(void *arg)
{
    sigset_t all;

    (void)arg;
    sigfillset(&all);
    pthread_sigmask(SIG_BLOCK, &all, NULL);
    for (;;) {
        struct kevent ev;
        Special *s;

        if (kevent(timer_queue, NULL, 0, &ev, 1, NULL) != 1)
            continue;
        pthread_mutex_lock(&lock);
        s = special((int)ev.ident, SPECIAL_TIMER);
        if (s && s->deadline_ns) {
            uint64_t fired = ev.data > 0 ? (uint64_t)ev.data : 1;

            if (!s->count)
                send(s->other, "", 1, 0);
            s->count += fired;
            if (s->interval_ns) {
                s->deadline_ns += (s->rearm ? fired - 1 : fired) * s->interval_ns + (s->rearm ? s->interval_ns : 0);
                if (s->rearm) {
                    s->rearm = 0;
                    timer_rearm((int)ev.ident, s);
                }
            } else {
                s->deadline_ns = 0;
            }
        }
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

long
events_timerfd_create(int clock, int flags)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    static int started;
    long fd;

    // CLOCK_REALTIME, CLOCK_MONOTONIC, CLOCK_BOOTTIME and the alarm clocks
    if ((clock != 0 && clock != 1 && clock != 7 && clock != 8 && clock != 9) || (flags & ~(0x800 | 0x80000)))
        return -22;
    pthread_mutex_lock(&lock);
    if (!started) {
        pthread_t t;

        timer_queue = kqueue();
        if (timer_queue >= 0 && !pthread_create(&t, NULL, timer_thread, NULL)) {
            pthread_detach(t);
            fcntl(timer_queue, F_SETFD, FD_CLOEXEC);
            started = 1;
        }
    }
    pthread_mutex_unlock(&lock);
    (void)once;
    if (!started)
        return -12;
    fd = counted_new(SPECIAL_TIMER, flags, 0);
    if (fd >= 0)
        specials[fd]->clock = clock == 0 || clock == 8 ? 0 : 1;
    return fd;
}

typedef struct {
    int64_t interval_sec, interval_nsec, value_sec, value_nsec;
} LinuxItimerspec;

static void
timer_get(Special *s, LinuxItimerspec *out)
{
    uint64_t now = clock_ns(s->clock), left = s->deadline_ns > now ? s->deadline_ns - now : (s->deadline_ns ? 1 : 0);

    out->interval_sec = (int64_t)(s->interval_ns / 1000000000ull);
    out->interval_nsec = (int64_t)(s->interval_ns % 1000000000ull);
    out->value_sec = (int64_t)(left / 1000000000ull), out->value_nsec = (int64_t)(left % 1000000000ull);
}

long
events_timerfd_settime(int fd, int flags, const void *value_arg, void *old_arg)
{
    const LinuxItimerspec *value = value_arg;
    LinuxItimerspec *old = old_arg;
    Special *s = special(fd, SPECIAL_TIMER);
    struct kevent ev;
    uint64_t now, start, interval;

    if (!s)
        return -9;
    if (!value || (flags & ~3) || value->value_nsec < 0 || value->value_nsec >= 1000000000 || value->interval_nsec < 0 ||
        value->interval_nsec >= 1000000000 || value->value_sec < 0 || value->interval_sec < 0)
        return -22;
    pthread_mutex_lock(&lock);
    if (old)
        timer_get(s, old);
    now = clock_ns(s->clock);
    start = (uint64_t)value->value_sec * 1000000000ull + (uint64_t)value->value_nsec;
    interval = (uint64_t)value->interval_sec * 1000000000ull + (uint64_t)value->interval_nsec;
    EV_SET(&ev, (uintptr_t)fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
    kevent(timer_queue, &ev, 1, NULL, 0, NULL);
    s->interval_ns = interval, s->deadline_ns = 0, s->rearm = 0;
    if (start) {
        uint64_t first = flags & 1 ? (start > now ? start - now : 1) : start;

        s->deadline_ns = now + first;
        // the first expiry, then the interval: a periodic timer the first time it is armed is late by
        // nothing, Darwin's periodic timers start one interval out
        if (interval && first == interval) {
            EV_SET(&ev, (uintptr_t)fd, EVFILT_TIMER, EV_ADD, NOTE_NSECONDS, (int64_t)interval, NULL);
        } else {
            EV_SET(&ev, (uintptr_t)fd, EVFILT_TIMER, EV_ADD | EV_ONESHOT, NOTE_NSECONDS, (int64_t)first, NULL);
        }
        if (kevent(timer_queue, &ev, 1, NULL, 0, NULL)) {
            pthread_mutex_unlock(&lock);
            return -linux_error(errno);
        }
        s->rearm = interval && first != interval;
    }
    pthread_mutex_unlock(&lock);
    return 0;
}

long
events_timerfd_gettime(int fd, void *out_arg)
{
    LinuxItimerspec *out = out_arg;
    Special *s = special(fd, SPECIAL_TIMER);

    if (!s)
        return -9;
    pthread_mutex_lock(&lock);
    timer_get(s, out);
    pthread_mutex_unlock(&lock);
    return 0;
}

long
events_epoll_create(int flags)
{
    int kq;
    Special *s;

    if (flags & ~0x80000)
        return -22;
    if ((kq = kqueue()) < 0)
        return -linux_error(errno);
    if (kq >= MAX_SPECIAL || !(s = calloc(1, sizeof(*s)))) {
        close(kq);
        return -24;
    }
    if (flags & 0x80000)
        fcntl(kq, F_SETFD, FD_CLOEXEC);
    s->kind = SPECIAL_EPOLL;
    pthread_mutex_lock(&lock);
    specials[kq] = s;
    pthread_mutex_unlock(&lock);
    return kq;
}

static Watch *
watch_of(Special *e, int fd)
{
    for (unsigned i = 0; i < e->nwatches; i++) {
        if (e->watches[i].fd == fd)
            return &e->watches[i];
    }
    return NULL;
}

// the kqueue filters for a watch: read and write, level or edge triggered, one-shot
static int
watch_apply(int kq, int fd, uint32_t events, int add)
{
    struct kevent ev[2];
    unsigned short flags = (events & EPOLLET ? EV_CLEAR : 0) | (events & EPOLLONESHOT ? EV_ONESHOT : 0);
    int n = 0;

    EV_SET(&ev[n++], (uintptr_t)fd, EVFILT_READ, add && (events & (EPOLLIN | EPOLLRDHUP | EPOLLPRI)) ? EV_ADD | flags :
                                                   EV_DELETE, 0, 0, NULL);
    EV_SET(&ev[n++], (uintptr_t)fd, EVFILT_WRITE, add && (events & EPOLLOUT) ? EV_ADD | flags : EV_DELETE, 0, 0, NULL);
    // deleting a filter that was never added fails with ENOENT, which says nothing: each change on its own
    for (int i = 0; i < n; i++) {
        if (kevent(kq, &ev[i], 1, NULL, 0, NULL) && !(ev[i].flags & EV_DELETE))
            return -linux_error(errno);
    }
    return 0;
}

long
events_epoll_ctl(int epfd, int op, int fd, const void *event_arg)
{
    const LinuxEpollEvent *event = event_arg;
    Special *e = special(epfd, SPECIAL_EPOLL);
    Watch *w;
    long rc = 0;

    if (!e)
        return special(epfd, 0) || fcntl(epfd, F_GETFD) >= 0 ? -22 : -9;
    if (fd == epfd)
        return -22;
    if (fcntl(fd, F_GETFD) < 0)
        return -9;
    pthread_mutex_lock(&lock);
    w = watch_of(e, fd);
    if (op == 1) {
        if (w) {
            rc = -17;
        } else if (e->nwatches == e->cap &&
                   !(e->watches = realloc(e->watches, (e->cap = e->cap ? e->cap * 2 : 16) * sizeof(Watch)))) {
            rc = -12;
        } else {
            w = &e->watches[e->nwatches++];
            w->fd = fd, w->events = event->events, w->data = event->data;
            if ((rc = watch_apply(epfd, fd, event->events, 1)))
                e->nwatches--;
        }
    } else if (op == 3) {
        if (!w) {
            rc = -2;
        } else {
            w->events = event->events, w->data = event->data;
            watch_apply(epfd, fd, 0, 0);
            rc = watch_apply(epfd, fd, event->events, 1);
        }
    } else if (op == 2) {
        if (!w) {
            rc = -2;
        } else {
            watch_apply(epfd, fd, 0, 0);
            *w = e->watches[--e->nwatches];
        }
    } else {
        rc = -22;
    }
    pthread_mutex_unlock(&lock);
    return rc;
}

long
events_epoll_wait(int epfd, void *out_arg, int max, int64_t timeout_ns)
{
    LinuxEpollEvent *out = out_arg;
    Special *e = special(epfd, SPECIAL_EPOLL);
    struct kevent ev[256];
    struct timespec ts, *tp = NULL;
    int n, count = 0, owner[256];

    if (!e)
        return -9;
    if (max <= 0)
        return -22;
    if (timeout_ns >= 0) {
        ts.tv_sec = timeout_ns / 1000000000, ts.tv_nsec = timeout_ns % 1000000000;
        tp = &ts;
    }
    n = kevent(epfd, NULL, 0, ev, max * 2 < 256 ? max * 2 : 256, tp);
    if (n < 0)
        return -linux_error(errno);
    pthread_mutex_lock(&lock);
    for (int i = 0; i < n; i++) {
        int fd = (int)ev[i].ident, j;
        Watch *w = watch_of(e, fd);
        uint32_t bits = 0;

        if (!w)
            continue;
        if (ev[i].flags & EV_ERROR) {
            bits = EPOLLERR;
        } else if (ev[i].filter == EVFILT_READ) {
            bits = EPOLLIN;
            if (ev[i].flags & EV_EOF)
                bits |= EPOLLRDHUP | (ev[i].fflags ? EPOLLERR : 0);
        } else if (ev[i].filter == EVFILT_WRITE) {
            bits = EPOLLOUT;
            if (ev[i].flags & EV_EOF)
                bits |= EPOLLHUP | (ev[i].fflags ? EPOLLERR : 0);
        }
        // a stream closed both ways is a hang-up
        if ((bits & EPOLLRDHUP) && !(w->events & EPOLLOUT))
            bits |= EPOLLHUP;
        bits &= w->events | EPOLLERR | EPOLLHUP;
        if (!bits)
            continue;
        // the read and write filters of one fd are one Linux event
        for (j = 0; j < count && owner[j] != fd; j++) {
        }
        if (j < count) {
            out[j].events |= bits;
        } else if (count < max && count < 256) {
            out[count].events = bits, out[count].data = w->data;
            owner[count++] = fd;
        }
        // one-shot: disabled until rearmed with EPOLL_CTL_MOD, as Linux does
        if (w->events & EPOLLONESHOT)
            watch_apply(epfd, fd, 0, 0);
    }
    pthread_mutex_unlock(&lock);
    return count;
}

// the fd goes: its state, its hidden end, and every epoll's watch on it
void
events_close(int fd)
{
    Special *s;

    pthread_mutex_lock(&lock);
    for (int i = 0; i < MAX_SPECIAL; i++) {
        Special *e = specials[i];
        Watch *w;

        if (!e || e->kind != SPECIAL_EPOLL || !(w = watch_of(e, fd)))
            continue;
        *w = e->watches[--e->nwatches];
    }
    s = special(fd, 0);
    if (s) {
        if (s->kind == SPECIAL_TIMER) {
            struct kevent ev;

            EV_SET(&ev, (uintptr_t)fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
            kevent(timer_queue, &ev, 1, NULL, 0, NULL);
        }
        if (s->other > 0)
            close(s->other);
        free(s->watches);
        free(s);
        specials[fd] = NULL;
    }
    pthread_mutex_unlock(&lock);
}

// select through poll: Linux's fd_set is a bitmap of longs like Darwin's, but holds up to 1024 fds either way
long
events_select(int nfds, uint64_t *rd, uint64_t *wr, uint64_t *ex, int64_t timeout_ns)
{
    struct pollfd fds[1024];
    unsigned n = 0;
    int ready, timeout;

    if (nfds < 0 || nfds > 1024)
        return -22;
    for (int fd = 0; fd < nfds; fd++) {
        uint64_t bit = 1ull << (fd % 64);
        short want = (rd && (rd[fd / 64] & bit) ? POLLIN : 0) | (wr && (wr[fd / 64] & bit) ? POLLOUT : 0) |
                     (ex && (ex[fd / 64] & bit) ? POLLPRI : 0);

        if (want)
            fds[n].fd = fd, fds[n].events = want, fds[n].revents = 0, n++;
    }
    timeout = timeout_ns < 0 ? -1 : timeout_ns / 1000000 > INT32_MAX ? INT32_MAX : (int)((timeout_ns + 999999) / 1000000);
    ready = poll(fds, n, timeout);
    if (ready < 0)
        return -linux_error(errno);
    for (int w = 0; w < (nfds + 63) / 64; w++) {
        if (rd)
            rd[w] = 0;
        if (wr)
            wr[w] = 0;
        if (ex)
            ex[w] = 0;
    }
    ready = 0;
    for (unsigned i = 0; i < n; i++) {
        uint64_t bit = 1ull << (fds[i].fd % 64);
        short got = fds[i].revents;

        if (got & POLLNVAL)
            return -9;
        if (rd && (fds[i].events & POLLIN) && (got & (POLLIN | POLLHUP | POLLERR)))
            rd[fds[i].fd / 64] |= bit, ready++;
        if (wr && (fds[i].events & POLLOUT) && (got & (POLLOUT | POLLERR)))
            wr[fds[i].fd / 64] |= bit, ready++;
        if (ex && (fds[i].events & POLLPRI) && (got & POLLPRI))
            ex[fds[i].fd / 64] |= bit, ready++;
    }
    return ready;
}
