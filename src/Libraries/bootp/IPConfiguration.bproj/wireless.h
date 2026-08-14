/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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
 * wireless.h
 * - definitions for WiFi
 */
/* 
 * Modification History
 *
 * July 6, 2020 	Dieter Siegmund (dieter@apple.com)
 * - moved out of ipconfigd.c
 */
#ifndef _S_WIRELESS_H
#define _S_WIRELESS_H

#include <stdint.h>
#include <CoreFoundation/CFString.h>
#include <net/ethernet.h>

typedef uint32_t wifi_auth_type;

#define WIFI_AUTH_TYPE_NONE 	0x0000
#define WIFI_AUTH_TYPE_UNKNOWN 	0xFFFF

const char *
wifi_auth_type_string(wifi_auth_type auth_type);
	
CFStringRef
wireless_copy_ssid_bssid(CFStringRef ifname, struct ether_addr * ap_mac,
			 wifi_auth_type * auth_type_p);

#endif /* _S_WIRELESS_H */
