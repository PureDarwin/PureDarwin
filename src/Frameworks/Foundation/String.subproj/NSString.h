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
#include <stdarg.h>
#import <Foundation/NSRange.h>

@class NSData, NSArray, NSError;

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

typedef NS_OPTIONS(NSUInteger, NSStringCompareOptions) {
    NSCaseInsensitiveSearch = 1,
    NSLiteralSearch         = 2,
    NSBackwardsSearch       = 4,
    NSAnchoredSearch        = 8,
    NSNumericSearch         = 64,
};

@interface NSString : NSObject

+ (instancetype)stringWithUTF8String:(const char *)utf8String;
+ (instancetype)stringWithFormat:(NSString *)format, ...;
+ (nullable instancetype)stringWithContentsOfFile:(NSString *)path
                                          encoding:(NSStringEncoding)encoding
                                             error:(NSError **)error;
+ (nullable instancetype)stringWithContentsOfFile:(NSString *)path;

- (instancetype)init;
- (instancetype)initWithString:(NSString *)string;
- (instancetype)initWithFormat:(NSString *)format, ...;
- (instancetype)initWithFormat:(NSString *)format arguments:(va_list)arguments;
- (instancetype)initWithCharacters:(const unichar *)characters length:(NSUInteger)length;
- (instancetype)initWithCString:(const char *)cString encoding:(NSStringEncoding)encoding;
- (instancetype)initWithUTF8String:(const char *)utf8String;
- (instancetype)initWithBytes:(const void *)bytes
                       length:(NSUInteger)length
                     encoding:(NSStringEncoding)encoding;
- (nullable instancetype)initWithContentsOfFile:(NSString *)path
                                        encoding:(NSStringEncoding)encoding
                                           error:(NSError **)error;
- (nullable instancetype)initWithContentsOfFile:(NSString *)path;

- (BOOL)isEqualToString:(NSString *)other;
- (void)getLineStart:(NSUInteger *)startPtr end:(NSUInteger *)endPtr
         contentsEnd:(NSUInteger *)contentsEndPtr forRange:(NSRange)range;
- (void)getParagraphStart:(NSUInteger *)startPtr end:(NSUInteger *)endPtr
              contentsEnd:(NSUInteger *)contentsEndPtr forRange:(NSRange)range;
- (int)intValue;
- (NSInteger)integerValue;
- (long long)longLongValue;
- (float)floatValue;
- (double)doubleValue;
- (BOOL)boolValue;
- (NSComparisonResult)compare:(NSString *)other;
- (NSComparisonResult)caseInsensitiveCompare:(NSString *)other;
- (BOOL)hasPrefix:(NSString *)prefix;
- (BOOL)hasSuffix:(NSString *)suffix;
- (BOOL)containsString:(NSString *)string;
- (NSString *)lowercaseString;
- (NSString *)uppercaseString;
- (NSString *)capitalizedString;
- (NSString *)stringByTrimmingCharactersInSet:(NSCharacterSet *)set;
+ (instancetype)stringWithString:(NSString *)string;
+ (instancetype)stringWithCString:(const char *)cString encoding:(NSStringEncoding)encoding;
+ (instancetype)stringWithCString:(const char *)cString;
+ (NSStringEncoding)defaultCStringEncoding;
- (const char *)cStringUsingEncoding:(NSStringEncoding)encoding NS_RETURNS_INNER_POINTER;
- (NSRange)rangeOfCharacterFromSet:(NSCharacterSet *)set;
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (NSArray *)componentsSeparatedByCharactersInSet:(NSCharacterSet *)set;
- (NSString *)stringByReplacingCharactersInRange:(NSRange)range withString:(NSString *)replacement;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target withString:(NSString *)replacement;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement
                                           options:(NSStringCompareOptions)options
                                             range:(NSRange)range;
- (NSComparisonResult)compare:(NSString *)other options:(NSStringCompareOptions)options;
- (NSComparisonResult)compare:(NSString *)other
                      options:(NSStringCompareOptions)options
                        range:(NSRange)range;
- (NSComparisonResult)localizedCompare:(NSString *)other;
- (NSComparisonResult)localizedCaseInsensitiveCompare:(NSString *)other;
- (NSData *)dataUsingEncoding:(NSStringEncoding)encoding;
- (instancetype)initWithData:(NSData *)data encoding:(NSStringEncoding)encoding;
+ (instancetype)stringWithCharacters:(const unichar *)characters length:(NSUInteger)length;

- (NSUInteger)length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (void)getCharacters:(unichar *)buffer;
- (void)getCharacters:(unichar *)buffer range:(NSRange)range;
- (const char *)UTF8String;
- (const char *)cString;
- (const char *)fileSystemRepresentation;
- (NSArray *)componentsSeparatedByString:(NSString *)separator;
- (NSRange)rangeOfString:(NSString *)string;
- (NSRange)lineRangeForRange:(NSRange)range;
- (NSRange)paragraphRangeForRange:(NSRange)range;
- (NSRange)rangeOfString:(NSString *)string options:(NSStringCompareOptions)options;
- (NSRange)rangeOfString:(NSString *)string
                 options:(NSStringCompareOptions)options
                   range:(NSRange)searchRange;
- (NSString *)substringWithRange:(NSRange)range;
- (NSString *)substringFromIndex:(NSUInteger)index;
- (NSString *)substringToIndex:(NSUInteger)index;
- (NSString *)stringByAppendingString:(NSString *)string;

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
- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string;
- (void)insertString:(NSString *)string atIndex:(NSUInteger)index;
- (void)deleteCharactersInRange:(NSRange)range;
- (NSUInteger)replaceOccurrencesOfString:(NSString *)target
                              withString:(NSString *)replacement
                                 options:(NSStringCompareOptions)options
                                   range:(NSRange)searchRange;

@end

#endif /* NSString_h */
