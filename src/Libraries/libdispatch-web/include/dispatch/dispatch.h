// dispatch.h - the libdispatch API for the browser build, single-threaded on the page's event loop
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __DISPATCH_PUBLIC__
#define __DISPATCH_PUBLIC__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#define DISPATCH_API_VERSION 20181008
#define DISPATCH_EXPORT extern
#define DISPATCH_INLINE static inline
#define DISPATCH_NOTHROW
#define DISPATCH_NONNULL_ALL
#define DISPATCH_RETURNS_RETAINED
#define DISPATCH_WARN_RESULT
#define DISPATCH_EXPECT(x, v) (x)

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pd_dispatch_object *dispatch_object_t;
typedef struct pd_dispatch_object *dispatch_queue_t;
typedef struct pd_dispatch_object *dispatch_source_t;
typedef struct pd_dispatch_object *dispatch_semaphore_t;
typedef struct pd_dispatch_object *dispatch_group_t;
typedef struct pd_dispatch_attr *dispatch_queue_attr_t;
typedef const struct pd_dispatch_source_type *dispatch_source_type_t;

typedef void (*dispatch_function_t)(void *);
#ifdef __BLOCKS__
typedef void (^dispatch_block_t)(void);
#endif
typedef long dispatch_once_t;
typedef uint64_t dispatch_time_t;

#define NSEC_PER_SEC 1000000000ull
#define NSEC_PER_MSEC 1000000ull
#define USEC_PER_SEC 1000000ull
#define NSEC_PER_USEC 1000ull

#define DISPATCH_TIME_NOW (0ull)
#define DISPATCH_TIME_FOREVER (~0ull)
#define DISPATCH_WALLTIME_NOW (~1ull)

#define DISPATCH_QUEUE_SERIAL ((dispatch_queue_attr_t)0)
#define DISPATCH_QUEUE_CONCURRENT (&_dispatch_queue_attr_concurrent)
DISPATCH_EXPORT struct pd_dispatch_attr _dispatch_queue_attr_concurrent;

#define DISPATCH_QUEUE_PRIORITY_HIGH 2
#define DISPATCH_QUEUE_PRIORITY_DEFAULT 0
#define DISPATCH_QUEUE_PRIORITY_LOW (-2)
#define DISPATCH_QUEUE_PRIORITY_BACKGROUND (-32768)
#define DISPATCH_QUEUE_OVERCOMMIT 0x2ull
#define DISPATCH_APPLY_AUTO ((dispatch_queue_t)0)
#define DISPATCH_APPLY_CURRENT_ROOT_QUEUE ((dispatch_queue_t)0)
#define DISPATCH_TARGET_QUEUE_DEFAULT ((dispatch_queue_t)0)

#ifndef __QOS_ENUM
typedef unsigned int qos_class_t;
#define QOS_CLASS_USER_INTERACTIVE 0x21
#define QOS_CLASS_USER_INITIATED 0x19
#define QOS_CLASS_DEFAULT 0x15
#define QOS_CLASS_UTILITY 0x11
#define QOS_CLASS_BACKGROUND 0x09
#define QOS_CLASS_UNSPECIFIED 0x00
#endif

DISPATCH_EXPORT const struct pd_dispatch_source_type _dispatch_source_type_timer, _dispatch_source_type_read,
    _dispatch_source_type_write, _dispatch_source_type_signal, _dispatch_source_type_proc,
    _dispatch_source_type_vnode, _dispatch_source_type_data_add, _dispatch_source_type_data_or,
    _dispatch_source_type_mach_send, _dispatch_source_type_mach_recv;
#define DISPATCH_SOURCE_TYPE_TIMER (&_dispatch_source_type_timer)
#define DISPATCH_SOURCE_TYPE_READ (&_dispatch_source_type_read)
#define DISPATCH_SOURCE_TYPE_WRITE (&_dispatch_source_type_write)
#define DISPATCH_SOURCE_TYPE_SIGNAL (&_dispatch_source_type_signal)
#define DISPATCH_SOURCE_TYPE_PROC (&_dispatch_source_type_proc)
#define DISPATCH_SOURCE_TYPE_VNODE (&_dispatch_source_type_vnode)
#define DISPATCH_SOURCE_TYPE_DATA_ADD (&_dispatch_source_type_data_add)
#define DISPATCH_SOURCE_TYPE_DATA_OR (&_dispatch_source_type_data_or)
#define DISPATCH_SOURCE_TYPE_MACH_SEND (&_dispatch_source_type_mach_send)
#define DISPATCH_SOURCE_TYPE_MACH_RECV (&_dispatch_source_type_mach_recv)
#define DISPATCH_MACH_SEND_DEAD 0x1
#define DISPATCH_TIMER_STRICT 0x1

// objects
DISPATCH_EXPORT void dispatch_retain(dispatch_object_t object);
DISPATCH_EXPORT void dispatch_release(dispatch_object_t object);
DISPATCH_EXPORT void dispatch_suspend(dispatch_object_t object);
DISPATCH_EXPORT void dispatch_resume(dispatch_object_t object);
DISPATCH_EXPORT void dispatch_activate(dispatch_object_t object);
DISPATCH_EXPORT void *dispatch_get_context(dispatch_object_t object);
DISPATCH_EXPORT void dispatch_set_context(dispatch_object_t object, void *context);
DISPATCH_EXPORT void dispatch_set_finalizer_f(dispatch_object_t object, dispatch_function_t finalizer);

