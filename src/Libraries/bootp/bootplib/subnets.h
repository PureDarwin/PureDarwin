/*
 * Copyright (c) 1998-2018 Apple Inc. All rights reserved.
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

#ifndef _S_BOOTPLIB_SUBNETS_H
#define _S_BOOTPLIB_SUBNETS_H

/*
 * subnets.h
 * - API's to access DHCP server subnet information
 */

/*
 * Modification History:
 * 
 * June 23, 2006	Dieter Siegmund (dieter@apple.com)
 * - initial revision (based on subnetDescr.h)
 */

#include <stdbool.h>
#include <netinet/in.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFArray.h>

#include "dhcp.h"

#define SUBNET_PROP__CREATOR		"_creator"
#define SUBNET_PROP_NAME		"name"
#define SUBNET_PROP_NET_ADDRESS		"net_address"
#define SUBNET_PROP_NET_MASK		"net_mask"
#define SUBNET_PROP_NET_RANGE		"net_range"
#define SUBNET_PROP_CLIENT_TYPES	"client_types"
#define SUBNET_PROP_SUPERNET		"supernet"
#define SUBNET_PROP_LEASE_MIN		"lease_min"
#define SUBNET_PROP_LEASE_MAX		"lease_max"


typedef bool (SubnetIsAddressInUseFunc)(void * private, struct in_addr ip);
typedef SubnetIsAddressInUseFunc * SubnetIsAddressInUseFuncRef;
typedef struct _SubnetList * SubnetListRef;
typedef struct _Subnet * SubnetRef;

/**
 ** SubnetListRef API's
 **/

SubnetRef
SubnetListAcquireAddress(SubnetListRef list, struct in_addr * addr,
			 SubnetIsAddressInUseFuncRef func, void * arg);

SubnetRef
SubnetListGetSubnetForAddress(SubnetListRef list, struct in_addr addr,
			      bool in_range);

bool
SubnetListAreAddressesOnSameSupernet(SubnetListRef list,
				     struct in_addr addr,
				     struct in_addr other_addr);

SubnetListRef
SubnetListCreateWithArray(CFArrayRef list);

void
SubnetListFree(SubnetListRef * subnets);

void
SubnetListPrint(SubnetListRef subnets);

void
SubnetListPrintCFString(CFMutableStringRef str, SubnetListRef subnets);

/**
 ** SubnetRef API's
 **/
dhcp_lease_time_t
SubnetGetMaxLease(SubnetRef subnet);

dhcp_lease_time_t
SubnetGetMinLease(SubnetRef subnet);

const char *
SubnetGetOptionPtrAndLength(SubnetRef subnet, dhcptag_t tag,
			    int * option_length);

struct in_addr
SubnetGetMask(SubnetRef subnet);

bool
SubnetDoesAllocate(SubnetRef subnet);

#endif /* _S_BOOTPLIB_SUBNETS_H */
