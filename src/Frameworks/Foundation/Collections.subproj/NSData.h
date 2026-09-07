/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSData_h
#define NSData_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSRange.h>

@interface NSData : NSObject

+ (instancetype)data;
+ (instancetype)dataWithBytes:(const void *)bytes length:(NSUInteger)length;
+ (instancetype)dataWithBytesNoCopy:(void *)bytes length:(NSUInteger)length;
+ (instancetype)dataWithBytesNoCopy:(void *)bytes
                             length:(NSUInteger)length
                       freeWhenDone:(BOOL)freeWhenDone;
+ (instancetype)dataWithData:(NSData *)data;

- (instancetype)init;
- (instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)length;
- (instancetype)initWithBytesNoCopy:(void *)bytes
                             length:(NSUInteger)length
                       freeWhenDone:(BOOL)freeWhenDone;
- (instancetype)initWithData:(NSData *)data;

- (const void *)bytes;
- (NSUInteger)length;

- (void)getBytes:(void *)buffer length:(NSUInteger)length;
- (void)getBytes:(void *)buffer range:(NSRange)range;
- (NSData *)subdataWithRange:(NSRange)range;
- (BOOL)isEqualToData:(NSData *)other;

@end

@interface NSMutableData : NSData

+ (instancetype)dataWithCapacity:(NSUInteger)capacity;
+ (instancetype)dataWithLength:(NSUInteger)length;

- (instancetype)initWithCapacity:(NSUInteger)capacity;
- (instancetype)initWithLength:(NSUInteger)length;

- (void *)mutableBytes;
- (void)setLength:(NSUInteger)length;
- (void)appendBytes:(const void *)bytes length:(NSUInteger)length;
- (void)appendData:(NSData *)other;
- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes;
- (void)resetBytesInRange:(NSRange)range;
- (void)setData:(NSData *)data;

@end

#endif /* NSData_h */
