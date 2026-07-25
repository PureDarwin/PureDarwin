/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSString_h
#define NSString_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

typedef unsigned short unichar;

typedef NS_OPTIONS(NSUInteger, NSStringEncodingConversionOptions) {
    NSStringEncodingConversionExternalRepresentation = 1,
    NSStringEncodingConversionAllowLossy = 2,
};

typedef NS_ENUM(NSUInteger, NSStringEncoding) {
    NSASCIIStringEncoding = 1,
    NSUTF8StringEncoding = 4,
};

@interface NSString : NSObject

+ (instancetype)stringWithUTF8String:(const char *)utf8String;
+ (instancetype)stringWithFormat:(NSString *)format, ...;

- (instancetype)initWithUTF8String:(const char *)utf8String;

- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (const char *)UTF8String;

- (BOOL)getBytes:(void *)buffer
       maxLength:(NSUInteger)maxBufferCount
      usedLength:(NSUInteger *)usedBufferCount
        encoding:(NSStringEncoding)encoding
         options:(NSStringEncodingConversionOptions)options
           range:(NSRange)range
  remainingRange:(NSRange *)leftover;

@end

#endif /* NSString_h */
