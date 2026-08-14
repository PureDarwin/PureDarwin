/*
 * Copyright (c) 2010-2013 Apple Inc. All rights reserved.
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
 * HostUUID.c
 * - get the host UUID
 */

/* 
 * Modification History
 *
 * April 11, 2013
 * - created (split out from DHCPDUIDIAID.c)
 */

#include <unistd.h>
#include "HostUUID.h"
#include "symbol_scope.h"
#include "cfutil.h"

PRIVATE_EXTERN CFDataRef
HostUUIDGet(void)
{
    STATIC CFMutableDataRef	host_UUID;
    struct timespec		ts = { 0, 0 };

    if (host_UUID != NULL) {
	return (host_UUID);
    }
    host_UUID = CFDataCreateMutable(NULL, sizeof(uuid_t));
    CFDataSetLength(host_UUID, sizeof(uuid_t));
    if (gethostuuid(CFDataGetMutableBytePtr(host_UUID), &ts) != 0) {
	my_CFRelease(&host_UUID);
    }
    return (host_UUID);
}

