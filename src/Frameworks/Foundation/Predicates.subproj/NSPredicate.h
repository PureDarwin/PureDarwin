/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSPredicate_h
#define NSPredicate_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

/* The NSArray category below needs the real interface, not a forward
 * declaration. */
#import <Foundation/NSArray.h>

@class NSString, NSDictionary;

/* Declared so bindings and controllers compile. Predicate parsing is not
 * implemented: +predicateWithFormat: raises, and a predicate built any other
 * way evaluates true. */
@interface NSPredicate : NSObject <NSCopying>

+ (NSPredicate *)predicateWithFormat:(NSString *)format, ...;
+ (NSPredicate *)predicateWithValue:(BOOL)value;

- (BOOL)evaluateWithObject:(id)object;
- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables;
- (NSString *)predicateFormat;

@end

@interface NSArray (NSPredicateFiltering)
- (NSArray *)filteredArrayUsingPredicate:(NSPredicate *)predicate;
@end

#endif /* NSPredicate_h */
