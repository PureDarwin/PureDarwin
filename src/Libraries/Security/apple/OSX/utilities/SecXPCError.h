/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#ifndef _UTILITIES_SECXPCERROR_H_
#define _UTILITIES_SECXPCERROR_H_

#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFError.h>
#include <xpc/xpc.h>

__BEGIN_DECLS

extern CFStringRef sSecXPCErrorDomain;

enum {
    kSecXPCErrorSuccess = 0,
    kSecXPCErrorUnexpectedType = 1,
    kSecXPCErrorUnexpectedNull = 2,
    kSecXPCErrorConnectionFailed = 3,
    kSecXPCErrorUnknown = 4,
};

CFErrorRef SecCreateCFErrorWithXPCObject(xpc_object_t xpc_error);
xpc_object_t SecCreateXPCObjectWithCFError(CFErrorRef error);

__END_DECLS

#endif /* UTILITIES_SECXPCERROR_H */
