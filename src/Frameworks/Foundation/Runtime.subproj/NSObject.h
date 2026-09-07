/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if !defined(__FOUNDATION_NSOBJECT__)
#define __FOUNDATION_NSOBJECT__ 1

#import <objc/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSZone.h>
#include <CoreFoundation/CFBase.h>

/* The two bridging casts ARC code uses to hand an object to CoreFoundation and
 * back. They are inlines rather than functions so the __bridge_retained and
 * __bridge_transfer casts happen in the caller, where ARC can see them. */
static inline CFTypeRef _Nullable CFBridgingRetain(id _Nullable object) {
    return (__bridge_retained CFTypeRef)object;
}

static inline id _Nullable CFBridgingRelease(CFTypeRef CF_RELEASES_ARGUMENT _Nullable value) {
    return (__bridge_transfer id)value;
}

@protocol NSCopying
- (id)copyWithZone:(NSZone *)zone;
@end

@protocol NSMutableCopying
- (id)mutableCopyWithZone:(NSZone *)zone;
@end

#endif /* ! __FOUNDATION_NSOBJECT__ */
