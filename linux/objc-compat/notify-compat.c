// notify-compat.c - notify(3) for Argon on Linux, across processes
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// each name is a file in $XDG_RUNTIME_DIR/argon-notify holding a generation and the state, replaced atomically
// an inotify thread per process wakes the registrations, by file descriptor or dispatch handler

#define _GNU_SOURCE
#include <notify.h>
#include <objc/blocks_runtime.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

struct argon_notify_record { uint64_t generation; uint64_t state; };

struct argon_notify_reg {
    char *file;
    int token;
    uint64_t seen;
    int write_fd;
    dispatch_queue_t queue;
    notify_handler_t handler;
    struct argon_notify_reg *next;
};

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static struct argon_notify_reg *regs;
static int next_token = 1;
static pthread_once_t watcher_once = PTHREAD_ONCE_INIT;

static const char *notify_dir(void)
{
    static char dir[PATH_MAX];

    if (dir[0] == '\0') {
        const char *runtime = getenv("XDG_RUNTIME_DIR");
        if (runtime && *runtime)
            snprintf(dir, sizeof(dir), "%s/argon-notify", runtime);
        else
            snprintf(dir, sizeof(dir), "/tmp/argon-notify-%u", (unsigned)getuid());
        mkdir(dir, 0700);
    }
    return dir;
}

// a name as a file name, slashes are the only character a file name cannot hold
static char *file_for(const char *name)
{
    char *file = strdup(name);

    for (char *c = file; *c; c++) {
        if (*c == '/')
            *c = '_';
    }
    return file;
}

static struct argon_notify_record read_record(const char *file)
{
    struct argon_notify_record record = { 0, 0 };
    char path[PATH_MAX];
    int fd;

    snprintf(path, sizeof(path), "%s/%s", notify_dir(), file);
    fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        if (read(fd, &record, sizeof(record)) != sizeof(record))
            memset(&record, 0, sizeof(record));
        close(fd);
    }
    return record;
}

// read, change and atomically replace a name's record, serialised by a lock file
static void update_record(const char *file, int post, int set, uint64_t state)
{
    char path[PATH_MAX], tmp[PATH_MAX], lockpath[PATH_MAX];
    int lockfd, fd;

    snprintf(path, sizeof(path), "%s/%s", notify_dir(), file);
    snprintf(tmp, sizeof(tmp), "%s/.%s.%d", notify_dir(), file, (int)getpid());
    snprintf(lockpath, sizeof(lockpath), "%s/.lock", notify_dir());
    lockfd = open(lockpath, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (lockfd >= 0)
        flock(lockfd, LOCK_EX);
    struct argon_notify_record record = read_record(file);
    if (post)
        record.generation++;
    if (set)
        record.state = state;
    fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd >= 0) {
        if (write(fd, &record, sizeof(record)) == sizeof(record))
            rename(tmp, path);
        else
            unlink(tmp);
        close(fd);
    }
    if (lockfd >= 0) {
        flock(lockfd, LOCK_UN);
        close(lockfd);
    }
}

// wakes every registration whose name just changed generation
static void deliver(const char *file)
{
    struct argon_notify_record record = read_record(file);

    pthread_mutex_lock(&lock);
    for (struct argon_notify_reg *r = regs; r; r = r->next) {
        if (strcmp(r->file, file) != 0 || r->seen == record.generation)
            continue;
        r->seen = record.generation;
        if (r->write_fd >= 0) {
            uint32_t token = htonl((uint32_t)r->token);
            (void)write(r->write_fd, &token, sizeof(token));
        }
        if (r->handler) {
            notify_handler_t handler = r->handler;
            int token = r->token;
            dispatch_async(r->queue ? r->queue : dispatch_get_main_queue(), ^{ handler(token); });
        }
    }
    pthread_mutex_unlock(&lock);
}

static void *watch(void *unused)
{
    (void)unused;
    int fd = inotify_init1(IN_CLOEXEC);
    char buffer[4096] __attribute__((aligned(__alignof__(struct inotify_event))));

    if (fd < 0 || inotify_add_watch(fd, notify_dir(), IN_MOVED_TO | IN_CLOSE_WRITE) < 0)
        return NULL;
    for (;;) {
        ssize_t n = read(fd, buffer, sizeof(buffer));
        if (n <= 0) {
            if (n < 0 && errno == EINTR)
                continue;
            break;
        }
        for (char *at = buffer; at < buffer + n;) {
            struct inotify_event *event = (struct inotify_event *)at;
            if (event->len && event->name[0] != '.')
                deliver(event->name);
            at += sizeof(*event) + event->len;
        }
    }
    close(fd);
    return NULL;
}

