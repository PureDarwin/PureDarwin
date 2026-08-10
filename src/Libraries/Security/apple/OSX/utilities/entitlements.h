/*
* Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#ifndef _utilities_entitlements_h
#define _utilities_entitlements_h

#include <CoreFoundation/CoreFoundation.h>
#include <sys/cdefs.h>

__BEGIN_DECLS

/// Checks an entitlement dictionary to determine if any Catalyst-related entitlements need to be updated.
bool needsCatalystEntitlementFixup(CFDictionaryRef entitlements);

/// Modifies an entitlements dictionary to add the necessary Catalyst-related entitlements based on pre-existing entitlements.
/// Returns whether the entitlements were modified.
bool updateCatalystEntitlements(CFMutableDictionaryRef entitlements);

__END_DECLS

#endif /* _utilities_entitlements_h */
