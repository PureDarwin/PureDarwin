// private.h - the libdispatch hooks CoreFoundation's run loop drives the main and mode queues through
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#ifndef __DISPATCH_PRIVATE__
#define __DISPATCH_PRIVATE__

#include <dispatch/dispatch.h>

#ifdef __cplusplus
extern "C" {
#endif

// there is no port to wait on in a page, the handle only has to compare equal to itself
typedef int dispatch_runloop_handle_t;
#ifndef __MACH_PORT_T_DEFINED
#define __MACH_PORT_T_DEFINED
typedef uint32_t mach_port_t;
#endif

DISPATCH_EXPORT dispatch_runloop_handle_t _dispatch_get_main_queue_port_4CF(void);
DISPATCH_EXPORT void _dispatch_main_queue_callback_4CF(void *msg);
DISPATCH_EXPORT dispatch_queue_t _dispatch_runloop_root_queue_create_4CF(const char *label, unsigned long flags);
DISPATCH_EXPORT mach_port_t _dispatch_runloop_root_queue_get_port_4CF(dispatch_queue_t queue);
DISPATCH_EXPORT bool _dispatch_runloop_root_queue_perform_4CF(dispatch_queue_t queue);
// whether a handle from the two port calls above has work, CoreFoundation polls it as a descriptor
DISPATCH_EXPORT bool _dispatch_web_handle_ready(int handle);
DISPATCH_EXPORT void _dispatch_runloop_root_queue_wakeup_4CF(dispatch_queue_t queue);
DISPATCH_EXPORT void _dispatch_source_set_runloop_timer_4CF(dispatch_source_t source, dispatch_time_t start,
    uint64_t interval, uint64_t leeway);

#ifdef __cplusplus
}
#endif

#endif
