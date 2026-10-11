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
#import <Foundation/NSSet.h>

@class NSString, NSDictionary;

@interface NSPredicate : NSObject <NSCopying>

+ (NSPredicate *)predicateWithFormat:(NSString *)format, ...;
+ (NSPredicate *)predicateWithFormat:(NSString *)format argumentArray:(NSArray *)arguments;
+ (NSPredicate *)predicateWithFormat:(NSString *)format arguments:(va_list)arguments;
+ (NSPredicate *)predicateWithValue:(BOOL)value;
+ (NSPredicate *)predicateWithBlock:(BOOL (^)(id evaluatedObject, NSDictionary *bindings))block;

- (BOOL)evaluateWithObject:(id)object;
- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables;
- (NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables;
- (NSString *)predicateFormat;

@end

@interface NSArray (NSPredicateSupport)
- (NSArray *)filteredArrayUsingPredicate:(NSPredicate *)predicate;
@end

@interface NSMutableArray (NSPredicateSupport)
- (void)filterUsingPredicate:(NSPredicate *)predicate;
@end

@interface NSSet (NSPredicateSupport)
- (NSSet *)filteredSetUsingPredicate:(NSPredicate *)predicate;
@end

@interface NSMutableSet (NSPredicateSupport)
- (void)filterUsingPredicate:(NSPredicate *)predicate;
@end

#endif /* NSPredicate_h */
