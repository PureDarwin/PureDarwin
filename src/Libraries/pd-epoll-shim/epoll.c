/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
#include "epoll.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/un.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

struct pd_timer {
	int fd, kq;
	// fd is the read end of a pipe, an expiry seen by epoll_wait writes its count to wfd
	int wfd;
	dev_t dev;
	ino_t ino;
	void *udata;
	/* Kept so timerfd_gettime can report what is left: kqueue's EVFILT_TIMER
	 * has no way to read a pending timer back out. */
	struct itimerspec armed;
	struct timespec deadline;
	struct pd_timer *next;
};
struct pd_signal { int read_fd, write_fd, signal_number; struct pd_signal *next; };
static struct pd_timer *timers;
static struct pd_signal *signals[NSIG];

static void
set_nonblock_cloexec(int fd)
{
    int flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    flags = fcntl(fd, F_GETFD);
    fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

// the timer behind fd, if fd is still that timer's pipe: a closed timerfd's number gets reused
static struct pd_timer *
timer_lookup(int fd)
{
    struct pd_timer **link, *timer;
    struct stat st;
    for (link = &timers; (timer = *link) != NULL; link = &timer->next) {
        if (timer->fd != fd) continue;
        if (fstat(fd, &st) == 0 && st.st_dev == timer->dev && st.st_ino == timer->ino)
            return timer;
        *link = timer->next;
        close(timer->wfd);
        free(timer);
        return NULL;
    }
    return NULL;
}

// one shot at the next expiry, epoll_wait re-arms periodic ones
static int
timer_arm(struct pd_timer *timer, int64_t nanoseconds)
{
    struct kevent change;
    if (timer->kq < 0) return 0;
    if (nanoseconds < 1) nanoseconds = 1;
    // xnu's unit flags differ from FreeBSD's, 0x2 there is microseconds, so say nanoseconds outright
    EV_SET(&change, (uintptr_t)timer->fd, EVFILT_TIMER,
           EV_ADD | EV_ENABLE | EV_ONESHOT, NOTE_NSECONDS, nanoseconds, timer);
    return kevent(timer->kq, &change, 1, NULL, 0, NULL);
}

static int64_t
timer_remaining(struct pd_timer *timer)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)(timer->deadline.tv_sec - now.tv_sec) * 1000000000
         + (timer->deadline.tv_nsec - now.tv_nsec);
}

// an expiry: the count goes into the pipe for read(), a periodic timer is armed again
static void
timer_fired(struct pd_timer *timer)
{
    uint64_t count = 1, pending = 0;
    int64_t interval = (int64_t)timer->armed.it_interval.tv_sec * 1000000000
                     + timer->armed.it_interval.tv_nsec;
    int queued = 0;
    // an unread count is folded in, without ever blocking on a blocking timerfd
    if (ioctl(timer->fd, FIONREAD, &queued) == 0 && queued >= (int)sizeof(pending) &&
        read(timer->fd, &pending, sizeof(pending)) == (ssize_t)sizeof(pending))
        count += pending;
    (void)write(timer->wfd, &count, sizeof(count));
    if (interval > 0) {
        int64_t deadline = (int64_t)timer->deadline.tv_sec * 1000000000 + timer->deadline.tv_nsec + interval;
        timer->deadline.tv_sec = deadline / 1000000000;
        timer->deadline.tv_nsec = deadline % 1000000000;
        timer_arm(timer, timer_remaining(timer));
    } else {
        memset(&timer->armed, 0, sizeof(timer->armed));
    }
}

int epoll_create(int size) { (void)size; return kqueue(); }
int
epoll_create1(int flags)
{
    int fd = kqueue();
    if (fd >= 0 && (flags & EPOLL_CLOEXEC)) {
        int old = fcntl(fd, F_GETFD);
        fcntl(fd, F_SETFD, old | FD_CLOEXEC);
    }
    return fd;
}

