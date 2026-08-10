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

#include "entitlements.h"

/// Moves the entitlement value from the original entitlement into the target entitlement.
static void transferEntitlement(CFMutableDictionaryRef entitlements, CFStringRef originalEntitlement, CFStringRef targetEntitlement)
{
    CFTypeRef value = (CFStringRef)CFDictionaryGetValue(entitlements, originalEntitlement);
    CFDictionaryAddValue(entitlements, targetEntitlement, value);
}

/// Determines if an entitlement needs fixup, which means it has a value for the original entitlement and no value for the
/// target entitlement.
static bool entitlementNeedsFixup(CFDictionaryRef entitlements, CFStringRef originalEntitlement, CFStringRef targetEntitlement)
{
    // Entitlements only need fixup on macOS running on Apple Silicon, so just always fall through to the default case otherwise.
#if TARGET_OS_OSX && TARGET_CPU_ARM64
    CFTypeRef originalValue = (CFStringRef)CFDictionaryGetValue(entitlements, originalEntitlement);
    CFTypeRef newValue = (CFStringRef)CFDictionaryGetValue(entitlements, targetEntitlement);
    if (originalValue != NULL && newValue == NULL) {
        return true;
    }
#endif
    return false;
}

bool needsCatalystEntitlementFixup(CFDictionaryRef entitlements)
{
    return entitlementNeedsFixup(entitlements, CFSTR("application-identifier"), CFSTR("com.apple.application-identifier")) ||
           entitlementNeedsFixup(entitlements, CFSTR("aps-environment"), CFSTR("com.apple.developer.aps-environment"));
}

bool updateCatalystEntitlements(CFMutableDictionaryRef entitlements)
{
    bool updated = false;
    if (entitlementNeedsFixup(entitlements, CFSTR("application-identifier"), CFSTR("com.apple.application-identifier"))) {
        transferEntitlement(entitlements, CFSTR("application-identifier"), CFSTR("com.apple.application-identifier"));
        updated = true;
    }
    if (entitlementNeedsFixup(entitlements, CFSTR("aps-environment"), CFSTR("com.apple.developer.aps-environment"))) {
        transferEntitlement(entitlements, CFSTR("aps-environment"), CFSTR("com.apple.developer.aps-environment"));
        updated = true;
    }
    return updated;
}
