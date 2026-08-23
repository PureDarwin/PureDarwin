/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCharacterSet_h
#define NSCharacterSet_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSString.h>

@class NSData;

@interface NSCharacterSet : NSObject

+ (NSCharacterSet *)alphanumericCharacterSet;
+ (NSCharacterSet *)controlCharacterSet;
+ (NSCharacterSet *)decimalDigitCharacterSet;
+ (NSCharacterSet *)letterCharacterSet;
+ (NSCharacterSet *)lowercaseLetterCharacterSet;
+ (NSCharacterSet *)newlineCharacterSet;
+ (NSCharacterSet *)punctuationCharacterSet;
+ (NSCharacterSet *)uppercaseLetterCharacterSet;
+ (NSCharacterSet *)whitespaceAndNewlineCharacterSet;
+ (NSCharacterSet *)whitespaceCharacterSet;

+ (NSCharacterSet *)characterSetWithCharactersInString:(NSString *)string;
+ (NSCharacterSet *)characterSetWithRange:(NSRange)range;

- (BOOL)characterIsMember:(unichar)character;
- (NSCharacterSet *)invertedSet;

@end

@interface NSMutableCharacterSet : NSCharacterSet

+ (NSMutableCharacterSet *)characterSetWithRange:(NSRange)range;

- (void)addCharactersInRange:(NSRange)range;
- (void)addCharactersInString:(NSString *)string;
- (void)removeCharactersInRange:(NSRange)range;
- (void)removeCharactersInString:(NSString *)string;
- (void)invert;

@end

#endif /* NSCharacterSet_h */
