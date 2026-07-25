/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import "__NSCFType.h"
#include <CoreFoundation/CFBase.h>

#pragma mark - __NSCFType

// __NSCFType instances are toll-free bridged CF objects (their isa was
// swapped onto a live CFRuntimeBase by _CFRuntimeCreateInstance). The real
// reference count and memory live in CF's own bookkeeping (CFRuntimeBase's
// _cfinfoa word), not in libobjc's side tables - so retain/release/hash/
// isEqual must all forward to the CF C API instead of NSObject's default
// (isa-keyed side table) implementations, or the two counters would
// diverge.
@implementation __NSCFType

- (BOOL) isEqual:(id)object {
    if (object == NULL) {
        return FALSE;
    }

    if (self == object) {
        return TRUE;
    }

    return CFEqual((CFTypeRef)self, (CFTypeRef)object);
}

- (NSUInteger) hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (id) retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void) release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger) retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

- (id) autorelease {
    return [(NSObject *)self autorelease];
}

- (BOOL) _tryRetain {
    CFRetain((CFTypeRef)self);
    return YES;
}

- (BOOL) _isDeallocating {
    return NO;
}

- (NSString *) description {
    return [(id)CFCopyDescription((CFTypeRef)self) autorelease];
}

- (void) dealloc {
    // never actually reached
}

@end


#pragma mark - NSObject (__NSCFType)

@implementation NSObject (__NSCFType)

- (CFTypeID) _cfTypeID {
    return _kCFRuntimeIDCFType;
}

@end
