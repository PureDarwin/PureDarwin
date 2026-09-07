/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSValue_h
#define NSValue_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSGeometry.h>
#import <Foundation/NSRange.h>

@class NSString;

@interface NSValue : NSObject

+ (instancetype)valueWithBytes:(const void *)bytes objCType:(const char *)type;
+ (instancetype)value:(const void *)bytes withObjCType:(const char *)type;
+ (instancetype)valueWithPointer:(const void *)pointer;
+ (instancetype)valueWithNonretainedObject:(id)object;
+ (instancetype)valueWithRange:(NSRange)range;
+ (instancetype)valueWithPoint:(NSPoint)point;
+ (instancetype)valueWithSize:(NSSize)size;
+ (instancetype)valueWithRect:(NSRect)rect;

- (void)getValue:(void *)buffer;
- (const char *)objCType NS_RETURNS_INNER_POINTER;

- (void *)pointerValue;
- (id)nonretainedObjectValue;
- (NSRange)rangeValue;
- (NSPoint)pointValue;
- (NSSize)sizeValue;
- (NSRect)rectValue;

@end

#endif /* NSValue_h */
