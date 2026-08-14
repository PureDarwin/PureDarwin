/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSNumber_h
#define NSNumber_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString;

/* The +numberWith... family is not just convenience API: the compiler emits
 * calls to it for boxed expressions, so @(x) needs whichever one matches the
 * static type of x. */
@interface NSNumber : NSObject

+ (instancetype)numberWithChar:(char)value;
+ (instancetype)numberWithUnsignedChar:(unsigned char)value;
+ (instancetype)numberWithShort:(short)value;
+ (instancetype)numberWithUnsignedShort:(unsigned short)value;
+ (instancetype)numberWithInt:(int)value;
+ (instancetype)numberWithUnsignedInt:(unsigned int)value;
+ (instancetype)numberWithLong:(long)value;
+ (instancetype)numberWithUnsignedLong:(unsigned long)value;
+ (instancetype)numberWithLongLong:(long long)value;
+ (instancetype)numberWithUnsignedLongLong:(unsigned long long)value;
+ (instancetype)numberWithFloat:(float)value;
+ (instancetype)numberWithDouble:(double)value;
+ (instancetype)numberWithBool:(BOOL)value;
+ (instancetype)numberWithInteger:(NSInteger)value;
+ (instancetype)numberWithUnsignedInteger:(NSUInteger)value;

@property (readonly) char charValue;
@property (readonly) unsigned char unsignedCharValue;
@property (readonly) short shortValue;
@property (readonly) unsigned short unsignedShortValue;
@property (readonly) int intValue;
@property (readonly) unsigned int unsignedIntValue;
@property (readonly) long longValue;
@property (readonly) unsigned long unsignedLongValue;
@property (readonly) long long longLongValue;
@property (readonly) unsigned long long unsignedLongLongValue;
@property (readonly) float floatValue;
@property (readonly) double doubleValue;
@property (readonly) BOOL boolValue;
@property (readonly) NSInteger integerValue;
@property (readonly) NSUInteger unsignedIntegerValue;
@property (readonly) NSString *stringValue;

@end

#endif /* NSNumber_h */
