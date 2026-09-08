/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#if !defined(__FOUNDATION_NSCODER__)
#define __FOUNDATION_NSCODER__ 1

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

/* NSCoding and NSSecureCoding are declared in NSObject.h, where Apple puts them. */

@class NSString, NSData;

/* Abstract: every method here raises. Concrete archivers override them. */
@interface NSCoder : NSObject

- (BOOL)allowsKeyedCoding;

- (void)encodeObject:(id)object;
- (id)decodeObject;
- (void)encodeBytes:(const void *)bytes length:(NSUInteger)length;
- (void)encodeValueOfObjCType:(const char *)type at:(const void *)address;
- (void)decodeValueOfObjCType:(const char *)type at:(void *)address;

- (BOOL)containsValueForKey:(NSString *)key;
- (void)encodeObject:(id)object forKey:(NSString *)key;
- (void)encodeBool:(BOOL)value forKey:(NSString *)key;
- (void)encodeInt:(int)value forKey:(NSString *)key;
- (void)encodeInt32:(int32_t)value forKey:(NSString *)key;
- (void)encodeInt64:(int64_t)value forKey:(NSString *)key;
- (void)encodeInteger:(NSInteger)value forKey:(NSString *)key;
- (void)encodeFloat:(float)value forKey:(NSString *)key;
- (void)encodeDouble:(double)value forKey:(NSString *)key;
- (void)encodeBytes:(const uint8_t *)bytes length:(NSUInteger)length forKey:(NSString *)key;

- (id)decodeObjectForKey:(NSString *)key;
- (BOOL)decodeBoolForKey:(NSString *)key;
- (int)decodeIntForKey:(NSString *)key;
- (int32_t)decodeInt32ForKey:(NSString *)key;
- (int64_t)decodeInt64ForKey:(NSString *)key;
- (NSInteger)decodeIntegerForKey:(NSString *)key;
- (float)decodeFloatForKey:(NSString *)key;
- (double)decodeDoubleForKey:(NSString *)key;
- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length;

@end

#endif /* ! __FOUNDATION_NSCODER__ */
