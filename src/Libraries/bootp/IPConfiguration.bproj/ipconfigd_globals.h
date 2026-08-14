/*
 * Copyright (c) 2000-2016 Apple Inc. All rights reserved.
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

#ifndef _S_IPCONFIGD_GLOBALS_H
#define _S_IPCONFIGD_GLOBALS_H

/*
 * ipconfigd_globals.h
 * - ipconfigd global definitions
 */
/* 
 * Modification History
 *
 * May 22, 2000		Dieter Siegmund (dieter@apple.com)
 * - created
 */

#include <CoreFoundation/CFString.h>
#include "mylog.h"
#include "util.h"
#include <sys/stat.h>

#define IPCONFIGURATION_PRIVATE_DIR	"/var/db/dhcpclient"
#define DHCPCLIENT_LEASES_DIR		IPCONFIGURATION_PRIVATE_DIR "/leases"
#define ARP_PROBE_FAILURE_RETRY_TIME	(8.0)

void
remove_unused_ip(const char * ifname, struct in_addr ip);

INLINE void
ipconfigd_create_paths(void)
{
    if (create_path(DHCPCLIENT_LEASES_DIR, 0700) < 0) {
	my_log(LOG_ERR, "failed to create " 
	       DHCPCLIENT_LEASES_DIR ", %s (%d)", strerror(errno), errno);
	return;
    }
    return;

}

#endif /* _S_IPCONFIGD_GLOBALS_H */
