#pragma once
// linux stand-in: dispatch_apply_f over a thread pool, async runs inline (linux/darwin.c)
#include <stddef.h>
#define QOS_CLASS_USER_INTERACTIVE 0x21
typedef void *dispatch_queue_t;
dispatch_queue_t dispatch_get_global_queue(long id, unsigned long flags);
void dispatch_apply_f(size_t n, dispatch_queue_t q, void *ctx, void (*fn)(void *, size_t));
dispatch_queue_t dispatch_queue_create(const char *label, void *attr);
void dispatch_async_f(dispatch_queue_t q, void *ctx, void (*fn)(void *));
