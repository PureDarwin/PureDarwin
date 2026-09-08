/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSTextCheckingResult_h
#define NSTextCheckingResult_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

@class NSString, NSArray, NSDictionary, NSOrthography;

typedef NS_OPTIONS(uint64_t, NSTextCheckingType) {
    NSTextCheckingTypeOrthography   = 1ULL << 0,
    NSTextCheckingTypeSpelling      = 1ULL << 1,
    NSTextCheckingTypeGrammar       = 1ULL << 2,
    NSTextCheckingTypeDate          = 1ULL << 3,
    NSTextCheckingTypeAddress       = 1ULL << 4,
    NSTextCheckingTypeLink          = 1ULL << 5,
    NSTextCheckingTypeQuote         = 1ULL << 6,
    NSTextCheckingTypeDash          = 1ULL << 7,
    NSTextCheckingTypeReplacement   = 1ULL << 8,
    NSTextCheckingTypeCorrection    = 1ULL << 9,
    NSTextCheckingTypeRegularExpression = 1ULL << 10,
};

/* Callers spell the mask type plural. */
typedef uint64_t NSTextCheckingTypes;

@interface NSTextCheckingResult : NSObject <NSCopying> {
    NSTextCheckingType _resultType;
    NSRange _range;
    NSString *_replacementString;
}

+ (NSTextCheckingResult *)spellCheckingResultWithRange:(NSRange)range;
+ (NSTextCheckingResult *)grammarCheckingResultWithRange:(NSRange)range
                                                 details:(NSArray *)details;
+ (NSTextCheckingResult *)replacementCheckingResultWithRange:(NSRange)range
                                           replacementString:(NSString *)string;

- (NSTextCheckingType)resultType;
- (NSRange)range;
- (NSString *)replacementString;

@end

#endif /* NSTextCheckingResult_h */
