// dispatch_web.c - libdispatch on the browser's event loop: one thread, queues drained by scheduled callbacks
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#include <dispatch/dispatch.h>
#include <dispatch/private.h>

#include <Block.h>
#include <emscripten.h>
#include <emscripten/eventloop.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { KIND_QUEUE = 1, KIND_SOURCE, KIND_SEMAPHORE, KIND_GROUP };

struct item {
    struct item *next;
    dispatch_function_t work;
    void *context;
};

struct pd_dispatch_attr {
    int concurrent;
};
struct pd_dispatch_attr _dispatch_queue_attr_concurrent = { 1 };

struct pd_dispatch_source_type {
    int timer;
};
const struct pd_dispatch_source_type _dispatch_source_type_timer = { 1 };
const struct pd_dispatch_source_type _dispatch_source_type_read, _dispatch_source_type_write,
    _dispatch_source_type_signal, _dispatch_source_type_proc, _dispatch_source_type_vnode,
    _dispatch_source_type_data_add, _dispatch_source_type_data_or, _dispatch_source_type_mach_send,
    _dispatch_source_type_mach_recv;

struct pd_dispatch_object {
    int kind;
    int refs;
    int suspended;
    void *context;
    dispatch_function_t finalizer;
    // queues
    const char *label;
    struct item *head, *tail;
    int scheduled;
    int runloop; // drained by CoreFoundation, never by the event loop
    // sources
    dispatch_source_type_t type;
    dispatch_queue_t target;
    uintptr_t handle, mask, data;
    dispatch_function_t event, cancel;
    int event_block, cancel_block;
    int cancelled;
    long timeout_id;
    uint64_t start, interval;
    // semaphores and groups
    intptr_t count;
    struct notify *notify;
};

// work waiting on a group, sent to its queue when the group empties
struct notify {
    struct notify *next;
    struct pd_dispatch_object *q;
    void *context;
    dispatch_function_t work;
};

// CoreFoundation's run loop waits on these handles as descriptors, each one ready while its queue has work
#define HANDLE_BASE 0x7f000000
#define MAX_RUNLOOP_QUEUES 256
static struct pd_dispatch_object *runloop_queues[MAX_RUNLOOP_QUEUES];

static struct pd_dispatch_object main_q = { .kind = KIND_QUEUE, .refs = 1, .label = "com.apple.main-thread" };
static struct pd_dispatch_object global_q = { .kind = KIND_QUEUE, .refs = 1, .label = "com.apple.root.default-qos" };

static struct pd_dispatch_object *new_object(int kind)
{
    struct pd_dispatch_object *o = calloc(1, sizeof(*o));
    o->kind = kind;
    o->refs = 1;
    return o;
}

static void drain(struct pd_dispatch_object *q);

static void scheduled_drain(void *q)
{
    ((struct pd_dispatch_object *)q)->scheduled = 0;
    drain(q);
}

// the event loop drains a queue soon after work arrives, CoreFoundation pulls its own
static void schedule(struct pd_dispatch_object *q)
{
    if (q->scheduled || q->suspended || q->runloop || q->head == NULL)
        return;
    q->scheduled = 1;
    emscripten_async_call(scheduled_drain, q, 0);
}

static void push(struct pd_dispatch_object *q, void *context, dispatch_function_t work)
{
    struct item *it = malloc(sizeof(*it));
    it->next = NULL;
    it->work = work;
    it->context = context;
    if (q->tail)
        q->tail->next = it;
    else
        q->head = it;
    q->tail = it;
    schedule(q);
}

static int run_one(struct pd_dispatch_object *q)
{
    struct item *it = q->head;
    if (it == NULL)
        return 0;
    q->head = it->next;
    if (q->head == NULL)
        q->tail = NULL;
    it->work(it->context);
    free(it);
    return 1;
}

static void drain(struct pd_dispatch_object *q)
{
    while (!q->suspended && run_one(q))
        ;
}

