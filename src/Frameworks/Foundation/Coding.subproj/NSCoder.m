/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSCoder.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObjCRuntime.h>
#include <objc/runtime.h>

/* NSCoder is abstract; a concrete archiver has to override everything. */
static void NSCoderAbstract(id self, SEL _cmd) {
    [NSException raise:NSInvalidArgumentException
                format:@"-[%@ %s] is abstract; use a concrete NSCoder subclass",
                       NSStringFromClass([self class]), sel_getName(_cmd)];
}

@implementation NSCoder

- (BOOL)allowsKeyedCoding {
    return NO;
}

- (void)encodeObject:(id)object {
    NSCoderAbstract(self, _cmd);
}

- (id)decodeObject {
    NSCoderAbstract(self, _cmd);
    return nil;
}

- (void)encodeBytes:(const void *)bytes length:(NSUInteger)length {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeValueOfObjCType:(const char *)type at:(const void *)address {
    NSCoderAbstract(self, _cmd);
}

- (void)decodeValueOfObjCType:(const char *)type at:(void *)address {
    NSCoderAbstract(self, _cmd);
}

- (BOOL)containsValueForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return NO;
}

- (void)encodeObject:(id)object forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeBool:(BOOL)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeInt:(int)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeInt32:(int32_t)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeInt64:(int64_t)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeFloat:(float)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeDouble:(double)value forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (void)encodeBytes:(const uint8_t *)bytes length:(NSUInteger)length forKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
}

- (id)decodeObjectForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return nil;
}

- (BOOL)decodeBoolForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return NO;
}

- (int)decodeIntForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return 0;
}

- (int32_t)decodeInt32ForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return 0;
}

- (int64_t)decodeInt64ForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return 0;
}

- (float)decodeFloatForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return 0.0f;
}

- (double)decodeDoubleForKey:(NSString *)key {
    NSCoderAbstract(self, _cmd);
    return 0.0;
}

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)length {
    NSCoderAbstract(self, _cmd);
    return NULL;
}

@end
