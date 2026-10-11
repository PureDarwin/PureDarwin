/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSExpression_h
#define NSExpression_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSString, NSArray, NSMutableDictionary, NSPredicate;

typedef NS_ENUM(NSUInteger, NSExpressionType) {
    NSConstantValueExpressionType = 0,
    NSEvaluatedObjectExpressionType = 1,
    NSVariableExpressionType = 2,
    NSKeyPathExpressionType = 3,
    NSFunctionExpressionType = 4,
    NSUnionSetExpressionType = 5,
    NSIntersectSetExpressionType = 6,
    NSMinusSetExpressionType = 7,
    NSSubqueryExpressionType = 13,
    NSAggregateExpressionType = 14,
    NSAnyKeyExpressionType = 15,
    NSBlockExpressionType = 19,
    NSConditionalExpressionType = 20,
};

@interface NSExpression : NSObject <NSCopying>

+ (NSExpression *)expressionWithFormat:(NSString *)format, ...;
+ (NSExpression *)expressionWithFormat:(NSString *)format argumentArray:(NSArray *)arguments;
+ (NSExpression *)expressionWithFormat:(NSString *)format arguments:(va_list)arguments;

+ (NSExpression *)expressionForConstantValue:(id)value;
+ (NSExpression *)expressionForEvaluatedObject;
+ (NSExpression *)expressionForVariable:(NSString *)name;
+ (NSExpression *)expressionForKeyPath:(NSString *)keyPath;
+ (NSExpression *)expressionForFunction:(NSString *)name arguments:(NSArray *)parameters;
+ (NSExpression *)expressionForAggregate:(NSArray *)subexpressions;
+ (NSExpression *)expressionForUnionSet:(NSExpression *)left with:(NSExpression *)right;
+ (NSExpression *)expressionForIntersectSet:(NSExpression *)left with:(NSExpression *)right;
+ (NSExpression *)expressionForMinusSet:(NSExpression *)left with:(NSExpression *)right;
+ (NSExpression *)expressionForBlock:(id (^)(id evaluatedObject, NSArray *expressions, NSMutableDictionary *context))block
                           arguments:(NSArray *)arguments;
+ (NSExpression *)expressionForConditional:(NSPredicate *)predicate
                            trueExpression:(NSExpression *)trueExpression
                           falseExpression:(NSExpression *)falseExpression;
+ (NSExpression *)expressionForAnyKey;

- (instancetype)initWithExpressionType:(NSExpressionType)type;

- (NSExpressionType)expressionType;
- (id)constantValue;
- (NSString *)keyPath;
- (NSString *)function;
- (NSString *)variable;
- (NSExpression *)operand;
- (NSArray *)arguments;
- (id)collection;
- (NSPredicate *)predicate;
- (NSExpression *)leftExpression;
- (NSExpression *)rightExpression;
- (NSExpression *)trueExpression;
- (NSExpression *)falseExpression;

- (id)expressionValueWithObject:(id)object context:(NSMutableDictionary *)context;

@end

#endif /* NSExpression_h */