static int
change_filter(int kq, int fd, int filter, uint16_t flags, void *udata)
{
    struct kevent change;
    EV_SET(&change, (uintptr_t)fd, filter, flags, 0, 0, udata);
    return kevent(kq, &change, 1, NULL, 0, NULL);
}

int
epoll_ctl(int epfd, int operation, int fd, struct epoll_event *event)
{
    struct pd_timer *timer;
    /* No EV_CLEAR: callers use epoll's default level-triggered semantics and
     * handle one item per wakeup rather than draining the fd. Edge-triggered
     * loses everything that arrived before the handler ran - a second client
     * connecting while the first is still queued is never accepted. */
    uint16_t flags = EV_ADD | EV_ENABLE;
    if (operation == EPOLL_CTL_DEL || operation == EPOLL_CTL_MOD) {
        change_filter(epfd, fd, EVFILT_READ, EV_DELETE, NULL);
        change_filter(epfd, fd, EVFILT_WRITE, EV_DELETE, NULL);
        change_filter(epfd, fd, EVFILT_TIMER, EV_DELETE, NULL);
        if (operation == EPOLL_CTL_DEL) {
            if ((timer = timer_lookup(fd)) != NULL && timer->kq == epfd) timer->kq = -1;
            return 0;
        }
    }
    if ((timer = timer_lookup(fd)) != NULL) {
        timer->kq = epfd;
        timer->udata = event->data.ptr;
        // armed before it was added: the kqueue gets it now
        if (timer->armed.it_value.tv_sec || timer->armed.it_value.tv_nsec)
            return timer_arm(timer, timer_remaining(timer));
        return 0;
    }
    if ((event->events & EPOLLIN) &&
        change_filter(epfd, fd, EVFILT_READ, flags, event->data.ptr) < 0)
        return -1;
    if ((event->events & EPOLLOUT) &&
        change_filter(epfd, fd, EVFILT_WRITE, flags, event->data.ptr) < 0)
        return -1;
    return 0;
}

int
epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
    struct kevent ready[64];
    struct timespec ts;
    int count, i;
    if (maxevents <= 0 || maxevents > 64) { errno = EINVAL; return -1; }
    if (timeout < 0) count = kevent(epfd, NULL, 0, ready, maxevents, NULL);
    else {
        ts.tv_sec = timeout / 1000;
        ts.tv_nsec = (timeout % 1000) * 1000000L;
        count = kevent(epfd, NULL, 0, ready, maxevents, &ts);
    }
    if (count < 0) return -1;
    for (i = 0; i < count; i++) {
        if (ready[i].filter == EVFILT_TIMER) {
            struct pd_timer *timer = ready[i].udata;
            timer_fired(timer);
            events[i].events = EPOLLIN;
            events[i].data.ptr = timer->udata;
            continue;
        }
        events[i].events = 0;
        if (ready[i].filter == EVFILT_READ) events[i].events |= EPOLLIN;
        if (ready[i].filter == EVFILT_WRITE) events[i].events |= EPOLLOUT;
        if (ready[i].flags & EV_EOF) events[i].events |= EPOLLHUP;
        if (ready[i].flags & EV_ERROR) events[i].events |= EPOLLERR;
        events[i].data.ptr = ready[i].udata;
    }
    return count;
}

int
timerfd_create(int clockid, int flags)
{
    struct pd_timer *timer;
    struct stat st;
    int fds[2];
    (void)clockid;
    if (pipe(fds) < 0) return -1;
    // the write end never blocks the loop, the read end follows the caller's flags
    fcntl(fds[1], F_SETFL, fcntl(fds[1], F_GETFL) | O_NONBLOCK);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    if (flags & TFD_NONBLOCK) fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    if (flags & TFD_CLOEXEC) fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    timer = calloc(1, sizeof(*timer));
    if (!timer || fstat(fds[0], &st) < 0) {
        free(timer); close(fds[0]); close(fds[1]); errno = ENOMEM; return -1;
    }
    (void)timer_lookup(fds[0]);
    timer->fd = fds[0]; timer->wfd = fds[1]; timer->kq = -1;
    timer->dev = st.st_dev; timer->ino = st.st_ino;
    timer->next = timers; timers = timer;
    return fds[0];
}

