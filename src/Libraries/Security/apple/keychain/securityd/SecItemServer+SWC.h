/*
 * Copyright (c) 2007-2019 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>

#if TARGET_OS_IOS && !TARGET_OS_BRIDGE
#include <CoreFoundation/CoreFoundation.h>

// Compatibility wrappers for SWC Objective-C interface.
typedef enum SecSWCFlags {
	kSecSWCFlags_None = 0,
	kSecSWCFlag_UserApproved = (1 << 0),
	kSecSWCFlag_UserDenied = (1 << 1),
	kSecSWCFlag_SiteApproved = (1 << 2),
	kSecSWCFlag_SiteDenied = (1 << 3),
} SecSWCFlags;

extern SecSWCFlags _SecAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFErrorRef *error);
extern void _SecSetAppDomainApprovalStatus(CFStringRef appID, CFStringRef fqdn, CFBooleanRef approved);

extern CFTypeRef _SecCopyFQDNObjectFromString(CFStringRef entitlementValue);
extern CFStringRef _SecGetFQDNFromFQDNObject(CFTypeRef fqdnObject, SInt32 *outPort);
#if !TARGET_OS_SIMULATOR
extern bool _SecEntitlementContainsDomainForService(CFArrayRef domains, CFStringRef domain, SInt32 port);
#endif /* !TARGET_OS_SIMULATOR */
#endif // TARGET_OS_IOS && !TARGET_OS_BRIDGE