static void start_watcher(void)
{
    pthread_t thread;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&thread, &attr, watch, NULL);
    pthread_attr_destroy(&attr);
}

static struct argon_notify_reg *reg_for(int token)
{
    for (struct argon_notify_reg *r = regs; r; r = r->next) {
        if (r->token == token)
            return r;
    }
    return NULL;
}

static uint32_t add_reg(const char *name, int *out_token, int write_fd, dispatch_queue_t queue, notify_handler_t handler)
{
    if (name == NULL || out_token == NULL)
        return NOTIFY_STATUS_INVALID_NAME;
    struct argon_notify_reg *r = calloc(1, sizeof(*r));
    r->file = file_for(name);
    r->seen = read_record(r->file).generation;
    r->write_fd = write_fd;
    r->queue = queue;
    r->handler = handler ? Block_copy(handler) : NULL;
    pthread_mutex_lock(&lock);
    r->token = next_token++;
    r->next = regs;
    regs = r;
    *out_token = r->token;
    pthread_mutex_unlock(&lock);
    if (write_fd >= 0 || handler)
        pthread_once(&watcher_once, start_watcher);
    return NOTIFY_STATUS_OK;
}

uint32_t notify_register_check(const char *name, int *out_token) { return add_reg(name, out_token, -1, NULL, NULL); }

uint32_t notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, notify_handler_t handler)
{
    return add_reg(name, out_token, -1, queue, handler);
}

// the reading end gets each posting's token in network byte order, as on Darwin
uint32_t notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token)
{
    int fds[2];

    (void)flags;
    if (notify_fd == NULL || pipe2(fds, O_CLOEXEC) != 0)
        return NOTIFY_STATUS_FAILED;
    uint32_t status = add_reg(name, out_token, fds[1], NULL, NULL);
    if (status != NOTIFY_STATUS_OK) {
        close(fds[0]);
        close(fds[1]);
        return status;
    }
    *notify_fd = fds[0];
    return NOTIFY_STATUS_OK;
}

uint32_t notify_post(const char *name)
{
    if (name == NULL)
        return NOTIFY_STATUS_INVALID_NAME;
    char *file = file_for(name);
    update_record(file, 1, 0, 0);
    free(file);
    return NOTIFY_STATUS_OK;
}

uint32_t notify_check(int token, int *check)
{
    pthread_mutex_lock(&lock);
    struct argon_notify_reg *r = reg_for(token);
    uint32_t status = NOTIFY_STATUS_INVALID_TOKEN;
    if (r) {
        uint64_t generation = read_record(r->file).generation;
        if (check)
            *check = generation != r->seen;
        r->seen = generation;
        status = NOTIFY_STATUS_OK;
    }
    pthread_mutex_unlock(&lock);
    return status;
}

uint32_t notify_get_state(int token, uint64_t *state)
{
    pthread_mutex_lock(&lock);
    struct argon_notify_reg *r = reg_for(token);
    if (r && state)
        *state = read_record(r->file).state;
    pthread_mutex_unlock(&lock);
    return r ? NOTIFY_STATUS_OK : NOTIFY_STATUS_INVALID_TOKEN;
}

uint32_t notify_set_state(int token, uint64_t state)
{
    pthread_mutex_lock(&lock);
    struct argon_notify_reg *r = reg_for(token);
    char *file = r ? strdup(r->file) : NULL;
    pthread_mutex_unlock(&lock);
    if (file == NULL)
        return NOTIFY_STATUS_INVALID_TOKEN;
    update_record(file, 0, 1, state);
    free(file);
    return NOTIFY_STATUS_OK;
}

uint32_t notify_cancel(int token)
{
    pthread_mutex_lock(&lock);
    for (struct argon_notify_reg **at = &regs; *at; at = &(*at)->next) {
        if ((*at)->token == token) {
            struct argon_notify_reg *r = *at;
            *at = r->next;
            if (r->handler)
                Block_release(r->handler);
            if (r->write_fd >= 0)
                close(r->write_fd);
            free(r->file);
            free(r);
            break;
        }
    }
    pthread_mutex_unlock(&lock);
    return NOTIFY_STATUS_OK;
}