int
timerfd_settime(int fd, int flags, const struct itimerspec *value,
                struct itimerspec *old_value)
{
    struct pd_timer *timer;
    struct kevent change;
    struct timespec now;
    int64_t nanoseconds;
    uint64_t stale;
    if ((timer = timer_lookup(fd)) == NULL) { errno = EINVAL; return -1; }
    if (timer->kq >= 0) {
        EV_SET(&change, (uintptr_t)fd, EVFILT_TIMER, EV_DELETE, 0, 0, NULL);
        kevent(timer->kq, &change, 1, NULL, 0, NULL);
    }
    if (old_value != NULL) timerfd_gettime(fd, old_value);
    // a new setting starts the expiration count over, as on linux
    int fl = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    while (read(fd, &stale, sizeof(stale)) > 0);
    fcntl(fd, F_SETFL, fl);
    if (!value->it_value.tv_sec && !value->it_value.tv_nsec) {
        memset(&timer->armed, 0, sizeof(timer->armed));
        return 0;
    }
    clock_gettime(CLOCK_MONOTONIC, &now);
    timer->armed = *value;
    timer->deadline = value->it_value;
    nanoseconds = (int64_t)value->it_value.tv_sec * 1000000000 + value->it_value.tv_nsec;
    if (flags & TFD_TIMER_ABSTIME)
        nanoseconds -= (int64_t)now.tv_sec * 1000000000 + now.tv_nsec;
    else {
        int64_t deadline = (int64_t)now.tv_sec * 1000000000 + now.tv_nsec + nanoseconds;
        timer->deadline.tv_sec = deadline / 1000000000;
        timer->deadline.tv_nsec = deadline % 1000000000;
    }
    return timer_arm(timer, nanoseconds);
}

static void
pd_signal_handler(int signal_number)
{
    struct signalfd_siginfo info = { (uint32_t)signal_number };
    struct pd_signal *signal = signals[signal_number];
    if (signal) (void)write(signal->write_fd, &info, sizeof(info));
}

int
signalfd(int fd, const sigset_t *mask, int flags)
{
    struct pd_signal *signal;
    struct sigaction action;
    int signal_number, pipe_fds[2];
    (void)fd; (void)flags;
    if (!mask) { errno = EINVAL; return -1; }
    for (signal_number = 1; signal_number < NSIG; signal_number++)
        if (sigismember(mask, signal_number) == 1) break;
    if (signal_number == NSIG || pipe(pipe_fds) < 0) return -1;
    set_nonblock_cloexec(pipe_fds[0]); set_nonblock_cloexec(pipe_fds[1]);
    signal = calloc(1, sizeof(*signal));
    if (!signal) { close(pipe_fds[0]); close(pipe_fds[1]); errno = ENOMEM; return -1; }
    signal->read_fd = pipe_fds[0]; signal->write_fd = pipe_fds[1];
    signal->signal_number = signal_number; signal->next = signals[signal_number];
    signals[signal_number] = signal;
    action.sa_handler = pd_signal_handler; sigemptyset(&action.sa_mask); action.sa_flags = 0;
    sigaction(signal_number, &action, NULL);
    return pipe_fds[0];
}

/* kevent() takes no signal mask, so this is the swap-around-the-wait
 * emulation: a signal delivered between the two pthread_sigmask calls is seen
 * late rather than atomically, exactly as pre-syscall glibc and FreeBSD's
 * epoll-shim behave. Callers use it to let SIGCHLD/SIGTERM break the wait,
 * which this still does. */
