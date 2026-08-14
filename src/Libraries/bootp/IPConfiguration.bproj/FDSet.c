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
 * FDSet.c
 * - contains FDCallout, a thin wrapper on CFSocketRef/CFFileDescriptorRef
 */
/* 
 * Modification History
 *
 * May 11, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 * June 12, 2000	Dieter Siegmund (dieter@apple.com)
 * - converted to use CFRunLoop
 * January 27, 2010	Dieter Siegmund (dieter@apple.com)
 * - use CFFileDescriptorRef for non-sockets
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <net/if_types.h>
#include <syslog.h>
#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFSocket.h>
#include <CoreFoundation/CFFileDescriptor.h>

#include "dynarray.h"
#include "FDSet.h"
#include "symbol_scope.h"

struct FDCallout {
    Boolean			is_socket;
    union {
	CFSocketRef		socket;
	CFFileDescriptorRef 	fdesc;
    } u;
    FDCalloutFuncRef		func;
    void *			arg1;
    void *			arg2;
};

STATIC void
FDCalloutSocketReceive(CFSocketRef s, CFSocketCallBackType type, 
		       CFDataRef address, const void *data, void *info);

STATIC void
FDCalloutFileDescriptorReceive(CFFileDescriptorRef f, 
			       CFOptionFlags callBackTypes, void *info);


STATIC void
FDCalloutCreateSocket(FDCalloutRef callout, int fd)
{
    CFSocketContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls;

    context.info = callout;
    callout->is_socket = TRUE;
    callout->u.socket 
	= CFSocketCreateWithNative(NULL, fd, kCFSocketReadCallBack,
				   FDCalloutSocketReceive, &context);
    rls = CFSocketCreateRunLoopSource(NULL, callout->u.socket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, 
		       kCFRunLoopDefaultMode);
    CFRelease(rls);
    return;
}

STATIC void
FDCalloutCreateFileDescriptor(FDCalloutRef callout, int fd)
{
    CFFileDescriptorContext	context = { 0, NULL, NULL, NULL, NULL };
    CFRunLoopSourceRef	rls;

    context.info = callout;
    callout->is_socket = FALSE;
    callout->u.fdesc
	= CFFileDescriptorCreate(NULL, fd, TRUE,
				 FDCalloutFileDescriptorReceive, &context);
    CFFileDescriptorEnableCallBacks(callout->u.fdesc,
				    kCFFileDescriptorReadCallBack);
    rls = CFFileDescriptorCreateRunLoopSource(NULL, callout->u.fdesc, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), rls, 
		       kCFRunLoopDefaultMode);
    CFRelease(rls);
    return;
}

PRIVATE_EXTERN FDCalloutRef
FDCalloutCreate(int fd, FDCalloutFuncRef func,
		void * arg1, void * arg2)
{
    FDCalloutRef	callout;
    struct stat		sb;

    if (fstat(fd, &sb) < 0) {
	return (NULL);
    }
    callout = malloc(sizeof(*callout));
    if (callout == NULL) {
	return (NULL);
    }
    bzero(callout, sizeof(*callout));
    if (S_ISSOCK(sb.st_mode)) {
	FDCalloutCreateSocket(callout, fd);
    }
    else {
	FDCalloutCreateFileDescriptor(callout, fd);
    }
    callout->func = func;
    callout->arg1 = arg1;
    callout->arg2 = arg2;
    return (callout);
}

STATIC void
FDCalloutReleaseSocket(FDCalloutRef callout)
{
    if (callout->u.socket != NULL) {
	CFSocketInvalidate(callout->u.socket);
	CFRelease(callout->u.socket);
	callout->u.socket = NULL;
    }
    return;
}

STATIC void
FDCalloutReleaseFileDescriptor(FDCalloutRef callout)
{
    if (callout->u.fdesc != NULL) {
	CFFileDescriptorInvalidate(callout->u.fdesc);
	CFRelease(callout->u.fdesc);
	callout->u.fdesc = NULL;
    }
    return;
}

PRIVATE_EXTERN void
FDCalloutRelease(FDCalloutRef * callout_p)
{
    FDCalloutRef callout = *callout_p;

    if (callout == NULL) {
	return;
    }
    if (callout->is_socket) {
	FDCalloutReleaseSocket(callout);
    }
    else {
	FDCalloutReleaseFileDescriptor(callout);
    }
    free(callout);
    *callout_p = NULL;
    return;
}

STATIC void
FDCalloutSocketReceive(CFSocketRef s, CFSocketCallBackType type, 
		       CFDataRef address, const void *data, void *info)
{
    FDCalloutRef 	callout = (FDCalloutRef)info;

    if (callout->func) {
	(*callout->func)(callout->arg1, callout->arg2);
    }
    return;
}

STATIC void
FDCalloutFileDescriptorReceive(CFFileDescriptorRef f, 
			       CFOptionFlags callBackTypes, void *info)
{
    FDCalloutRef 	callout = (FDCalloutRef)info;

    if (callout->func) {
	(*callout->func)(callout->arg1, callout->arg2);
    }
    /* each callback is one-shot, so we need to re-arm */
    CFFileDescriptorEnableCallBacks(callout->u.fdesc,
				    kCFFileDescriptorReadCallBack);
    return;
}

PRIVATE_EXTERN int
FDCalloutGetFD(FDCalloutRef callout)
{
    if (callout->is_socket) {
	return (CFSocketGetNative(callout->u.socket));
    }
    else {
	return (CFFileDescriptorGetNativeDescriptor(callout->u.fdesc));
    }
}
