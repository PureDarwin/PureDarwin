/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRegularExpression_h
#define NSRegularExpression_h

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSTextCheckingResult.h>

@class NSString, NSArray, NSError;

typedef NS_OPTIONS(NSUInteger, NSRegularExpressionOptions) {
    NSRegularExpressionCaseInsensitive = 1 << 0,
    NSRegularExpressionAllowCommentsAndWhitespace = 1 << 1,
    NSRegularExpressionIgnoreMetacharacters = 1 << 2,
    NSRegularExpressionDotMatchesLineSeparators = 1 << 3,
    NSRegularExpressionAnchorsMatchLines = 1 << 4,
    NSRegularExpressionUseUnixLineSeparators = 1 << 5,
    NSRegularExpressionUseUnicodeWordBoundaries = 1 << 6
};

typedef NS_OPTIONS(NSUInteger, NSMatchingOptions) {
    NSMatchingReportProgress = 1 << 0,
    NSMatchingReportCompletion = 1 << 1,
    NSMatchingAnchored = 1 << 2,
    NSMatchingWithTransparentBounds = 1 << 3,
    NSMatchingWithoutAnchoringBounds = 1 << 4
};

/* Backed by POSIX extended regular expressions, so the pattern syntax is
 * ERE rather than ICU's. */
@interface NSRegularExpression : NSObject {
    NSString *_pattern;
    NSRegularExpressionOptions _options;
    void *_compiled;
}

+ (NSRegularExpression *)regularExpressionWithPattern:(NSString *)pattern
                                              options:(NSRegularExpressionOptions)options
                                                error:(NSError **)error;

- (instancetype)initWithPattern:(NSString *)pattern
                        options:(NSRegularExpressionOptions)options
                          error:(NSError **)error;

- (NSString *)pattern;
- (NSRegularExpressionOptions)options;
- (NSUInteger)numberOfCaptureGroups;

- (NSArray *)matchesInString:(NSString *)string
                     options:(NSMatchingOptions)options
                       range:(NSRange)range;
- (NSUInteger)numberOfMatchesInString:(NSString *)string
                              options:(NSMatchingOptions)options
                                range:(NSRange)range;
- (NSTextCheckingResult *)firstMatchInString:(NSString *)string
                                     options:(NSMatchingOptions)options
                                       range:(NSRange)range;
- (NSRange)rangeOfFirstMatchInString:(NSString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range;

@end

#endif /* NSRegularExpression_h */
