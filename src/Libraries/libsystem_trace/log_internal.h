/**
 * log_internal.h
 * Author: Zoe Knox      Created: 2026-02-15
 *
 * Copyright (C) 2026 Zoe Knox. All rights reserved.
 * SPDX: BSD-2-Clause
 *
 * This is an original implementation of Apple's libsystem_trace.dylib based on
 * open-source code, including ASL, XNU, Swift, and other sources, and the API
 * specs on developer.apple.com.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __LOG_INTERNAL_H
#define __LOG_INTERNAL_H

#include <sys/param.h>
#include <sys/types.h>
#include <time.h>
#include <Block.h>
#include <os/base.h>
#include <os/object.h>
#include <os/log.h>

typedef struct __pd_CFRuntimeBase {
    uintptr_t _cfisa;
#if defined(__LP64__)
    _Atomic(uint64_t) _cfinfoa;
#else
    _Atomic(uint32_t) _cfinfoa;
#endif
} _pd_CFRuntimeBase;
#include <os/log_private.h>

typedef enum : uint32_t {
    OS_LOG_SINK_TYPE_FD, // dest is a file descriptor number
    OS_LOG_SINK_TYPE_SOCKET, // dest is a struct sockaddr*
    OS_LOG_SINK_TYPE_MACH // dest is a mach_port_t
} _os_log_sink_type_t;

struct os_log_s {
    uint8_t pad[sizeof(_pd_CFRuntimeBase)];
    uint32_t flags;
    void* subsystem;
    void* category;
    uintptr_t sink_dest; // either int val or pointer
    _os_log_sink_type_t sink_type;
    uint64_t generate_symptoms;
};

typedef void (^os_block_t)(void);
typedef void (*os_function_t)(void*);

extern size_t os_proc_available_memory(void);

/*
oslog
The OS_LOG_DEFAULT constant or a custom log object that you create with the os_log_create function.

type
A log type constant, such as OS_LOG_TYPE_DEFAULT, OS_LOG_TYPE_INFO, OS_LOG_TYPE_DEBUG, OS_LOG_TYPE_ERROR, or OS_LOG_TYPE_FAULT, that specifies the level of logging to check.

Return Value
true if logging at the specified level is in an enabled state; otherwise, false.
*/
extern bool os_log_type_enabled(os_log_t oslog, os_log_type_t type);

extern bool os_signpost_enabled(os_log_t log);


#endif /* __LOG_INTERNAL_H */
