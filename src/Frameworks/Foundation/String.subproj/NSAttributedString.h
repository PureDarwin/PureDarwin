/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSAttributedString_h
#define NSAttributedString_h

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSRange.h>

@class NSDictionary, NSMutableString;

typedef NSString *NSAttributedStringKey;

@interface NSAttributedString : NSObject <NSCopying, NSMutableCopying, NSCoding>

- (instancetype)initWithString:(NSString *)string;
- (instancetype)initWithString:(NSString *)string attributes:(NSDictionary *)attributes;
- (instancetype)initWithAttributedString:(NSAttributedString *)other;

- (NSString *)string;
- (NSUInteger)length;

- (NSDictionary *)attributesAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range;
- (NSDictionary *)attributesAtIndex:(NSUInteger)index
              longestEffectiveRange:(NSRangePointer)range
                            inRange:(NSRange)limit;
- (id)attribute:(NSAttributedStringKey)name
        atIndex:(NSUInteger)index
 effectiveRange:(NSRangePointer)range;
- (id)attribute:(NSAttributedStringKey)name
              atIndex:(NSUInteger)index
longestEffectiveRange:(NSRangePointer)range
              inRange:(NSRange)limit;

- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range;
- (BOOL)isEqualToAttributedString:(NSAttributedString *)other;

@end

@interface NSMutableAttributedString : NSAttributedString

- (NSMutableString *)mutableString;

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string;
- (void)replaceCharactersInRange:(NSRange)range
            withAttributedString:(NSAttributedString *)string;
- (void)deleteCharactersInRange:(NSRange)range;

- (void)setAttributes:(NSDictionary *)attributes range:(NSRange)range;
- (void)addAttribute:(NSAttributedStringKey)name value:(id)value range:(NSRange)range;
- (void)addAttributes:(NSDictionary *)attributes range:(NSRange)range;
- (void)removeAttribute:(NSAttributedStringKey)name range:(NSRange)range;

- (void)appendAttributedString:(NSAttributedString *)string;
- (void)insertAttributedString:(NSAttributedString *)string atIndex:(NSUInteger)index;
- (void)setAttributedString:(NSAttributedString *)string;

- (void)beginEditing;
- (void)endEditing;

@end

#endif /* NSAttributedString_h */
