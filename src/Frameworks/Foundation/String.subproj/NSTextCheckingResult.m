/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSTextCheckingResult.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSString.h>

@implementation NSTextCheckingResult

+ (NSTextCheckingResult *)_resultWithType:(NSTextCheckingType)type range:(NSRange)range {
    NSTextCheckingResult *result = [[[self alloc] init] autorelease];

    result->_resultType = type;
    result->_range = range;
    return result;
}

+ (NSTextCheckingResult *)spellCheckingResultWithRange:(NSRange)range {
    return [self _resultWithType:NSTextCheckingTypeSpelling range:range];
}

+ (NSTextCheckingResult *)grammarCheckingResultWithRange:(NSRange)range
                                                 details:(NSArray *)details {
    return [self _resultWithType:NSTextCheckingTypeGrammar range:range];
}

+ (NSTextCheckingResult *)replacementCheckingResultWithRange:(NSRange)range
                                           replacementString:(NSString *)string {
    NSTextCheckingResult *result = [self _resultWithType:NSTextCheckingTypeReplacement
                                                   range:range];

    result->_replacementString = [string copy];
    return result;
}

- (void)dealloc {
    [_replacementString release];
    [super dealloc];
}

+ (NSTextCheckingResult *)regularExpressionCheckingResultWithRanges:(NSArray *)ranges {
    NSTextCheckingResult *result = [[[self alloc] init] autorelease];

    result->_resultType = NSTextCheckingTypeRegularExpression;
    result->_ranges = [ranges retain];
    result->_range = ([ranges count] > 0)
        ? [[ranges objectAtIndex:0] rangeValue] : NSMakeRange(NSNotFound, 0);
    return result;
}

- (NSUInteger)numberOfRanges {
    return (_ranges != nil) ? [_ranges count] : 1;
}

- (NSRange)rangeAtIndex:(NSUInteger)index {
    if (_ranges == nil) {
        return (index == 0) ? _range : NSMakeRange(NSNotFound, 0);
    }
    if (index >= [_ranges count]) {
        return NSMakeRange(NSNotFound, 0);
    }
    return [[_ranges objectAtIndex:index] rangeValue];
}

- (NSTextCheckingType)resultType {
    return _resultType;
}

- (NSRange)range {
    return _range;
}

- (NSString *)replacementString {
    return _replacementString;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
