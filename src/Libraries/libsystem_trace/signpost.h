/**
 * signpost.h
 * Author: Zoe Knox      Created: 2026-7-3
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

/* NOTE! This is all placeholder for now */

#ifndef SIGNPOST_H_
#define SIGNPOST_H_

#include <Availability.h>
#include <os/log.h>

typedef uint8_t os_signpost_type_t; // FIXME: probably wrong
typedef uint8_t os_signpost_id_t;   // FIXME: probably wrong

void __os_signpost_emit_impl(os_log_t log,
    os_signpost_type_t type,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);

void __os_signpost_emit_with_name_impl(os_log_t log,
    os_signpost_type_t type,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);

void __os_signpost_emit_unreliably_impl(os_log_t log,
    os_signpost_type_t type,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);

bool os_signpost_enabled(os_log_t log);
os_signpost_id_t os_signpost_id_generate(os_log_t log);
os_signpost_id_t os_signpost_id_make_with_pointer(os_log_t log,
    const void *ptr);

void os_signpost_interval_begin(os_log_t log,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);

void os_signpost_interval_end(os_log_t log,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);

void os_signpost_event_emit(os_log_t log,
    os_signpost_id_t spid,
    const char *name,
    const char *format,
    ...);


#endif // SIGNPOST_H_
