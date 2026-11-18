// web_compat.c - the Darwin and glibc calls the frameworks expect, for one process on one thread in a page
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <execinfo.h>
#include <notify.h>

#include <Block.h>
#include <stdlib.h>
#include <string.h>
#include <sys/random.h>
#include <sys/sysinfo.h>
#include <uuid/uuid.h>
#include <emscripten/heap.h>

int backtrace(void **frames, int max)
{
    (void)frames;
    (void)max;
    return 0;
}

char **backtrace_symbols(void *const *frames, int count)
{
    (void)frames;
    (void)count;
    // callers free the array, so it is a real allocation even when empty
    return calloc(1, sizeof(char *));
}

void backtrace_symbols_fd(void *const *frames, int count, int fd)
{
    (void)frames;
    (void)count;
    (void)fd;
}

// uuid(3) from the browser's random source. A time-based UUID would carry no host identity here, so both are random
void uuid_generate_random(uuid_t out)
{
    getentropy(out, sizeof(uuid_t));
    out[6] = (out[6] & 0x0f) | 0x40; // version 4
    out[8] = (out[8] & 0x3f) | 0x80; // RFC 4122 variant
}

void uuid_generate_time(uuid_t out)
{
    uuid_generate_random(out);
}

void uuid_generate(uuid_t out)
{
    uuid_generate_random(out);
}

// sysinfo(2): the page's memory is the wasm heap and how far it may grow
int sysinfo(struct sysinfo *info)
{
    memset(info, 0, sizeof(*info));
    info->totalram = emscripten_get_heap_max();
    info->freeram = emscripten_get_heap_max() - emscripten_get_heap_size();
    info->mem_unit = 1;
    info->procs = 1;
    return 0;
}

// notify(3) inside the page: a name table, a state per name, and the handlers registered on each
struct registration {
    int token;
    char *name;
    int posted; // for notify_check
    dispatch_queue_t queue;
    notify_handler_t handler;
};

struct name_state {
    char *name;
    uint64_t state;
};

static struct registration *regs;
static int nregs, next_token = 1;
static struct name_state *names;
static int nnames;

static struct name_state *state_for(const char *name)
{
    for (int i = 0; i < nnames; i++)
        if (strcmp(names[i].name, name) == 0)
            return &names[i];
    names = realloc(names, (nnames + 1) * sizeof(*names));
    names[nnames].name = strdup(name);
    names[nnames].state = 0;
    return &names[nnames++];
}

static struct registration *reg_for(int token)
{
    for (int i = 0; i < nregs; i++)
        if (regs[i].token == token)
            return &regs[i];
    return NULL;
}

static int add_reg(const char *name, dispatch_queue_t queue, notify_handler_t handler)
{
    regs = realloc(regs, (nregs + 1) * sizeof(*regs));
    struct registration *r = &regs[nregs++];
    r->token = next_token++;
    r->name = strdup(name);
    r->posted = 1; // the first check reports a change, as Darwin's does
    r->queue = queue;
    r->handler = handler ? Block_copy(handler) : NULL;
    return r->token;
}

uint32_t notify_post(const char *name)
{
    for (int i = 0; i < nregs; i++) {
        if (strcmp(regs[i].name, name) != 0)
            continue;
        regs[i].posted = 1;
        if (regs[i].handler) {
            int token = regs[i].token;
            notify_handler_t handler = regs[i].handler;
            dispatch_async(regs[i].queue, ^{ handler(token); });
        }
    }
    return NOTIFY_STATUS_OK;
}

uint32_t notify_register_check(const char *name, int *out_token)
{
    *out_token = add_reg(name, NULL, NULL);
    return NOTIFY_STATUS_OK;
}

uint32_t notify_register_dispatch(const char *name, int *out_token, dispatch_queue_t queue, notify_handler_t handler)
{
    *out_token = add_reg(name, queue, handler);
    return NOTIFY_STATUS_OK;
}

uint32_t notify_register_file_descriptor(const char *name, int *notify_fd, int flags, int *out_token)
{
    (void)name;
    (void)notify_fd;
    (void)flags;
    (void)out_token;
    return NOTIFY_STATUS_FAILED;
}

uint32_t notify_check(int token, int *check)
{
    struct registration *r = reg_for(token);
    if (r == NULL)
        return NOTIFY_STATUS_INVALID_TOKEN;
    *check = r->posted;
    r->posted = 0;
    return NOTIFY_STATUS_OK;
}

uint32_t notify_get_state(int token, uint64_t *state)
{
    struct registration *r = reg_for(token);
    if (r == NULL)
        return NOTIFY_STATUS_INVALID_TOKEN;
    *state = state_for(r->name)->state;
    return NOTIFY_STATUS_OK;
}

uint32_t notify_set_state(int token, uint64_t state)
{
    struct registration *r = reg_for(token);
    if (r == NULL)
        return NOTIFY_STATUS_INVALID_TOKEN;
    state_for(r->name)->state = state;
    return NOTIFY_STATUS_OK;
}

uint32_t notify_cancel(int token)
{
    for (int i = 0; i < nregs; i++) {
        if (regs[i].token != token)
            continue;
        free(regs[i].name);
        if (regs[i].handler)
            Block_release(regs[i].handler);
        regs[i] = regs[--nregs];
        return NOTIFY_STATUS_OK;
    }
    return NOTIFY_STATUS_INVALID_TOKEN;
}
