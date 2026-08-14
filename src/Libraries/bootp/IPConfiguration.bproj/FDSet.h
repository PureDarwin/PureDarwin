
#ifndef _S_FDSET_H
#define _S_FDSET_H
/*
 * Copyright (c) 2000-2010 Apple Inc. All rights reserved.
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

/*
 * FDSet.h
 * - maintains a list of file descriptors to watch and corresponding
 *   functions to call when the file descriptor is ready
 */

/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

/*
 * Type: FDCallout_func_t
 * Purpose:
 *   Client registers a function to call when file descriptor is ready.
 */

typedef void (FDCalloutFunc)(void * arg1, void * arg2);
typedef FDCalloutFunc * FDCalloutFuncRef;

struct FDCallout;
typedef struct FDCallout * FDCalloutRef;

FDCalloutRef
FDCalloutCreate(int fd, FDCalloutFuncRef func, void * arg1, void * arg2);

void
FDCalloutRelease(FDCalloutRef * callout_p);

int
FDCalloutGetFD(FDCalloutRef callout);

#endif /* _S_FDSET_H */
