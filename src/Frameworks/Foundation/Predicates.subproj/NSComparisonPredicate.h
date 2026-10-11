/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSComparisonPredicate_h
#define NSComparisonPredicate_h

#import <Foundation/NSPredicate.h>

@class NSExpression;

typedef NS_OPTIONS(NSUInteger, NSComparisonPredicateOptions) {
    NSCaseInsensitivePredicateOption = 0x01,
    NSDiacriticInsensitivePredicateOption = 0x02,
    NSNormalizedPredicateOption = 0x04,
};

typedef NS_ENUM(NSUInteger, NSComparisonPredicateModifier) {
    NSDirectPredicateModifier = 0,
    NSAllPredicateModifier,
    NSAnyPredicateModifier,
};

typedef NS_ENUM(NSUInteger, NSPredicateOperatorType) {
    NSLessThanPredicateOperatorType = 0,
    NSLessThanOrEqualToPredicateOperatorType,
    NSGreaterThanPredicateOperatorType,
    NSGreaterThanOrEqualToPredicateOperatorType,
    NSEqualToPredicateOperatorType,
    NSNotEqualToPredicateOperatorType,
    NSMatchesPredicateOperatorType,
    NSLikePredicateOperatorType,
    NSBeginsWithPredicateOperatorType,
    NSEndsWithPredicateOperatorType,
    NSInPredicateOperatorType,
    NSCustomSelectorPredicateOperatorType,
    NSContainsPredicateOperatorType = 99,
    NSBetweenPredicateOperatorType,
};

@interface NSComparisonPredicate : NSPredicate

+ (NSPredicate *)predicateWithLeftExpression:(NSExpression *)lhs
                             rightExpression:(NSExpression *)rhs
                                    modifier:(NSComparisonPredicateModifier)modifier
                                        type:(NSPredicateOperatorType)type
                                     options:(NSComparisonPredicateOptions)options;
+ (NSPredicate *)predicateWithLeftExpression:(NSExpression *)lhs
                             rightExpression:(NSExpression *)rhs
                              customSelector:(SEL)selector;

- (instancetype)initWithLeftExpression:(NSExpression *)lhs
                       rightExpression:(NSExpression *)rhs
                              modifier:(NSComparisonPredicateModifier)modifier
                                  type:(NSPredicateOperatorType)type
                               options:(NSComparisonPredicateOptions)options;
- (instancetype)initWithLeftExpression:(NSExpression *)lhs
                       rightExpression:(NSExpression *)rhs
                        customSelector:(SEL)selector;

- (NSPredicateOperatorType)predicateOperatorType;
- (NSComparisonPredicateModifier)comparisonPredicateModifier;
- (NSExpression *)leftExpression;
- (NSExpression *)rightExpression;
- (SEL)customSelector;
- (NSComparisonPredicateOptions)options;

@end

#endif /* NSComparisonPredicate_h */
