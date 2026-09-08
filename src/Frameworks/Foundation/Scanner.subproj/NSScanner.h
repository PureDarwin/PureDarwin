/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSScanner_h
#define NSScanner_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSRange.h>

@class NSString, NSCharacterSet, NSDictionary;

@interface NSScanner : NSObject <NSCopying> {
    NSString *_string;
    NSCharacterSet *_skipSet;
    NSUInteger _location;
    BOOL _caseSensitive;
}

+ (instancetype)scannerWithString:(NSString *)string;
+ (instancetype)localizedScannerWithString:(NSString *)string;

- (instancetype)initWithString:(NSString *)string;

- (NSString *)string;
- (NSUInteger)scanLocation;
- (void)setScanLocation:(NSUInteger)location;
- (BOOL)isAtEnd;

- (NSCharacterSet *)charactersToBeSkipped;
- (void)setCharactersToBeSkipped:(NSCharacterSet *)set;
- (BOOL)caseSensitive;
- (void)setCaseSensitive:(BOOL)caseSensitive;

- (BOOL)scanInt:(int *)value;
- (BOOL)scanInteger:(NSInteger *)value;
- (BOOL)scanLongLong:(long long *)value;
- (BOOL)scanHexInt:(unsigned *)value;
- (BOOL)scanFloat:(float *)value;
- (BOOL)scanDouble:(double *)value;
- (BOOL)scanString:(NSString *)string intoString:(NSString **)result;
- (BOOL)scanCharactersFromSet:(NSCharacterSet *)set intoString:(NSString **)result;
- (BOOL)scanUpToString:(NSString *)string intoString:(NSString **)result;
- (BOOL)scanUpToCharactersFromSet:(NSCharacterSet *)set intoString:(NSString **)result;

@end

#endif /* NSScanner_h */
