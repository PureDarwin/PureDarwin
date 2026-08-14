/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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
 * sysconfig.h
 * - system configuration related functions
 */

/* 
 * Modification History
 *
 * June 23, 2009	Dieter Siegmund (dieter@apple.com)
 * - split out from ipconfigd.c
 */

#ifndef _S_SYSCONFIG_H
#define _S_SYSCONFIG_H

#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFString.h>
#include <SystemConfiguration/SCDynamicStore.h>
#include <stdint.h>
#include "timer.h"
#include "ipconfigd_types.h"
#include "dhcp_options.h"
#include "DHCPv6Options.h"

CFDictionaryRef
my_SCDynamicStoreCopyDictionary(SCDynamicStoreRef session, CFStringRef key);

/*
 * Function: my_SCDynamicStoreSetService
 * Purpose:
 *   Accumulate the keys to set/remove for a particular service.
 * Note:
 *   This function does not update the SCDynamicStore, it just 
 *   accumulates keys/values.
 */
void
my_SCDynamicStoreSetService(SCDynamicStoreRef store,
			    CFStringRef serviceID,
			    CFStringRef * entities,
			    CFDictionaryRef * values,
			    int count,
			    boolean_t alternate_location);

/*
 * Function: my_SCDynamicStoreSetInterface
 * Purpose:
 *   Accumulate the keys to set/remove for a particular interface.
 * Note:
 *   This function does not update the SCDynamicStore, it just
 *   accumulates keys/values.
 */
void
my_SCDynamicStoreSetInterface(SCDynamicStoreRef store,
			      CFStringRef ifname,
			      CFStringRef entity,
			      CFDictionaryRef value);

/*
 * Function: my_SCDynamicStorePublish
 * Purpose:
 *   Update the SCDynamicStore with the accumulated keys/values generated
 *   by previous calls to my_SCDynamicStoreSetService().
 */
void
my_SCDynamicStorePublish(SCDynamicStoreRef store);
				
CFDictionaryRef
DHCPInfoDictionaryCreate(ipconfig_method_t method, dhcpol_t * options_p,
			 absolute_time_t start_time,
			 absolute_time_t expiration_time);

CFDictionaryRef
DHCPv6InfoDictionaryCreate(DHCPv6OptionListRef options);

CFStringRef
IPv4ARPCollisionKeyParse(CFStringRef cache_key, struct in_addr * ipaddr_p,
			 void * * hwaddr, int * hwlen);

CFDictionaryRef
DNSEntityCreateWithInfo(const char * if_name,
			dhcp_info_t * info_v4_p,
			ipv6_info_t * info_v6_p);

CFDictionaryRef
CaptivePortalEntityCreateWithInfo(dhcp_info_t * info_v4_p,
				  ipv6_info_t * info_v6_p);

void *
bytesFromColonHexString(CFStringRef colon_hex, int * len);

#endif /* _S_SYSCONFIG_H */
