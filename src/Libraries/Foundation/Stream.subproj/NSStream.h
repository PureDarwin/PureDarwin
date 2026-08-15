/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _FOUNDATION_NSSTREAM_H_
#define _FOUNDATION_NSSTREAM_H_

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSData, NSString, NSURL;

/* Values match CFStreamStatus one-for-one; NSStream is the CFReadStream /
 * CFWriteStream bridge, so they must not drift. */
typedef NS_ENUM(NSUInteger, NSStreamStatus) {
    NSStreamStatusNotOpen = 0,
    NSStreamStatusOpening = 1,
    NSStreamStatusOpen = 2,
    NSStreamStatusReading = 3,
    NSStreamStatusWriting = 4,
    NSStreamStatusAtEnd = 5,
    NSStreamStatusClosed = 6,
    NSStreamStatusError = 7
};

typedef NSString *NSStreamPropertyKey;

/* Aliased to CF's own key at load time rather than a matching string literal,
 * so the two can never drift apart. */
extern NSStreamPropertyKey NSStreamFileCurrentOffsetKey;

@interface NSStream : NSObject

- (void)open;
- (void)close;

- (NSStreamStatus)streamStatus;

- (id)propertyForKey:(NSStreamPropertyKey)key;
- (BOOL)setProperty:(id)property forKey:(NSStreamPropertyKey)key;

@end

@interface NSInputStream : NSStream

+ (instancetype)inputStreamWithData:(NSData *)data;
+ (instancetype)inputStreamWithFileAtPath:(NSString *)path;
+ (instancetype)inputStreamWithURL:(NSURL *)url;

- (NSInteger)read:(uint8_t *)buffer maxLength:(NSUInteger)length;
- (BOOL)hasBytesAvailable;

@end

@interface NSOutputStream : NSStream

+ (instancetype)outputStreamToFileAtPath:(NSString *)path append:(BOOL)shouldAppend;
+ (instancetype)outputStreamWithURL:(NSURL *)url append:(BOOL)shouldAppend;

- (NSInteger)write:(const uint8_t *)buffer maxLength:(NSUInteger)length;
- (BOOL)hasSpaceAvailable;

@end

#endif /* _FOUNDATION_NSSTREAM_H_ */