int
epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout,
            const sigset_t *sigmask)
{
    sigset_t original;
    int ready, saved_errno;

    if (sigmask == NULL)
        return epoll_wait(epfd, events, maxevents, timeout);

    if (pthread_sigmask(SIG_SETMASK, sigmask, &original) != 0)
        return -1;

    ready = epoll_wait(epfd, events, maxevents, timeout);
    saved_errno = errno;

    pthread_sigmask(SIG_SETMASK, &original, NULL);

    errno = saved_errno;
    return ready;
}

/* eventfd is a single descriptor that is written to and then becomes readable
 * on itself. A pipe cannot do that - it has distinct ends - so this uses a
 * self-connected AF_UNIX datagram socket, which delivers a write back to the
 * same descriptor. The socket is bound to a temporary path that is unlinked
 * immediately, since Darwin has no abstract socket namespace.
 *
 * The counter is not summed the way Linux's is: each write is one datagram, so
 * a read returns the most recent value rather than the accumulated total.
 * Callers that use it purely as a wakeup - which is the only use here - cannot
 * tell the difference.
 */
int
eventfd(unsigned int initval, int flags)
{
    struct sockaddr_un addr;
    char path[sizeof(addr.sun_path)];
    const char *tmp = getenv("TMPDIR");
    int fd;

    fd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (fd < 0)
        return -1;

    snprintf(path, sizeof(path), "%s/pd-eventfd-%d-%d",
             tmp != NULL && tmp[0] != '\0' ? tmp : "/tmp", (int)getpid(), fd);

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    unlink(path);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        connect(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        unlink(path);
        close(fd);
        return -1;
    }
    /* The binding survives the name going away, so nothing is left behind. */
    unlink(path);

    if (flags & EFD_NONBLOCK) {
        int fl = fcntl(fd, F_GETFL);
        fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    }
    if (flags & EFD_CLOEXEC) {
        int fl = fcntl(fd, F_GETFD);
        fcntl(fd, F_SETFD, fl | FD_CLOEXEC);
    }

    if (initval > 0) {
        uint64_t value = initval;
        if (write(fd, &value, sizeof(value)) != (ssize_t)sizeof(value)) {
            close(fd);
            return -1;
        }
    }

    return fd;
}

int
eventfd_read(int fd, uint64_t *value)
{
    uint64_t buf;
    if (read(fd, &buf, sizeof(buf)) != (ssize_t)sizeof(buf))
        return -1;
    if (value != NULL)
        *value = buf;
    return 0;
}

int
eventfd_write(int fd, uint64_t value)
{
    return write(fd, &value, sizeof(value)) == (ssize_t)sizeof(value) ? 0 : -1;
}

/* kqueue cannot report a pending EVFILT_TIMER, so this answers from the value
 * recorded at arm time: remaining = deadline - now, floored at zero, which is
 * also what a disarmed or already-expired timer reports. */
int
timerfd_gettime(int fd, struct itimerspec *curr_value)
{
    struct pd_timer *timer;
    struct timespec now;
    int64_t remaining_ns;

    if ((timer = timer_lookup(fd)) == NULL) { errno = EINVAL; return -1; }
    if (curr_value == NULL) { errno = EFAULT; return -1; }

    memset(curr_value, 0, sizeof(*curr_value));
    curr_value->it_interval = timer->armed.it_interval;

    if (!timer->armed.it_value.tv_sec && !timer->armed.it_value.tv_nsec)
        return 0;

    clock_gettime(CLOCK_MONOTONIC, &now);
    remaining_ns = (int64_t)(timer->deadline.tv_sec - now.tv_sec) * 1000000000
                 + (timer->deadline.tv_nsec - now.tv_nsec);
    if (remaining_ns < 0)
        remaining_ns = 0;

    curr_value->it_value.tv_sec = remaining_ns / 1000000000;
    curr_value->it_value.tv_nsec = remaining_ns % 1000000000;
    return 0;
}