static struct pd_dispatch_object *queue_or_default(dispatch_queue_t q)
{
    return q ? q : &global_q;
}

// blocks travel as a copied block and a trampoline that runs then releases it
static void run_block(void *block)
{
    ((dispatch_block_t)block)();
    Block_release(block);
}

void dispatch_retain(dispatch_object_t o)
{
    if (o && o != &main_q && o != &global_q)
        o->refs++;
}

void dispatch_release(dispatch_object_t o)
{
    if (o == NULL || o == &main_q || o == &global_q || --o->refs > 0)
        return;
    if (o->finalizer)
        o->finalizer(o->context);
    if (o->runloop && o->handle > HANDLE_BASE)
        runloop_queues[o->handle - HANDLE_BASE - 1] = NULL;
    // a queue with work left keeps running it, its memory is the one thing reclaimed late
    if (o->head == NULL)
        free(o);
}

void dispatch_suspend(dispatch_object_t o)
{
    o->suspended++;
}

static void arm_timer(struct pd_dispatch_object *s);

void dispatch_resume(dispatch_object_t o)
{
    if (o->suspended > 0 && --o->suspended == 0) {
        if (o->kind == KIND_QUEUE)
            schedule(o);
        else if (o->kind == KIND_SOURCE && o->type == DISPATCH_SOURCE_TYPE_TIMER)
            arm_timer(o);
    }
}

void dispatch_activate(dispatch_object_t o)
{
    dispatch_resume(o);
}

void *dispatch_get_context(dispatch_object_t o)
{
    return o->context;
}

void dispatch_set_context(dispatch_object_t o, void *context)
{
    o->context = context;
}

void dispatch_set_finalizer_f(dispatch_object_t o, dispatch_function_t finalizer)
{
    o->finalizer = finalizer;
}

dispatch_queue_t dispatch_get_main_queue(void)
{
    return &main_q;
}

dispatch_queue_t dispatch_get_global_queue(intptr_t identifier, uintptr_t flags)
{
    (void)identifier;
    (void)flags;
    return &global_q;
}

dispatch_queue_t dispatch_queue_create(const char *label, dispatch_queue_attr_t attr)
{
    (void)attr;
    struct pd_dispatch_object *q = new_object(KIND_QUEUE);
    q->label = label ? strdup(label) : "";
    return q;
}

dispatch_queue_t dispatch_queue_create_with_target(const char *label, dispatch_queue_attr_t attr, dispatch_queue_t target)
{
    (void)target;
    return dispatch_queue_create(label, attr);
}

dispatch_queue_attr_t dispatch_queue_attr_make_with_qos_class(dispatch_queue_attr_t attr, qos_class_t qos, int prio)
{
    (void)qos;
    (void)prio;
    return attr;
}

const char *dispatch_queue_get_label(dispatch_queue_t q)
{
    return q ? q->label : main_q.label;
}

void dispatch_set_target_queue(dispatch_object_t o, dispatch_queue_t q)
{
    if (o->kind == KIND_SOURCE)
        o->target = q;
}

void dispatch_async_f(dispatch_queue_t q, void *context, dispatch_function_t work)
{
    push(queue_or_default(q), context, work);
}

// one thread: run what is already queued first, so the queue keeps its order, then the work itself
void dispatch_sync_f(dispatch_queue_t q, void *context, dispatch_function_t work)
{
    struct pd_dispatch_object *queue = queue_or_default(q);
    if (!queue->runloop)
        drain(queue);
    work(context);
}

void dispatch_barrier_async_f(dispatch_queue_t q, void *context, dispatch_function_t work)
{
    dispatch_async_f(q, context, work);
}

void dispatch_barrier_sync_f(dispatch_queue_t q, void *context, dispatch_function_t work)
{
    dispatch_sync_f(q, context, work);
}

struct after {
    struct pd_dispatch_object *q;
    void *context;
    dispatch_function_t work;
};

