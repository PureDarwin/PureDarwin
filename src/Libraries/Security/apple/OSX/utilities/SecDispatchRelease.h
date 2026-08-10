/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



#ifndef _SECDISPATCHRELEASE_H_
#define _SECDISPATCHRELEASE_H_

#include <dispatch/dispatch.h>
#include <xpc/xpc.h>

#define dispatch_retain_safe(DO) {  \
    __typeof__(DO) _do = (DO);      \
    if (_do)                        \
        dispatch_retain(_do);       \
}

#define dispatch_release_safe(DO) { \
    __typeof__(DO) _do = (DO);      \
    if (_do)                        \
        dispatch_release(_do);      \
}

#define dispatch_release_null(DO) { \
    __typeof__(DO) _do = (DO);      \
    if (_do) {                      \
        (DO) = NULL;                \
        dispatch_release(_do);      \
    }                               \
}


#define xpc_retain_safe(XO) {  \
    __typeof__(XO) _xo = (XO); \
    if (_xo)                   \
        xpc_retain(_xo);       \
}

#define xpc_release_safe(XO) { \
    __typeof__(XO) _xo = (XO); \
    if (_xo)                   \
        xpc_release(_xo);      \
}

#define xpc_release_null(XO) {  \
    __typeof__(XO) _xo = (XO); \
    if (_xo) {                  \
        (XO) = NULL;            \
        xpc_release(_xo);       \
    }                           \
}

#endif

