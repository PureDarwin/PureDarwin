/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSCompoundPredicate_h
#define NSCompoundPredicate_h

#import <Foundation/NSPredicate.h>

@class NSArray;

typedef NS_ENUM(NSUInteger, NSCompoundPredicateType) {
    NSNotPredicateType = 0,
    NSAndPredicateType,
    NSOrPredicateType,
};

@interface NSCompoundPredicate : NSPredicate

+ (NSCompoundPredicate *)andPredicateWithSubpredicates:(NSArray *)subpredicates;
+ (NSCompoundPredicate *)orPredicateWithSubpredicates:(NSArray *)subpredicates;
+ (NSCompoundPredicate *)notPredicateWithSubpredicate:(NSPredicate *)predicate;

- (instancetype)initWithType:(NSCompoundPredicateType)type subpredicates:(NSArray *)subpredicates;

- (NSCompoundPredicateType)compoundPredicateType;
- (NSArray *)subpredicates;

@end

#endif /* NSCompoundPredicate_h */