static void after_fire(void *p)
{
    struct after *a = p;
    push(a->q, a->context, a->work);
    free(a);
}

static double ms_until(dispatch_time_t when);

void dispatch_after_f(dispatch_time_t when, dispatch_queue_t q, void *context, dispatch_function_t work)
{
    if (when == DISPATCH_TIME_FOREVER)
        return;
    struct after *a = malloc(sizeof(*a));
    a->q = queue_or_default(q);
    a->context = context;
    a->work = work;
    emscripten_set_timeout(after_fire, ms_until(when), a);
}

void dispatch_apply_f(size_t n, dispatch_queue_t q, void *context, void (*work)(void *, size_t))
{
    (void)q;
    for (size_t i = 0; i < n; i++)
        work(context, i);
}

void dispatch_once_f(dispatch_once_t *predicate, void *context, dispatch_function_t function)
{
    if (*predicate == ~0l)
        return;
    *predicate = ~0l;
    function(context);
}

void dispatch_main(void)
{
    emscripten_exit_with_live_runtime();
}

// time is nanoseconds on the page's monotonic clock, emscripten_get_now counts milliseconds
static uint64_t now_ns(void)
{
    return (uint64_t)(emscripten_get_now() * 1e6);
}

dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta)
{
    if (when == DISPATCH_TIME_FOREVER)
        return when;
    uint64_t base = when == DISPATCH_TIME_NOW ? now_ns() : when;
    return delta < 0 && (uint64_t)-delta > base ? 1 : base + delta;
}

dispatch_time_t dispatch_walltime(const struct timespec *when, int64_t delta)
{
    (void)when;
    return dispatch_time(DISPATCH_TIME_NOW, delta);
}

static double ms_until(dispatch_time_t when)
{
    uint64_t now = now_ns();
    return when <= now ? 0 : (double)(when - now) / 1e6;
}

dispatch_source_t dispatch_source_create(dispatch_source_type_t type, uintptr_t handle, uintptr_t mask, dispatch_queue_t q)
{
    struct pd_dispatch_object *s = new_object(KIND_SOURCE);
    s->type = type;
    s->handle = handle;
    s->mask = mask;
    s->target = queue_or_default(q);
    // sources start suspended, a resume arms them
    s->suspended = 1;
    s->timeout_id = -1;
    return s;
}

static void set_handler(dispatch_function_t *slot, int *is_block, dispatch_function_t f, int block)
{
    if (*is_block && *slot)
        Block_release((void *)*slot);
    *slot = f;
    *is_block = block;
}

void dispatch_source_set_event_handler_f(dispatch_source_t s, dispatch_function_t handler)
{
    set_handler(&s->event, &s->event_block, handler, 0);
}

void dispatch_source_set_cancel_handler_f(dispatch_source_t s, dispatch_function_t handler)
{
    set_handler(&s->cancel, &s->cancel_block, handler, 0);
}

void dispatch_source_set_registration_handler_f(dispatch_source_t s, dispatch_function_t handler)
{
    (void)s;
    (void)handler;
}

static void call_handler(dispatch_function_t f, int is_block, void *context)
{
    if (f == NULL)
        return;
    if (is_block)
        ((dispatch_block_t)(void *)f)();
    else
        f(context);
}

static void source_event(void *p)
{
    struct pd_dispatch_object *s = p;
    if (!s->cancelled)
        call_handler(s->event, s->event_block, s->context);
}

static void source_cancelled(void *p)
{
    struct pd_dispatch_object *s = p;
    call_handler(s->cancel, s->cancel_block, s->context);
}

static void timer_fire(void *p)
{
    struct pd_dispatch_object *s = p;
    s->timeout_id = -1;
    if (s->cancelled || s->suspended)
        return;
    s->data++;
    push(s->target, s, source_event);
    if (s->interval != 0 && s->interval != DISPATCH_TIME_FOREVER)
        s->timeout_id = emscripten_set_timeout(timer_fire, (double)s->interval / 1e6, s);
}

