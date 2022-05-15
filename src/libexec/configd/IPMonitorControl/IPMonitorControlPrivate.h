/*
 * Copyright (c) 2013-2021 Apple Inc. All rights reserved.
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

#ifndef _IPMONITOR_CONTROL_PRIVATE_H
#define _IPMONITOR_CONTROL_PRIVATE_H

#define kIPMonitorControlServerName \
    "com.apple.SystemConfiguration.IPMonitorControl"

typedef CF_ENUM(uint32_t, IPMonitorControlRequestType) {
    kIPMonitorControlRequestTypeNone					= 0,
    kIPMonitorControlRequestTypeSetInterfaceRank			= 1,
    kIPMonitorControlRequestTypeGetInterfaceRank			= 2,
    kIPMonitorControlRequestTypeSetInterfaceAdvisory			= 3,
    kIPMonitorControlRequestTypeInterfaceAdvisoryIsSet			= 4,
    kIPMonitorControlRequestTypeAnyInterfaceAdvisoryIsSet	 	= 5,
    kIPMonitorControlRequestTypeGetInterfaceRankAssertionInfo 		= 6,
    kIPMonitorControlRequestTypeGetInterfaceAdvisoryInfo 		= 7,
    kIPMonitorControlRequestTypeGetInterfaceRankAssertionInterfaceNames	= 8,
    kIPMonitorControlRequestTypeGetInterfaceAdvisoryInterfaceNames 	= 9,
};

/*
 * kIPMonitorControlRequestKey*
 * - keys used to communicate a request to the server
 */
#define kIPMonitorControlRequestKeyType			"Type"
#define kIPMonitorControlRequestKeyProcessName		"ProcessName"
#define kIPMonitorControlRequestKeyInterfaceName	"InterfaceName"
#define kIPMonitorControlRequestKeyPrimaryRank		"PrimaryRank"
#define kIPMonitorControlRequestKeyAdvisory		"Advisory"
#define kIPMonitorControlRequestKeyReason		"Reason"

/*
 * kIPMonitorControlResponseKey*
 * - keys used to communicate the response from the server
 */
#define kIPMonitorControlResponseKeyError		"Error"
#define kIPMonitorControlResponseKeyPrimaryRank		"PrimaryRank"
#define kIPMonitorControlResponseKeyAdvisoryIsSet	"AdvisoryIsSet"
#define kIPMonitorControlResponseKeyRankAssertionInfo	"RankAssertionInfo"
#define kIPMonitorControlResponseKeyAdvisoryInfo	"AdvisoryInfo"
#define kIPMonitorControlResponseKeyInterfaceNames	"InterfaceNames"

/*
 * kIPMonitorControlRankAssertionInfoKey*
 * - keys used in the individual rank assertion info dictionaries
 */
#define kIPMonitorControlRankAssertionInfoPrimaryRank	"PrimaryRank"
#define kIPMonitorControlRankAssertionInfoProcessID	"ProcessID"
#define kIPMonitorControlRankAssertionInfoProcessName	"ProcessName"

/*
 * kIPMonitorControlAdvisoryInfoKey*
 * - keys used in the individual advisory info dictionaries
 */
#define kIPMonitorControlAdvisoryInfoAdvisory		"Advisory"
#define kIPMonitorControlAdvisoryInfoProcessID		"ProcessID"
#define kIPMonitorControlAdvisoryInfoProcessName	"ProcessName"

static inline CFStringRef
_IPMonitorControlCopyInterfaceAdvisoryNotificationKey(CFStringRef ifname)
{
    return SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							 kSCDynamicStoreDomainState,
							 ifname,
							 CFSTR("Advisory"));
}

static inline CFStringRef
_IPMonitorControlCopyInterfaceRankAssertionNotificationKey(CFStringRef ifname)
{
    return SCDynamicStoreKeyCreateNetworkInterfaceEntity(NULL,
							 kSCDynamicStoreDomainState,
							 ifname,
							 CFSTR("RankAssertion"));
}

static inline void
my_CFRelease(void * t)
{
    void * * obj = (void * *)t;
    if (obj && *obj) {
	CFRelease(*obj);
	*obj = NULL;
    }
    return;
}

#endif /* _IPMONITOR_CONTROL_PRIVATE_H */
