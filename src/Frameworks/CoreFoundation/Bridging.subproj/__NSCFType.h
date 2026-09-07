/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef __NSCFType_h
#define __NSCFType_h

#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFRuntime_Internal.h>
#import <Foundation/NSObject.h>

/*
 * API NOTES:
 *
 * __NSCFType is an opaque type - used for when a CF type is not directly bridged to a Foundation class.
 *
 * This allows ANY CoreFoundation object to enter the ARC system when used in Objective-C code.
 */
@interface __NSCFType : NSObject

- (BOOL) _isDeallocating;
- (BOOL) _tryRetain;

- (void) dealloc;

- (NSString *) description;

- (NSUInteger) hash;

- (BOOL) isEqual: (id) object;

- (oneway void) release;

- (id) retain;

- (NSUInteger) retainCount;

@end

@interface NSObject (__NSCFType)

- (CFTypeID) _cfTypeID;

@end

#endif /* __NSCFType_h */
