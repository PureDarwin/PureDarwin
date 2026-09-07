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

@class NSData;

typedef unsigned short unichar;

typedef NS_OPTIONS(NSUInteger, NSStringEncodingConversionOptions) {
    NSStringEncodingConversionExternalRepresentation = 1,
    NSStringEncodingConversionAllowLossy = 2,
};

typedef NS_ENUM(NSUInteger, NSStringEncoding) {
    NSASCIIStringEncoding = 1,
    NSNEXTSTEPStringEncoding = 2,
    NSJapaneseEUCStringEncoding = 3,
    NSUTF8StringEncoding = 4,
    NSISOLatin1StringEncoding = 5,
    NSSymbolStringEncoding = 6,
    NSNonLossyASCIIStringEncoding = 7,
    NSShiftJISStringEncoding = 8,
    NSISOLatin2StringEncoding = 9,
    NSUnicodeStringEncoding = 10,
    NSWindowsCP1251StringEncoding = 11,
    NSWindowsCP1252StringEncoding = 12,
    NSWindowsCP1253StringEncoding = 13,
    NSWindowsCP1254StringEncoding = 14,
    NSWindowsCP1250StringEncoding = 15,
    NSISO2022JPStringEncoding = 21,
    NSMacOSRomanStringEncoding = 30,
    NSUTF16StringEncoding = NSUnicodeStringEncoding,
    NSUTF16BigEndianStringEncoding = 0x90000100,
    NSUTF16LittleEndianStringEncoding = 0x94000100,
    NSUTF32StringEncoding = 0x8c000100,
    NSUTF32BigEndianStringEncoding = 0x98000100,
    NSUTF32LittleEndianStringEncoding = 0x9c000100,
};

@interface NSString : NSObject

+ (instancetype)stringWithUTF8String:(const char *)utf8String;
+ (instancetype)stringWithFormat:(NSString *)format, ...;

- (instancetype)initWithUTF8String:(const char *)utf8String;
- (instancetype)initWithBytes:(const void *)bytes
                       length:(NSUInteger)length
                     encoding:(NSStringEncoding)encoding;

- (NSData *)dataUsingEncoding:(NSStringEncoding)encoding;

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


@interface NSMutableString : NSString

+ (instancetype)string;
+ (instancetype)stringWithCapacity:(NSUInteger)capacity;

- (void)appendString:(NSString *)string;
- (void)appendFormat:(NSString *)format, ...;
- (void)setString:(NSString *)string;

@end

#endif /* NSString_h */