static void arm_timer(struct pd_dispatch_object *s)
{
    if (s->timeout_id >= 0) {
        emscripten_clear_timeout(s->timeout_id);
        s->timeout_id = -1;
    }
    if (s->cancelled || s->suspended || s->start == DISPATCH_TIME_FOREVER)
        return;
    s->timeout_id = emscripten_set_timeout(timer_fire, ms_until(s->start), s);
}

void dispatch_source_set_timer(dispatch_source_t s, dispatch_time_t start, uint64_t interval, uint64_t leeway)
{
    (void)leeway;
    s->start = start == DISPATCH_TIME_NOW ? now_ns() : start;
    s->interval = interval;
    arm_timer(s);
}

void dispatch_source_cancel(dispatch_source_t s)
{
    if (s->cancelled)
        return;
    s->cancelled = 1;
    if (s->timeout_id >= 0) {
        emscripten_clear_timeout(s->timeout_id);
        s->timeout_id = -1;
    }
    push(s->target, s, source_cancelled);
}

intptr_t dispatch_source_testcancel(dispatch_source_t s)
{
    return s->cancelled;
}

uintptr_t dispatch_source_get_handle(dispatch_source_t s)
{
    return s->handle;
}

uintptr_t dispatch_source_get_mask(dispatch_source_t s)
{
    return s->mask;
}

uintptr_t dispatch_source_get_data(dispatch_source_t s)
{
    uintptr_t d = s->data;
    s->data = 0;
    return d;
}

void dispatch_source_merge_data(dispatch_source_t s, uintptr_t value)
{
    s->data = s->type == DISPATCH_SOURCE_TYPE_DATA_OR ? (s->data | value) : (s->data + value);
    if (!s->cancelled && !s->suspended)
        push(s->target, s, source_event);
}

dispatch_semaphore_t dispatch_semaphore_create(intptr_t value)
{
    struct pd_dispatch_object *s = new_object(KIND_SEMAPHORE);
    s->count = value;
    return s;
}

// nothing else runs while this one thread waits, so a wait that cannot pass at once times out
intptr_t dispatch_semaphore_wait(dispatch_semaphore_t s, dispatch_time_t timeout)
{
    if (s->count > 0) {
        s->count--;
        return 0;
    }
    if (timeout == DISPATCH_TIME_FOREVER)
        fprintf(stderr, "dispatch: a forever wait on a semaphore no one else can signal, giving up\n");
    return 1;
}

intptr_t dispatch_semaphore_signal(dispatch_semaphore_t s)
{
    s->count++;
    return 0;
}

dispatch_group_t dispatch_group_create(void)
{
    return new_object(KIND_GROUP);
}

void dispatch_group_enter(dispatch_group_t g)
{
    g->count++;
}

void dispatch_group_leave(dispatch_group_t g)
{
    if (--g->count > 0)
        return;
    struct notify *n = g->notify;
    g->notify = NULL;
    while (n) {
        struct notify *next = n->next;
        push(n->q, n->context, n->work);
        free(n);
        n = next;
    }
}

intptr_t dispatch_group_wait(dispatch_group_t g, dispatch_time_t timeout)
{
    (void)timeout;
    return g->count > 0;
}

struct group_work {
    dispatch_group_t g;
    void *context;
    dispatch_function_t work;
};

static void group_run(void *p)
{
    struct group_work *w = p;
    w->work(w->context);
    dispatch_group_leave(w->g);
    free(w);
}

void dispatch_group_async_f(dispatch_group_t g, dispatch_queue_t q, void *context, dispatch_function_t work)
{
    struct group_work *w = malloc(sizeof(*w));
    w->g = g;
    w->context = context;
    w->work = work;
    dispatch_group_enter(g);
    dispatch_async_f(q, w, group_run);
}

