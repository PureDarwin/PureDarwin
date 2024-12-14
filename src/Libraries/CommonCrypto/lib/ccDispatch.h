/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

//
//  ccDispatch.h
//  CommonCrypto
//

#ifndef ccDispatch_h
#define ccDispatch_h

#if defined (_WIN32)
    #include <windows.h>
    #define dispatch_once_t  INIT_ONCE
    typedef void (*dispatch_function_t)(void *);
    void cc_dispatch_once(dispatch_once_t *predicate, void *context, dispatch_function_t function);
#else
    #include <dispatch/dispatch.h>
    #define cc_dispatch_once(predicate, context, function) dispatch_once_f(predicate, context, function)
#endif

#endif /* ccDispatch_h */