// queues
DISPATCH_EXPORT dispatch_queue_t dispatch_get_main_queue(void);
DISPATCH_EXPORT dispatch_queue_t dispatch_get_global_queue(intptr_t identifier, uintptr_t flags);
DISPATCH_EXPORT dispatch_queue_t dispatch_queue_create(const char *label, dispatch_queue_attr_t attr);
DISPATCH_EXPORT dispatch_queue_t dispatch_queue_create_with_target(const char *label, dispatch_queue_attr_t attr,
    dispatch_queue_t target);
DISPATCH_EXPORT dispatch_queue_attr_t dispatch_queue_attr_make_with_qos_class(dispatch_queue_attr_t attr,
    qos_class_t qos_class, int relative_priority);
DISPATCH_EXPORT const char *dispatch_queue_get_label(dispatch_queue_t queue);
DISPATCH_EXPORT void dispatch_set_target_queue(dispatch_object_t object, dispatch_queue_t queue);
DISPATCH_EXPORT void dispatch_async_f(dispatch_queue_t queue, void *context, dispatch_function_t work);
DISPATCH_EXPORT void dispatch_sync_f(dispatch_queue_t queue, void *context, dispatch_function_t work);
DISPATCH_EXPORT void dispatch_barrier_async_f(dispatch_queue_t queue, void *context, dispatch_function_t work);
DISPATCH_EXPORT void dispatch_barrier_sync_f(dispatch_queue_t queue, void *context, dispatch_function_t work);
DISPATCH_EXPORT void dispatch_after_f(dispatch_time_t when, dispatch_queue_t queue, void *context, dispatch_function_t work);
DISPATCH_EXPORT void dispatch_apply_f(size_t iterations, dispatch_queue_t queue, void *context,
    void (*work)(void *, size_t));
DISPATCH_EXPORT void dispatch_once_f(dispatch_once_t *predicate, void *context, dispatch_function_t function);
DISPATCH_EXPORT void dispatch_main(void);

// time
DISPATCH_EXPORT dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta);
DISPATCH_EXPORT dispatch_time_t dispatch_walltime(const struct timespec *when, int64_t delta);

// sources
DISPATCH_EXPORT dispatch_source_t dispatch_source_create(dispatch_source_type_t type, uintptr_t handle,
    uintptr_t mask, dispatch_queue_t queue);
DISPATCH_EXPORT void dispatch_source_set_event_handler_f(dispatch_source_t source, dispatch_function_t handler);
DISPATCH_EXPORT void dispatch_source_set_cancel_handler_f(dispatch_source_t source, dispatch_function_t handler);
DISPATCH_EXPORT void dispatch_source_set_registration_handler_f(dispatch_source_t source, dispatch_function_t handler);
DISPATCH_EXPORT void dispatch_source_cancel(dispatch_source_t source);
DISPATCH_EXPORT intptr_t dispatch_source_testcancel(dispatch_source_t source);
DISPATCH_EXPORT uintptr_t dispatch_source_get_handle(dispatch_source_t source);
DISPATCH_EXPORT uintptr_t dispatch_source_get_mask(dispatch_source_t source);
DISPATCH_EXPORT uintptr_t dispatch_source_get_data(dispatch_source_t source);
DISPATCH_EXPORT void dispatch_source_merge_data(dispatch_source_t source, uintptr_t value);
DISPATCH_EXPORT void dispatch_source_set_timer(dispatch_source_t source, dispatch_time_t start, uint64_t interval,
    uint64_t leeway);

// semaphores and groups
DISPATCH_EXPORT dispatch_semaphore_t dispatch_semaphore_create(intptr_t value);
DISPATCH_EXPORT intptr_t dispatch_semaphore_wait(dispatch_semaphore_t semaphore, dispatch_time_t timeout);
DISPATCH_EXPORT intptr_t dispatch_semaphore_signal(dispatch_semaphore_t semaphore);
DISPATCH_EXPORT dispatch_group_t dispatch_group_create(void);
DISPATCH_EXPORT void dispatch_group_enter(dispatch_group_t group);
DISPATCH_EXPORT void dispatch_group_leave(dispatch_group_t group);
DISPATCH_EXPORT intptr_t dispatch_group_wait(dispatch_group_t group, dispatch_time_t timeout);
DISPATCH_EXPORT void dispatch_group_async_f(dispatch_group_t group, dispatch_queue_t queue, void *context,
    dispatch_function_t work);
DISPATCH_EXPORT void dispatch_group_notify_f(dispatch_group_t group, dispatch_queue_t queue, void *context,
    dispatch_function_t work);

#ifdef __BLOCKS__
DISPATCH_EXPORT void dispatch_async(dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_sync(dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_barrier_async(dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_barrier_sync(dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_after(dispatch_time_t when, dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_apply(size_t iterations, dispatch_queue_t queue, void (^block)(size_t));
DISPATCH_EXPORT void dispatch_once(dispatch_once_t *predicate, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_source_set_event_handler(dispatch_source_t source, dispatch_block_t handler);
DISPATCH_EXPORT void dispatch_source_set_cancel_handler(dispatch_source_t source, dispatch_block_t handler);
DISPATCH_EXPORT void dispatch_source_set_registration_handler(dispatch_source_t source, dispatch_block_t handler);
DISPATCH_EXPORT void dispatch_group_async(dispatch_group_t group, dispatch_queue_t queue, dispatch_block_t block);
DISPATCH_EXPORT void dispatch_group_notify(dispatch_group_t group, dispatch_queue_t queue, dispatch_block_t block);
#endif

#ifdef __cplusplus
}
#endif

#endif