void dispatch_group_notify_f(dispatch_group_t g, dispatch_queue_t q, void *context, dispatch_function_t work)
{
    if (g->count <= 0) {
        dispatch_async_f(q, context, work);
        return;
    }
    struct notify *n = malloc(sizeof(*n));
    n->q = queue_or_default(q);
    n->context = context;
    n->work = work;
    n->next = g->notify;
    g->notify = n;
}

#ifdef __BLOCKS__
void dispatch_async(dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_async_f(q, Block_copy(block), run_block);
}

void dispatch_sync(dispatch_queue_t q, dispatch_block_t block)
{
    struct pd_dispatch_object *queue = queue_or_default(q);
    if (!queue->runloop)
        drain(queue);
    block();
}

void dispatch_barrier_async(dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_async(q, block);
}

void dispatch_barrier_sync(dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_sync(q, block);
}

void dispatch_after(dispatch_time_t when, dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_after_f(when, q, Block_copy(block), run_block);
}

void dispatch_apply(size_t n, dispatch_queue_t q, void (^block)(size_t))
{
    (void)q;
    for (size_t i = 0; i < n; i++)
        block(i);
}

void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block)
{
    if (*predicate == ~0l)
        return;
    *predicate = ~0l;
    block();
}

void dispatch_source_set_event_handler(dispatch_source_t s, dispatch_block_t handler)
{
    set_handler(&s->event, &s->event_block, handler ? (dispatch_function_t)(void *)Block_copy(handler) : NULL, 1);
}

void dispatch_source_set_cancel_handler(dispatch_source_t s, dispatch_block_t handler)
{
    set_handler(&s->cancel, &s->cancel_block, handler ? (dispatch_function_t)(void *)Block_copy(handler) : NULL, 1);
}

void dispatch_source_set_registration_handler(dispatch_source_t s, dispatch_block_t handler)
{
    (void)s;
    (void)handler;
}

void dispatch_group_async(dispatch_group_t g, dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_group_async_f(g, q, Block_copy(block), run_block);
}

void dispatch_group_notify(dispatch_group_t g, dispatch_queue_t q, dispatch_block_t block)
{
    dispatch_group_notify_f(g, q, Block_copy(block), run_block);
}
#endif

bool _dispatch_web_handle_ready(int handle)
{
    int i = handle - HANDLE_BASE;
    if (i == 0)
        return main_q.head != NULL;
    return i > 0 && i <= MAX_RUNLOOP_QUEUES && runloop_queues[i - 1] && runloop_queues[i - 1]->head != NULL;
}

dispatch_runloop_handle_t _dispatch_get_main_queue_port_4CF(void)
{
    return HANDLE_BASE;
}

void _dispatch_main_queue_callback_4CF(void *msg)
{
    (void)msg;
    drain(&main_q);
}

dispatch_queue_t _dispatch_runloop_root_queue_create_4CF(const char *label, unsigned long flags)
{
    (void)flags;
    struct pd_dispatch_object *q = dispatch_queue_create(label, NULL);
    q->runloop = 1;
    for (int i = 0; i < MAX_RUNLOOP_QUEUES; i++) {
        if (runloop_queues[i] == NULL) {
            runloop_queues[i] = q;
            q->handle = HANDLE_BASE + 1 + i;
            break;
        }
    }
    return q;
}

mach_port_t _dispatch_runloop_root_queue_get_port_4CF(dispatch_queue_t q)
{
    return q->handle;
}

bool _dispatch_runloop_root_queue_perform_4CF(dispatch_queue_t q)
{
    run_one(q);
    return q->head != NULL;
}

void _dispatch_runloop_root_queue_wakeup_4CF(dispatch_queue_t q)
{
    (void)q;
}

void _dispatch_source_set_runloop_timer_4CF(dispatch_source_t s, dispatch_time_t start, uint64_t interval, uint64_t leeway)
{
    dispatch_source_set_timer(s, start, interval, leeway);
}
