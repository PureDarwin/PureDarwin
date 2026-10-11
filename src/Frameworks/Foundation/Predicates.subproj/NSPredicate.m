/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

// NSPredicate, NSComparisonPredicate, NSCompoundPredicate and NSExpression with the format parser, after Apple's
// Predicate Programming Guide and the class references

#import <Foundation/NSPredicate.h>
#import <Foundation/NSComparisonPredicate.h>
#import <Foundation/NSCompoundPredicate.h>
#import <Foundation/NSExpression.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSException.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSRegularExpression.h>
#import <Foundation/NSOrderedSet.h>
#include <CoreFoundation/CoreFoundation.h>
#include <math.h>
#include <stdarg.h>

static void PDPredicateRaise(NSString *format, ...) {
    va_list args;

    va_start(args, format);
    NSString *reason = [[[NSString alloc] initWithFormat:format arguments:args] autorelease];
    va_end(args);
    [NSException raise:NSInvalidArgumentException format:@"%@", reason];
}

static id PDNilIfNull(id value) {
    return value == [NSNull null] ? nil : value;
}

static BOOL PDIsCollection(id value) {
    return [value isKindOfClass:[NSArray class]] || [value isKindOfClass:[NSSet class]] ||
           [value isKindOfClass:[NSOrderedSet class]] || [value isKindOfClass:[NSDictionary class]];
}

// the members of any collection, a dictionary contributing its values as Cocoa does
static NSArray *PDCollectionObjects(id value) {
    if ([value isKindOfClass:[NSArray class]])
        return value;
    if ([value isKindOfClass:[NSSet class]])
        return [value allObjects];
    if ([value isKindOfClass:[NSOrderedSet class]])
        return [value array];
    if ([value isKindOfClass:[NSDictionary class]])
        return [value allValues];
    return nil;
}

// a quoted string the way -predicateFormat writes it
static NSString *PDQuote(NSString *string) {
    NSMutableString *out = [NSMutableString stringWithString:@"\""];
    NSUInteger length = [string length];

    for (NSUInteger i = 0; i < length; i++) {
        unichar c = [string characterAtIndex:i];

        if (c == '"' || c == '\\')
            [out appendFormat:@"\\%C", c];
        else
            [out appendFormat:@"%C", c];
    }
    [out appendString:@"\""];
    return out;
}

#pragma mark - NSExpression

@interface NSExpression () {
@public
    NSExpressionType _type;
    id _constant;
    NSString *_keyPath;
    NSString *_variable;
    NSString *_function;
    NSArray *_arguments;
    NSExpression *_operand;
    id _collection;
    NSPredicate *_predicate;
    NSExpression *_left;
    NSExpression *_right;
    id _block;
}
@end

static NSString *PDExpressionFormat(NSExpression *e);

// the arithmetic functions the format language writes as operators
static NSString *PDInfixFor(NSString *function) {
    static NSDictionary *infix;

    if (infix == nil)
        infix = [@{ @"add:to:": @"+", @"from:subtract:": @"-", @"multiply:by:": @"*", @"divide:by:": @"/",
                    @"raise:toPower:": @"**" } retain];
    return infix[function];
}

static double PDDouble(id value) {
    value = PDNilIfNull(value);
    if ([value respondsToSelector:@selector(doubleValue)])
        return [value doubleValue];
    return 0;
}

static NSNumber *PDNumberFor(double value) {
    if (value == floor(value) && fabs(value) < 9007199254740992.0)
        return [NSNumber numberWithLongLong:(long long)value];
    return [NSNumber numberWithDouble:value];
}

static id PDEvaluateFunction(NSString *name, NSArray *values) {
    NSUInteger count = [values count];
    id first = count > 0 ? values[0] : nil;
    NSArray *list = PDCollectionObjects(first);

    if ([name isEqualToString:@"add:to:"])
        return PDNumberFor(PDDouble(values[0]) + PDDouble(values[1]));
    if ([name isEqualToString:@"from:subtract:"])
        return PDNumberFor(PDDouble(values[0]) - PDDouble(values[1]));
    if ([name isEqualToString:@"multiply:by:"])
        return PDNumberFor(PDDouble(values[0]) * PDDouble(values[1]));
    if ([name isEqualToString:@"divide:by:"])
        return [NSNumber numberWithDouble:PDDouble(values[0]) / PDDouble(values[1])];
    if ([name isEqualToString:@"modulus:by:"])
        return PDNumberFor(fmod(PDDouble(values[0]), PDDouble(values[1])));
    if ([name isEqualToString:@"raise:toPower:"])
        return PDNumberFor(pow(PDDouble(values[0]), PDDouble(values[1])));
    if ([name isEqualToString:@"sqrt:"])
        return [NSNumber numberWithDouble:sqrt(PDDouble(first))];
    if ([name isEqualToString:@"abs:"])
        return PDNumberFor(fabs(PDDouble(first)));
    if ([name isEqualToString:@"floor:"])
        return PDNumberFor(floor(PDDouble(first)));
    if ([name isEqualToString:@"ceiling:"])
        return PDNumberFor(ceil(PDDouble(first)));
    if ([name isEqualToString:@"trunc:"])
        return PDNumberFor(trunc(PDDouble(first)));
    if ([name isEqualToString:@"exp:"])
        return [NSNumber numberWithDouble:exp(PDDouble(first))];
    if ([name isEqualToString:@"ln:"])
        return [NSNumber numberWithDouble:log(PDDouble(first))];
    if ([name isEqualToString:@"log:"])
        return [NSNumber numberWithDouble:log10(PDDouble(first))];
    if ([name isEqualToString:@"uppercase:"])
        return [PDNilIfNull(first) uppercaseString];
    if ([name isEqualToString:@"lowercase:"])
        return [PDNilIfNull(first) lowercaseString];
    if ([name isEqualToString:@"now"])
        return [NSDate date];
    if ([name isEqualToString:@"noindex:"])
        return first;
    if ([name isEqualToString:@"count:"])
        return [NSNumber numberWithUnsignedInteger:list != nil ? [list count] : (first != nil ? 1 : 0)];
    if ([name isEqualToString:@"sum:"] || [name isEqualToString:@"average:"]) {
        double sum = 0;

        for (id v in list)
            sum += PDDouble(v);
        if ([name isEqualToString:@"sum:"])
            return PDNumberFor(sum);
        return [NSNumber numberWithDouble:[list count] > 0 ? sum / [list count] : 0];
    }
    if ([name isEqualToString:@"min:"] || [name isEqualToString:@"max:"]) {
        BOOL wantMax = [name isEqualToString:@"max:"];
        id best = nil;

        for (id v in list) {
            if (PDNilIfNull(v) == nil)
                continue;
            if (best == nil || ([v compare:best] == (wantMax ? NSOrderedDescending : NSOrderedAscending)))
                best = v;
        }
        return best;
    }
    if ([name isEqualToString:@"objectFrom:withIndex:"]) {
        NSArray *objects = PDCollectionObjects(first);
        id index = values[1];

        if ([index isKindOfClass:[NSString class]]) {
            if ([index isEqualToString:@"FIRST"])
                return [objects count] > 0 ? objects[0] : nil;
            if ([index isEqualToString:@"LAST"])
                return [objects lastObject];
            if ([index isEqualToString:@"SIZE"])
                return [NSNumber numberWithUnsignedInteger:[objects count]];
        }
        if ([first isKindOfClass:[NSDictionary class]])
            return [first objectForKey:index];
        NSUInteger i = [index unsignedIntegerValue];
        return i < [objects count] ? objects[i] : nil;
    }
    if ([name isEqualToString:@"valueForKeyPath:"])
        return [PDNilIfNull(first) valueForKeyPath:values[1]];
    PDPredicateRaise(@"Unsupported function %@ in predicate expression", name);
    return nil;
}

@implementation NSExpression

- (instancetype)initWithExpressionType:(NSExpressionType)type {
    if ((self = [super init]) != nil)
        _type = type;
    return self;
}

- (void)dealloc {
    [_constant release];
    [_keyPath release];
    [_variable release];
    [_function release];
    [_arguments release];
    [_operand release];
    [_collection release];
    [_predicate release];
    [_left release];
    [_right release];
    [_block release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

+ (NSExpression *)expressionForConstantValue:(id)value {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSConstantValueExpressionType] autorelease];

    e->_constant = [value retain];
    return e;
}

+ (NSExpression *)expressionForEvaluatedObject {
    return [[[self alloc] initWithExpressionType:NSEvaluatedObjectExpressionType] autorelease];
}

+ (NSExpression *)expressionForVariable:(NSString *)name {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSVariableExpressionType] autorelease];

    e->_variable = [name copy];
    return e;
}

+ (NSExpression *)expressionForKeyPath:(NSString *)keyPath {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSKeyPathExpressionType] autorelease];

    e->_keyPath = [keyPath copy];
    return e;
}

+ (NSExpression *)expressionForFunction:(NSString *)name arguments:(NSArray *)parameters {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSFunctionExpressionType] autorelease];

    e->_function = [name copy];
    e->_arguments = [parameters copy];
    return e;
}

+ (NSExpression *)expressionForAggregate:(NSArray *)subexpressions {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSAggregateExpressionType] autorelease];

    e->_collection = [subexpressions copy];
    return e;
}

static NSExpression *PDSetExpression(NSExpressionType type, NSExpression *left, NSExpression *right) {
    NSExpression *e = [[[NSExpression alloc] initWithExpressionType:type] autorelease];

    e->_left = [left retain];
    e->_right = [right retain];
    return e;
}

+ (NSExpression *)expressionForUnionSet:(NSExpression *)left with:(NSExpression *)right {
    return PDSetExpression(NSUnionSetExpressionType, left, right);
}

+ (NSExpression *)expressionForIntersectSet:(NSExpression *)left with:(NSExpression *)right {
    return PDSetExpression(NSIntersectSetExpressionType, left, right);
}

+ (NSExpression *)expressionForMinusSet:(NSExpression *)left with:(NSExpression *)right {
    return PDSetExpression(NSMinusSetExpressionType, left, right);
}

+ (NSExpression *)expressionForBlock:(id (^)(id, NSArray *, NSMutableDictionary *))block arguments:(NSArray *)arguments {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSBlockExpressionType] autorelease];

    e->_block = [block copy];
    e->_arguments = [arguments copy];
    return e;
}

+ (NSExpression *)expressionForConditional:(NSPredicate *)predicate trueExpression:(NSExpression *)trueExpression
                           falseExpression:(NSExpression *)falseExpression {
    NSExpression *e = [[[self alloc] initWithExpressionType:NSConditionalExpressionType] autorelease];

    e->_predicate = [predicate retain];
    e->_left = [trueExpression retain];
    e->_right = [falseExpression retain];
    return e;
}

+ (NSExpression *)expressionForAnyKey {
    return [[[self alloc] initWithExpressionType:NSAnyKeyExpressionType] autorelease];
}

- (NSExpressionType)expressionType {
    return _type;
}

- (id)constantValue {
    return _constant;
}

- (NSString *)keyPath {
    return _keyPath;
}

- (NSString *)function {
    return _function;
}

- (NSString *)variable {
    return _variable;
}

- (NSExpression *)operand {
    return _operand;
}

- (NSArray *)arguments {
    return _arguments;
}

- (id)collection {
    return _collection;
}

- (NSPredicate *)predicate {
    return _predicate;
}

- (NSExpression *)leftExpression {
    return _left;
}

- (NSExpression *)rightExpression {
    return _right;
}

- (NSExpression *)trueExpression {
    return _left;
}

- (NSExpression *)falseExpression {
    return _right;
}

- (id)expressionValueWithObject:(id)object context:(NSMutableDictionary *)context {
    switch (_type) {
    case NSConstantValueExpressionType:
        return _constant;
    case NSEvaluatedObjectExpressionType:
        return object;
    case NSVariableExpressionType: {
        id value = [context objectForKey:_variable];

        if (value == nil && [context objectForKey:_variable] == nil && ![[context allKeys] containsObject:_variable])
            PDPredicateRaise(@"Can't get value for '%@' in bindings %@.", _variable, context);
        return value;
    }
    case NSKeyPathExpressionType: {
        id base = _operand != nil ? [_operand expressionValueWithObject:object context:context] : object;

        base = PDNilIfNull(base);
        return base != nil ? [base valueForKeyPath:_keyPath] : nil;
    }
    case NSFunctionExpressionType: {
        NSMutableArray *values = [NSMutableArray array];

        for (NSExpression *arg in _arguments) {
            id v = [arg expressionValueWithObject:object context:context];

            [values addObject:v != nil ? v : [NSNull null]];
        }
        if (_operand != nil) {
            // a FUNCTION() call on an object's own method
            id target = PDNilIfNull([_operand expressionValueWithObject:object context:context]);
            SEL sel = NSSelectorFromString(_function);

            if ([values count] == 0)
                return [target performSelector:sel];
            return [target performSelector:sel withObject:PDNilIfNull(values[0])];
        }
        return PDEvaluateFunction(_function, values);
    }
    case NSAggregateExpressionType: {
        NSMutableArray *values = [NSMutableArray array];

        for (NSExpression *sub in _collection) {
            id v = [sub expressionValueWithObject:object context:context];

            [values addObject:v != nil ? v : [NSNull null]];
        }
        return values;
    }
    case NSUnionSetExpressionType:
    case NSIntersectSetExpressionType:
    case NSMinusSetExpressionType: {
        NSMutableSet *set = [NSMutableSet setWithArray:PDCollectionObjects([_left expressionValueWithObject:object context:context])];
        NSArray *other = PDCollectionObjects([_right expressionValueWithObject:object context:context]);

        if (_type == NSUnionSetExpressionType)
            [set addObjectsFromArray:other];
        else if (_type == NSIntersectSetExpressionType)
            [set intersectSet:[NSSet setWithArray:other]];
        else
            [set minusSet:[NSSet setWithArray:other]];
        return set;
    }
    case NSSubqueryExpressionType: {
        NSArray *objects = PDCollectionObjects([_collection expressionValueWithObject:object context:context]);
        NSMutableArray *result = [NSMutableArray array];
        NSMutableDictionary *bindings = context != nil ? [[context mutableCopy] autorelease] : [NSMutableDictionary dictionary];

        for (id item in objects) {
            [bindings setObject:item forKey:_variable];
            if ([_predicate evaluateWithObject:item substitutionVariables:bindings])
                [result addObject:item];
        }
        return result;
    }
    case NSBlockExpressionType: {
        NSMutableArray *values = [NSMutableArray array];

        for (NSExpression *arg in _arguments) {
            id v = [arg expressionValueWithObject:object context:context];

            [values addObject:v != nil ? v : [NSNull null]];
        }
        return ((id (^)(id, NSArray *, NSMutableDictionary *))_block)(object, values, context);
    }
    case NSConditionalExpressionType:
        return [_predicate evaluateWithObject:object substitutionVariables:context]
                   ? [_left expressionValueWithObject:object context:context]
                   : [_right expressionValueWithObject:object context:context];
    default:
        PDPredicateRaise(@"Expression type %lu cannot be evaluated", (unsigned long)_type);
        return nil;
    }
}

// a copy with $variables replaced by their values, for -predicateWithSubstitutionVariables:
- (NSExpression *)_expressionWithSubstitutionVariables:(NSDictionary *)variables {
    switch (_type) {
    case NSVariableExpressionType: {
        id value = [variables objectForKey:_variable];

        if (value == nil)
            return self;
        return [NSExpression expressionForConstantValue:PDNilIfNull(value)];
    }
    case NSKeyPathExpressionType: {
        if (_operand == nil)
            return self;
        NSExpression *e = [NSExpression expressionForKeyPath:_keyPath];

        e->_operand = [[_operand _expressionWithSubstitutionVariables:variables] retain];
        return e;
    }
    case NSFunctionExpressionType: {
        NSMutableArray *args = [NSMutableArray array];

        for (NSExpression *arg in _arguments)
            [args addObject:[arg _expressionWithSubstitutionVariables:variables]];
        NSExpression *e = [NSExpression expressionForFunction:_function arguments:args];

        e->_operand = [[_operand _expressionWithSubstitutionVariables:variables] retain];
        return e;
    }
    case NSAggregateExpressionType: {
        NSMutableArray *subs = [NSMutableArray array];

        for (NSExpression *sub in _collection)
            [subs addObject:[sub _expressionWithSubstitutionVariables:variables]];
        return [NSExpression expressionForAggregate:subs];
    }
    default:
        return self;
    }
}

- (NSString *)description {
    return PDExpressionFormat(self);
}

@end

// how tightly an arithmetic operator binds, 0 for anything that is not one
static int PDPrecedence(NSString *op) {
    if ([op isEqualToString:@"+"] || [op isEqualToString:@"-"])
        return 1;
    if ([op isEqualToString:@"*"] || [op isEqualToString:@"/"])
        return 2;
    if ([op isEqualToString:@"**"])
        return 3;
    return 0;
}

// an operand of an infix operator, bracketed when it would otherwise read back differently
static NSString *PDOperandFormat(NSExpression *operand, NSString *op, BOOL right) {
    NSString *text = PDExpressionFormat(operand);
    NSString *inner = operand->_type == NSFunctionExpressionType && [operand->_arguments count] == 2 ? PDInfixFor(operand->_function) : nil;
    int mine = PDPrecedence(op), theirs = PDPrecedence(inner);

    if (inner == nil)
        return text;
    if (theirs < mine || (right && theirs == mine && ![op isEqualToString:@"+"] && ![op isEqualToString:@"*"]))
        return [NSString stringWithFormat:@"(%@)", text];
    return text;
}

static NSString *PDConstantFormat(id value) {
    value = PDNilIfNull(value);
    if (value == nil)
        return @"nil";
    if ([value isKindOfClass:[NSString class]])
        return PDQuote(value);
    if ([value isKindOfClass:[NSNumber class]])
        return [value description];
    if (PDIsCollection(value)) {
        NSMutableArray *parts = [NSMutableArray array];

        for (id v in PDCollectionObjects(value))
            [parts addObject:PDConstantFormat(v)];
        return [NSString stringWithFormat:@"{%@}", [parts componentsJoinedByString:@", "]];
    }
    if ([value isKindOfClass:[NSDate class]])
        return [NSString stringWithFormat:@"CAST(%f, \"NSDate\")", [value timeIntervalSinceReferenceDate]];
    return [value description];
}

static NSString *PDExpressionFormat(NSExpression *e) {
    switch (e->_type) {
    case NSConstantValueExpressionType:
        return PDConstantFormat(e->_constant);
    case NSEvaluatedObjectExpressionType:
        return @"SELF";
    case NSVariableExpressionType:
        return [@"$" stringByAppendingString:e->_variable];
    case NSKeyPathExpressionType:
        if (e->_operand != nil)
            return [NSString stringWithFormat:@"%@.%@", PDExpressionFormat(e->_operand), e->_keyPath];
        return e->_keyPath;
    case NSFunctionExpressionType: {
        NSString *op = PDInfixFor(e->_function);
        NSMutableArray *args = [NSMutableArray array];

        if (op != nil && [e->_arguments count] == 2)
            return [NSString stringWithFormat:@"%@ %@ %@", PDOperandFormat(e->_arguments[0], op, NO), op,
                                              PDOperandFormat(e->_arguments[1], op, YES)];
        if ([e->_function isEqualToString:@"objectFrom:withIndex:"] && [e->_arguments count] == 2) {
            NSExpression *index = e->_arguments[1];

            return [NSString stringWithFormat:@"%@[%@]", PDExpressionFormat(e->_arguments[0]),
                                              index->_type == NSConstantValueExpressionType && [index->_constant isKindOfClass:[NSString class]]
                                                  ? index->_constant : PDExpressionFormat(index)];
        }
        if ([e->_function isEqualToString:@"valueForKeyPath:"] && [e->_arguments count] == 2)
            return [NSString stringWithFormat:@"%@.%@", PDExpressionFormat(e->_arguments[0]),
                                              ((NSExpression *)e->_arguments[1])->_constant];
        for (NSExpression *arg in e->_arguments)
            [args addObject:PDExpressionFormat(arg)];
        if (e->_operand != nil)
            return [NSString stringWithFormat:@"FUNCTION(%@, \"%@\"%@%@)", PDExpressionFormat(e->_operand), e->_function,
                                              [args count] > 0 ? @", " : @"", [args componentsJoinedByString:@", "]];
        return [NSString stringWithFormat:@"%@(%@)", e->_function, [args componentsJoinedByString:@", "]];
    }
    case NSAggregateExpressionType: {
        NSMutableArray *parts = [NSMutableArray array];

        for (NSExpression *sub in e->_collection)
            [parts addObject:PDExpressionFormat(sub)];
        return [NSString stringWithFormat:@"{%@}", [parts componentsJoinedByString:@", "]];
    }
    case NSUnionSetExpressionType:
        return [NSString stringWithFormat:@"%@ UNION %@", PDExpressionFormat(e->_left), PDExpressionFormat(e->_right)];
    case NSIntersectSetExpressionType:
        return [NSString stringWithFormat:@"%@ INTERSECT %@", PDExpressionFormat(e->_left), PDExpressionFormat(e->_right)];
    case NSMinusSetExpressionType:
        return [NSString stringWithFormat:@"%@ MINUS %@", PDExpressionFormat(e->_left), PDExpressionFormat(e->_right)];
    case NSSubqueryExpressionType:
        return [NSString stringWithFormat:@"SUBQUERY(%@, $%@, %@)", PDExpressionFormat(e->_collection), e->_variable,
                                          [e->_predicate predicateFormat]];
    case NSConditionalExpressionType:
        return [NSString stringWithFormat:@"TERNARY(%@, %@, %@)", [e->_predicate predicateFormat],
                                          PDExpressionFormat(e->_left), PDExpressionFormat(e->_right)];
    case NSBlockExpressionType:
        return [NSString stringWithFormat:@"BLOCK(%p, %@)", e->_block, e->_arguments];
    case NSAnyKeyExpressionType:
        return @"ANYKEY";
    default:
        return @"<expression>";
    }
}

#pragma mark - tokens and parser

typedef enum {
    PDTokEnd,
    PDTokIdent,
    PDTokNumber,
    PDTokString,
    PDTokVariable,
    PDTokFormat,
    PDTokPunct,
} PDTokType;

typedef struct {
    PDTokType type;
    NSString *text;
    id value;
    BOOL escaped;
} PDToken;

@interface PDPredicateParser : NSObject {
    NSString *_source;
    NSUInteger _pos;
    NSUInteger _length;
    NSArray *_argArray;
    NSUInteger _argIndex;
    va_list *_args;
    PDToken _tok;
    NSUInteger _tokStart;
}
- (instancetype)initWithFormat:(NSString *)format argumentArray:(NSArray *)array arguments:(va_list *)args;
- (NSPredicate *)parsePredicateToEnd;
- (NSExpression *)parseExpressionToEnd;
@end

@implementation PDPredicateParser

- (instancetype)initWithFormat:(NSString *)format argumentArray:(NSArray *)array arguments:(va_list *)args {
    if ((self = [super init]) == nil)
        return nil;
    _source = [format copy];
    _length = [_source length];
    _argArray = [array retain];
    _args = args;
    [self advance];
    return self;
}

- (void)dealloc {
    [_source release];
    [_argArray release];
    [super dealloc];
}

- (void)fail:(NSString *)why {
    PDPredicateRaise(@"Unable to parse the format string \"%@\" (%@)", _source, why);
}

- (unichar)peekChar:(NSUInteger)offset {
    return _pos + offset < _length ? [_source characterAtIndex:_pos + offset] : 0;
}

// the next argument for a % specifier, from the array or the va_list
- (id)nextArgument:(NSString *)spec {
    if (_argArray != nil) {
        if (_argIndex >= [_argArray count])
            [self fail:@"too few arguments"];
        return PDNilIfNull(_argArray[_argIndex++]);
    }
    if (_args == NULL)
        [self fail:@"no arguments"];
    if ([spec isEqualToString:@"@"] || [spec isEqualToString:@"K"])
        return va_arg(*_args, id);
    if ([spec isEqualToString:@"s"]) {
        const char *s = va_arg(*_args, const char *);

        return s != NULL ? [NSString stringWithUTF8String:s] : nil;
    }
    if ([spec hasSuffix:@"f"] || [spec hasSuffix:@"g"] || [spec hasSuffix:@"e"])
        return [NSNumber numberWithDouble:va_arg(*_args, double)];
    if ([spec hasPrefix:@"ll"] || [spec hasPrefix:@"q"])
        return [spec hasSuffix:@"u"] ? [NSNumber numberWithUnsignedLongLong:va_arg(*_args, unsigned long long)]
                                     : [NSNumber numberWithLongLong:va_arg(*_args, long long)];
    if ([spec hasPrefix:@"l"] || [spec hasPrefix:@"z"])
        return [spec hasSuffix:@"u"] ? [NSNumber numberWithUnsignedLong:va_arg(*_args, unsigned long)]
                                     : [NSNumber numberWithLong:va_arg(*_args, long)];
    if ([spec hasSuffix:@"u"])
        return [NSNumber numberWithUnsignedInt:va_arg(*_args, unsigned int)];
    if ([spec hasSuffix:@"c"])
        return [NSNumber numberWithChar:(char)va_arg(*_args, int)];
    return [NSNumber numberWithInt:va_arg(*_args, int)];
}

- (void)advance {
    unichar c;

    _tok.type = PDTokEnd;
    _tok.text = nil;
    _tok.value = nil;
    _tok.escaped = NO;
    while (_pos < _length && [[NSCharacterSet whitespaceAndNewlineCharacterSet] characterIsMember:[self peekChar:0]])
        _pos++;
    _tokStart = _pos;
    if (_pos >= _length)
        return;
    c = [self peekChar:0];

    if (c == '"' || c == '\'') {
        NSMutableString *s = [NSMutableString string];
        unichar quote = c;

        _pos++;
        while (_pos < _length && [self peekChar:0] != quote) {
            unichar ch = [self peekChar:0];

            if (ch == '\\' && _pos + 1 < _length) {
                unichar n = [self peekChar:1];

                _pos += 2;
                switch (n) {
                case 'n': [s appendString:@"\n"]; break;
                case 't': [s appendString:@"\t"]; break;
                case 'r': [s appendString:@"\r"]; break;
                case '0': [s appendFormat:@"%C", (unichar)0]; break;
                case 'u': {
                    unsigned value = 0;

                    for (int i = 0; i < 4 && _pos < _length; i++, _pos++) {
                        unichar h = [self peekChar:0];

                        value = value * 16 + (h <= '9' ? h - '0' : (h | 0x20) - 'a' + 10);
                    }
                    [s appendFormat:@"%C", (unichar)value];
                    break;
                }
                default: [s appendFormat:@"%C", n]; break;
                }
                continue;
            }
            [s appendFormat:@"%C", ch];
            _pos++;
        }
        if (_pos >= _length)
            [self fail:@"unterminated string"];
        _pos++;
        _tok.type = PDTokString;
        _tok.value = s;
        return;
    }
    if ((c >= '0' && c <= '9') || (c == '.' && [self peekChar:1] >= '0' && [self peekChar:1] <= '9')) {
        NSUInteger start = _pos;
        BOOL isFloat = NO;

        if (c == '0' && ([self peekChar:1] == 'x' || [self peekChar:1] == 'X' || [self peekChar:1] == 'o' ||
                         [self peekChar:1] == 'b')) {
            unichar base = [self peekChar:1] | 0x20;
            long long value = 0;
            int radix = base == 'x' ? 16 : base == 'o' ? 8 : 2;

            _pos += 2;
            while (_pos < _length) {
                unichar h = [self peekChar:0] | 0x20;
                int digit = (h >= '0' && h <= '9') ? h - '0' : (h >= 'a' && h <= 'f') ? h - 'a' + 10 : 99;

                if (digit >= radix)
                    break;
                value = value * radix + digit;
                _pos++;
            }
            _tok.type = PDTokNumber;
            _tok.value = [NSNumber numberWithLongLong:value];
            return;
        }
        while (_pos < _length) {
            unichar d = [self peekChar:0];

            if (d >= '0' && d <= '9') {
                _pos++;
            } else if (d == '.' && !isFloat && [self peekChar:1] != '.') {
                isFloat = YES;
                _pos++;
            } else if ((d == 'e' || d == 'E') && ([self peekChar:1] == '-' || [self peekChar:1] == '+' ||
                                                  ([self peekChar:1] >= '0' && [self peekChar:1] <= '9'))) {
                isFloat = YES;
                _pos += 2;
            } else {
                break;
            }
        }
        NSString *text = [_source substringWithRange:NSMakeRange(start, _pos - start)];

        _tok.type = PDTokNumber;
        _tok.value = isFloat ? [NSNumber numberWithDouble:[text doubleValue]] : [NSNumber numberWithLongLong:[text longLongValue]];
        return;
    }
    if (c == '%') {
        NSUInteger start = ++_pos;

        while (_pos < _length) {
            unichar f = [self peekChar:0];

            _pos++;
            if (f == '@' || f == 'K' || f == 'd' || f == 'i' || f == 'u' || f == 'f' || f == 'g' || f == 'e' ||
                f == 's' || f == 'c' || f == 'x' || f == '%')
                break;
        }
        NSString *spec = [_source substringWithRange:NSMakeRange(start, _pos - start)];

        _tok.type = PDTokFormat;
        _tok.text = spec;
        if ([spec isEqualToString:@"%"])
            [self fail:@"stray %%"];
        _tok.value = [self nextArgument:spec];
        return;
    }
    if (c == '$') {
        NSUInteger start = ++_pos;

        while (_pos < _length && ([[NSCharacterSet alphanumericCharacterSet] characterIsMember:[self peekChar:0]] || [self peekChar:0] == '_'))
            _pos++;
        _tok.type = PDTokVariable;
        _tok.text = [_source substringWithRange:NSMakeRange(start, _pos - start)];
        return;
    }
    if ([[NSCharacterSet letterCharacterSet] characterIsMember:c] || c == '_' || c == '@' || c == '#') {
        NSUInteger start = _pos;
        BOOL escaped = (c == '#');

        if (escaped)
            start = ++_pos;
        while (_pos < _length) {
            unichar d = [self peekChar:0];

            if (![[NSCharacterSet alphanumericCharacterSet] characterIsMember:d] && d != '_' && d != '@')
                break;
            _pos++;
        }
        // a function name may end in colons, sum: or add:to:
        while (_pos < _length && [self peekChar:0] == ':' && [self peekChar:1] != '=') {
            _pos++;
            while (_pos < _length && ([[NSCharacterSet alphanumericCharacterSet] characterIsMember:[self peekChar:0]]))
                _pos++;
        }
        _tok.type = PDTokIdent;
        _tok.text = [_source substringWithRange:NSMakeRange(start, _pos - start)];
        _tok.escaped = escaped;
        return;
    }
    // punctuation, longest match first
    static NSString *ops[] = { @"**", @"==", @"!=", @"<>", @"<=", @"=<", @">=", @"=>", @"&&", @"||", @":=" };
    for (unsigned i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        if ([self peekChar:0] == [ops[i] characterAtIndex:0] && [self peekChar:1] == [ops[i] characterAtIndex:1]) {
            _pos += 2;
            _tok.type = PDTokPunct;
            _tok.text = ops[i];
            return;
        }
    }
    _pos++;
    _tok.type = PDTokPunct;
    _tok.text = [NSString stringWithFormat:@"%C", c];
}

- (BOOL)isKeyword:(NSString *)word {
    return _tok.type == PDTokIdent && !_tok.escaped && [_tok.text caseInsensitiveCompare:word] == NSOrderedSame;
}

- (BOOL)isPunct:(NSString *)p {
    return _tok.type == PDTokPunct && [_tok.text isEqualToString:p];
}

- (void)expectPunct:(NSString *)p {
    if (![self isPunct:p])
        [self fail:[NSString stringWithFormat:@"expected %@", p]];
    [self advance];
}

// state saved for the one place the grammar has to look ahead, a parenthesis that may open an expression
- (NSUInteger)mark {
    return _tokStart;
}

- (void)reset:(NSUInteger)mark {
    _pos = mark;
    [self advance];
}

#pragma mark predicates

- (NSPredicate *)parsePredicateToEnd {
    NSPredicate *p = [self parseOr];

    if (_tok.type != PDTokEnd)
        [self fail:@"unexpected text after the predicate"];
    return p;
}

- (NSExpression *)parseExpressionToEnd {
    NSExpression *e = [self parseExpression];

    if (_tok.type != PDTokEnd)
        [self fail:@"unexpected text after the expression"];
    return e;
}

- (NSPredicate *)parseOr {
    NSMutableArray *parts = [NSMutableArray arrayWithObject:[self parseAnd]];

    while ([self isKeyword:@"OR"] || [self isPunct:@"||"]) {
        [self advance];
        [parts addObject:[self parseAnd]];
    }
    return [parts count] == 1 ? parts[0] : [NSCompoundPredicate orPredicateWithSubpredicates:parts];
}

- (NSPredicate *)parseAnd {
    NSMutableArray *parts = [NSMutableArray arrayWithObject:[self parseNot]];

    while ([self isKeyword:@"AND"] || [self isPunct:@"&&"]) {
        [self advance];
        [parts addObject:[self parseNot]];
    }
    return [parts count] == 1 ? parts[0] : [NSCompoundPredicate andPredicateWithSubpredicates:parts];
}

- (NSPredicate *)parseNot {
    if ([self isKeyword:@"NOT"] || [self isPunct:@"!"]) {
        [self advance];
        return [NSCompoundPredicate notPredicateWithSubpredicate:[self parseNot]];
    }
    return [self parsePrimaryPredicate];
}

- (NSPredicate *)parsePrimaryPredicate {
    if ([self isKeyword:@"TRUEPREDICATE"]) {
        [self advance];
        return [NSPredicate predicateWithValue:YES];
    }
    if ([self isKeyword:@"FALSEPREDICATE"]) {
        [self advance];
        return [NSPredicate predicateWithValue:NO];
    }
    if ([self isPunct:@"("]) {
        // a parenthesised predicate, unless an operator follows, then it opened an expression
        NSUInteger mark = [self mark];

        @try {
            [self advance];
            NSPredicate *inner = [self parseOr];

            [self expectPunct:@")"];
            if (![self atComparisonOperator])
                return inner;
        } @catch (NSException *e) {
        }
        [self reset:mark];
    }
    return [self parseComparison];
}

- (BOOL)atComparisonOperator {
    static NSArray *words, *puncts;

    if (words == nil) {
        words = [@[ @"BEGINSWITH", @"ENDSWITH", @"CONTAINS", @"LIKE", @"MATCHES", @"IN", @"BETWEEN" ] retain];
        puncts = [@[ @"=", @"==", @"!=", @"<>", @"<", @">", @"<=", @"=<", @">=", @"=>", @"+", @"-", @"*", @"/", @"**", @"." ] retain];
    }
    for (NSString *w in words) {
        if ([self isKeyword:w])
            return YES;
    }
    return _tok.type == PDTokPunct && [puncts containsObject:_tok.text];
}

- (NSPredicate *)parseComparison {
    NSComparisonPredicateModifier modifier = NSDirectPredicateModifier;
    BOOL none = NO;
    NSPredicateOperatorType type;
    NSComparisonPredicateOptions options = 0;

    if ([self isKeyword:@"ANY"] || [self isKeyword:@"SOME"]) {
        modifier = NSAnyPredicateModifier;
        [self advance];
    } else if ([self isKeyword:@"ALL"]) {
        modifier = NSAllPredicateModifier;
        [self advance];
    } else if ([self isKeyword:@"NONE"]) {
        modifier = NSAnyPredicateModifier;
        none = YES;
        [self advance];
    }

    NSExpression *left = [self parseExpression];

    if ([self isPunct:@"="] || [self isPunct:@"=="])
        type = NSEqualToPredicateOperatorType;
    else if ([self isPunct:@"!="] || [self isPunct:@"<>"])
        type = NSNotEqualToPredicateOperatorType;
    else if ([self isPunct:@"<"])
        type = NSLessThanPredicateOperatorType;
    else if ([self isPunct:@"<="] || [self isPunct:@"=<"])
        type = NSLessThanOrEqualToPredicateOperatorType;
    else if ([self isPunct:@">"])
        type = NSGreaterThanPredicateOperatorType;
    else if ([self isPunct:@">="] || [self isPunct:@"=>"])
        type = NSGreaterThanOrEqualToPredicateOperatorType;
    else if ([self isKeyword:@"BEGINSWITH"])
        type = NSBeginsWithPredicateOperatorType;
    else if ([self isKeyword:@"ENDSWITH"])
        type = NSEndsWithPredicateOperatorType;
    else if ([self isKeyword:@"CONTAINS"])
        type = NSContainsPredicateOperatorType;
    else if ([self isKeyword:@"LIKE"])
        type = NSLikePredicateOperatorType;
    else if ([self isKeyword:@"MATCHES"])
        type = NSMatchesPredicateOperatorType;
    else if ([self isKeyword:@"IN"])
        type = NSInPredicateOperatorType;
    else if ([self isKeyword:@"BETWEEN"])
        type = NSBetweenPredicateOperatorType;
    else {
        [self fail:@"expected a comparison operator"];
        return nil;
    }
    [self advance];

    // [c], [d], [cd] and friends, read straight from the source since c and d are not tokens of their own
    if ([self isPunct:@"["]) {
        NSUInteger close = [_source rangeOfString:@"]" options:0 range:NSMakeRange(_pos, _length - _pos)].location;

        if (close == NSNotFound)
            [self fail:@"unterminated option list"];
        NSString *flags = [[_source substringWithRange:NSMakeRange(_pos, close - _pos)] lowercaseString];

        if ([flags rangeOfString:@"c"].location != NSNotFound)
            options |= NSCaseInsensitivePredicateOption;
        if ([flags rangeOfString:@"d"].location != NSNotFound)
            options |= NSDiacriticInsensitivePredicateOption;
        if ([flags rangeOfString:@"n"].location != NSNotFound)
            options |= NSNormalizedPredicateOption;
        _pos = close + 1;
        [self advance];
    }

    NSExpression *right = [self parseExpression];
    NSPredicate *p = [NSComparisonPredicate predicateWithLeftExpression:left rightExpression:right
                                                               modifier:modifier type:type options:options];

    return none ? [NSCompoundPredicate notPredicateWithSubpredicate:p] : p;
}

#pragma mark expressions

- (NSExpression *)parseExpression {
    NSExpression *e = [self parseMultiplicative];

    while ([self isPunct:@"+"] || [self isPunct:@"-"]) {
        NSString *fn = [self isPunct:@"+"] ? @"add:to:" : @"from:subtract:";

        [self advance];
        e = [NSExpression expressionForFunction:fn arguments:@[ e, [self parseMultiplicative] ]];
    }
    return e;
}

- (NSExpression *)parseMultiplicative {
    NSExpression *e = [self parsePower];

    while ([self isPunct:@"*"] || [self isPunct:@"/"]) {
        NSString *fn = [self isPunct:@"*"] ? @"multiply:by:" : @"divide:by:";

        [self advance];
        e = [NSExpression expressionForFunction:fn arguments:@[ e, [self parsePower] ]];
    }
    return e;
}

- (NSExpression *)parsePower {
    NSExpression *e = [self parseUnary];

    while ([self isPunct:@"**"]) {
        [self advance];
        e = [NSExpression expressionForFunction:@"raise:toPower:" arguments:@[ e, [self parseUnary] ]];
    }
    return e;
}

- (NSExpression *)parseUnary {
    if ([self isPunct:@"-"]) {
        [self advance];
        NSExpression *operand = [self parseUnary];

        if (operand->_type == NSConstantValueExpressionType && [operand->_constant isKindOfClass:[NSNumber class]]) {
            NSNumber *n = operand->_constant;

            return [NSExpression expressionForConstantValue:strcmp([n objCType], "d") == 0 || strcmp([n objCType], "f") == 0
                                                                ? [NSNumber numberWithDouble:-[n doubleValue]]
                                                                : [NSNumber numberWithLongLong:-[n longLongValue]]];
        }
        return [NSExpression expressionForFunction:@"from:subtract:"
                                         arguments:@[ [NSExpression expressionForConstantValue:@0], operand ]];
    }
    return [self parsePostfix:[self parsePrimary]];
}

- (NSExpression *)parsePostfix:(NSExpression *)e {
    while (YES) {
        if ([self isPunct:@"."]) {
            [self advance];
            if (_tok.type != PDTokIdent)
                [self fail:@"expected a key after ."];
            NSString *key = _tok.text;

            [self advance];
            if (e->_type == NSKeyPathExpressionType && e->_operand == nil) {
                e = [NSExpression expressionForKeyPath:[NSString stringWithFormat:@"%@.%@", e->_keyPath, key]];
            } else if (e->_type == NSEvaluatedObjectExpressionType) {
                e = [NSExpression expressionForKeyPath:key];
            } else {
                e = [NSExpression expressionForFunction:@"valueForKeyPath:"
                                              arguments:@[ e, [NSExpression expressionForConstantValue:key] ]];
            }
            continue;
        }
        if ([self isPunct:@"["]) {
            NSExpression *index;

            [self advance];
            if ([self isKeyword:@"FIRST"] || [self isKeyword:@"LAST"] || [self isKeyword:@"SIZE"]) {
                index = [NSExpression expressionForConstantValue:[_tok.text uppercaseString]];
                [self advance];
            } else {
                index = [self parseExpression];
            }
            [self expectPunct:@"]"];
            e = [NSExpression expressionForFunction:@"objectFrom:withIndex:" arguments:@[ e, index ]];
            continue;
        }
        return e;
    }
}

- (NSArray *)parseArgumentList {
    NSMutableArray *args = [NSMutableArray array];

    [self expectPunct:@"("];
    if (![self isPunct:@")"]) {
        [args addObject:[self parseExpression]];
        while ([self isPunct:@","]) {
            [self advance];
            [args addObject:[self parseExpression]];
        }
    }
    [self expectPunct:@")"];
    return args;
}

// the built-in functions by bare name, sum(x) meaning sum:(x)
static NSString *PDFunctionNamed(NSString *name, NSUInteger arity) {
    static NSDictionary *names;

    if ([name hasSuffix:@":"] || [name rangeOfString:@":"].location != NSNotFound)
        return name;
    if (names == nil)
        names = [@{ @"sum": @"sum:", @"count": @"count:", @"min": @"min:", @"max": @"max:", @"average": @"average:",
                    @"sqrt": @"sqrt:", @"abs": @"abs:", @"floor": @"floor:", @"ceiling": @"ceiling:", @"trunc": @"trunc:",
                    @"exp": @"exp:", @"ln": @"ln:", @"log": @"log:", @"uppercase": @"uppercase:", @"lowercase": @"lowercase:",
                    @"now": @"now", @"noindex": @"noindex:", @"add": @"add:to:", @"subtract": @"from:subtract:",
                    @"multiply": @"multiply:by:", @"divide": @"divide:by:", @"modulus": @"modulus:by:" } retain];
    return names[name] ?: (arity == 0 ? name : [name stringByAppendingString:@":"]);
}

- (NSExpression *)parsePrimary {
    switch (_tok.type) {
    case PDTokNumber: {
        id value = _tok.value;

        [self advance];
        return [NSExpression expressionForConstantValue:value];
    }
    case PDTokString: {
        id value = _tok.value;

        [self advance];
        return [NSExpression expressionForConstantValue:value];
    }
    case PDTokVariable: {
        NSString *name = _tok.text;

        [self advance];
        return [NSExpression expressionForVariable:name];
    }
    case PDTokFormat: {
        NSString *spec = _tok.text;
        id value = _tok.value;

        [self advance];
        if ([spec isEqualToString:@"K"]) {
            if (![value isKindOfClass:[NSString class]])
                [self fail:@"%K needs a key path string"];
            return [NSExpression expressionForKeyPath:value];
        }
        return [NSExpression expressionForConstantValue:value];
    }
    case PDTokPunct: {
        if ([self isPunct:@"("]) {
            [self advance];
            NSExpression *e = [self parseExpression];

            [self expectPunct:@")"];
            return e;
        }
        if ([self isPunct:@"{"]) {
            NSMutableArray *items = [NSMutableArray array];

            [self advance];
            if (![self isPunct:@"}"]) {
                [items addObject:[self parseExpression]];
                while ([self isPunct:@","]) {
                    [self advance];
                    [items addObject:[self parseExpression]];
                }
            }
            [self expectPunct:@"}"];
            return [NSExpression expressionForAggregate:items];
        }
        [self fail:[NSString stringWithFormat:@"unexpected %@", _tok.text]];
        return nil;
    }
    case PDTokIdent: {
        NSString *word = _tok.text;

        if (!_tok.escaped) {
            if ([self isKeyword:@"SELF"]) {
                [self advance];
                return [NSExpression expressionForEvaluatedObject];
            }
            if ([self isKeyword:@"nil"] || [self isKeyword:@"NULL"]) {
                [self advance];
                return [NSExpression expressionForConstantValue:nil];
            }
            if ([self isKeyword:@"TRUE"] || [self isKeyword:@"YES"]) {
                [self advance];
                return [NSExpression expressionForConstantValue:@YES];
            }
            if ([self isKeyword:@"FALSE"] || [self isKeyword:@"NO"]) {
                [self advance];
                return [NSExpression expressionForConstantValue:@NO];
            }
            if ([self isKeyword:@"SUBQUERY"]) {
                [self advance];
                [self expectPunct:@"("];
                NSExpression *collection = [self parseExpression];

                [self expectPunct:@","];
                if (_tok.type != PDTokVariable)
                    [self fail:@"SUBQUERY needs a $variable"];
                NSString *variable = _tok.text;

                [self advance];
                [self expectPunct:@","];
                NSPredicate *predicate = [self parseOr];

                [self expectPunct:@")"];
                NSExpression *e = [[[NSExpression alloc] initWithExpressionType:NSSubqueryExpressionType] autorelease];

                e->_collection = [collection retain];
                e->_variable = [variable copy];
                e->_predicate = [predicate retain];
                return e;
            }
            if ([self isKeyword:@"FUNCTION"]) {
                [self advance];
                NSArray *args = [self parseArgumentList];

                if ([args count] < 2)
                    [self fail:@"FUNCTION needs a target and a selector"];
                NSExpression *sel = args[1];
                NSExpression *e = [NSExpression expressionForFunction:sel->_constant
                                                            arguments:[args subarrayWithRange:NSMakeRange(2, [args count] - 2)]];

                e->_operand = [args[0] retain];
                return e;
            }
            if ([self isKeyword:@"TERNARY"]) {
                [self advance];
                [self expectPunct:@"("];
                NSPredicate *predicate = [self parseOr];

                [self expectPunct:@","];
                NSExpression *yes = [self parseExpression];

                [self expectPunct:@","];
                NSExpression *no = [self parseExpression];

                [self expectPunct:@")"];
                return [NSExpression expressionForConditional:predicate trueExpression:yes falseExpression:no];
            }
        }
        [self advance];
        if ([self isPunct:@"("]) {
            NSArray *args = [self parseArgumentList];

            return [NSExpression expressionForFunction:PDFunctionNamed(word, [args count]) arguments:args];
        }
        return [NSExpression expressionForKeyPath:word];
    }
    default:
        [self fail:@"unexpected end"];
        return nil;
    }
}

@end

#pragma mark - NSPredicate

@interface PDBlockPredicate : NSPredicate {
@public
    BOOL (^_block)(id, NSDictionary *);
}
@end

@implementation NSPredicate {
    BOOL _value;
}

+ (NSPredicate *)predicateWithFormat:(NSString *)format, ... {
    va_list args;
    NSPredicate *result;

    va_start(args, format);
    result = [self predicateWithFormat:format arguments:args];
    va_end(args);
    return result;
}

+ (NSPredicate *)predicateWithFormat:(NSString *)format arguments:(va_list)arguments {
    va_list copy;
    NSPredicate *result;

    va_copy(copy, arguments);
    PDPredicateParser *parser = [[[PDPredicateParser alloc] initWithFormat:format argumentArray:nil arguments:&copy] autorelease];

    result = [parser parsePredicateToEnd];
    va_end(copy);
    return result;
}

+ (NSPredicate *)predicateWithFormat:(NSString *)format argumentArray:(NSArray *)arguments {
    PDPredicateParser *parser = [[[PDPredicateParser alloc] initWithFormat:format argumentArray:arguments ?: @[] arguments:NULL] autorelease];

    return [parser parsePredicateToEnd];
}

+ (NSPredicate *)predicateWithValue:(BOOL)value {
    NSPredicate *predicate = [[[NSPredicate alloc] init] autorelease];

    predicate->_value = value;
    return predicate;
}

+ (NSPredicate *)predicateWithBlock:(BOOL (^)(id, NSDictionary *))block {
    PDBlockPredicate *p = [[[PDBlockPredicate alloc] init] autorelease];

    p->_block = [block copy];
    return p;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _value = YES;
    return self;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

- (BOOL)evaluateWithObject:(id)object {
    return [self evaluateWithObject:object substitutionVariables:nil];
}

- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables {
    return _value;
}

- (NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables {
    return self;
}

- (NSString *)predicateFormat {
    return _value ? @"TRUEPREDICATE" : @"FALSEPREDICATE";
}

- (NSString *)description {
    return [self predicateFormat];
}

@end

@implementation PDBlockPredicate

- (void)dealloc {
    [_block release];
    [super dealloc];
}

- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables {
    return _block(object, variables);
}

- (NSString *)predicateFormat {
    return [NSString stringWithFormat:@"BLOCKPREDICATE(%p)", _block];
}

@end

#pragma mark - NSComparisonPredicate

// a string with case and diacritics folded away as the options ask
static NSString *PDFold(NSString *s, NSComparisonPredicateOptions options) {
    CFOptionFlags flags = 0;
    CFMutableStringRef copy;

    if (s == nil || (options & (NSCaseInsensitivePredicateOption | NSDiacriticInsensitivePredicateOption)) == 0)
        return s;
    if (options & NSCaseInsensitivePredicateOption)
        flags |= kCFCompareCaseInsensitive;
    if (options & NSDiacriticInsensitivePredicateOption)
        flags |= kCFCompareDiacriticInsensitive;
    copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, (CFStringRef)s);
    CFStringFold(copy, flags, NULL);
    return [(NSString *)copy autorelease];
}

// LIKE's wildcards, * for any run and ? for one character
static BOOL PDLike(NSString *s, NSUInteger si, NSString *p, NSUInteger pi) {
    NSUInteger sl = [s length], pl = [p length];

    while (pi < pl) {
        unichar pc = [p characterAtIndex:pi];

        if (pc == '*') {
            while (pi < pl && [p characterAtIndex:pi] == '*')
                pi++;
            if (pi == pl)
                return YES;
            for (NSUInteger k = si; k <= sl; k++) {
                if (PDLike(s, k, p, pi))
                    return YES;
            }
            return NO;
        }
        if (si >= sl)
            return NO;
        if (pc == '\\' && pi + 1 < pl) {
            pi++;
            pc = [p characterAtIndex:pi];
            if ([s characterAtIndex:si] != pc)
                return NO;
        } else if (pc != '?' && [s characterAtIndex:si] != pc) {
            return NO;
        }
        si++;
        pi++;
    }
    return si == sl;
}

@implementation NSComparisonPredicate {
    NSExpression *_lhs;
    NSExpression *_rhs;
    NSComparisonPredicateModifier _modifier;
    NSPredicateOperatorType _type;
    NSComparisonPredicateOptions _options;
    SEL _selector;
}

+ (NSPredicate *)predicateWithLeftExpression:(NSExpression *)lhs rightExpression:(NSExpression *)rhs
                                    modifier:(NSComparisonPredicateModifier)modifier type:(NSPredicateOperatorType)type
                                     options:(NSComparisonPredicateOptions)options {
    return [[[self alloc] initWithLeftExpression:lhs rightExpression:rhs modifier:modifier type:type options:options] autorelease];
}

+ (NSPredicate *)predicateWithLeftExpression:(NSExpression *)lhs rightExpression:(NSExpression *)rhs customSelector:(SEL)selector {
    return [[[self alloc] initWithLeftExpression:lhs rightExpression:rhs customSelector:selector] autorelease];
}

- (instancetype)initWithLeftExpression:(NSExpression *)lhs rightExpression:(NSExpression *)rhs
                              modifier:(NSComparisonPredicateModifier)modifier type:(NSPredicateOperatorType)type
                               options:(NSComparisonPredicateOptions)options {
    if ((self = [super init]) == nil)
        return nil;
    _lhs = [lhs retain];
    _rhs = [rhs retain];
    _modifier = modifier;
    _type = type;
    _options = options;
    return self;
}

- (instancetype)initWithLeftExpression:(NSExpression *)lhs rightExpression:(NSExpression *)rhs customSelector:(SEL)selector {
    if ((self = [self initWithLeftExpression:lhs rightExpression:rhs modifier:NSDirectPredicateModifier
                                        type:NSCustomSelectorPredicateOperatorType options:0]) != nil)
        _selector = selector;
    return self;
}

- (void)dealloc {
    [_lhs release];
    [_rhs release];
    [super dealloc];
}

- (NSPredicateOperatorType)predicateOperatorType {
    return _type;
}

- (NSComparisonPredicateModifier)comparisonPredicateModifier {
    return _modifier;
}

- (NSExpression *)leftExpression {
    return _lhs;
}

- (NSExpression *)rightExpression {
    return _rhs;
}

- (SEL)customSelector {
    return _selector;
}

- (NSComparisonPredicateOptions)options {
    return _options;
}

- (BOOL)compareEqual:(id)l to:(id)r {
    l = PDNilIfNull(l);
    r = PDNilIfNull(r);
    if (l == nil || r == nil)
        return l == r;
    if ([l isKindOfClass:[NSString class]] && [r isKindOfClass:[NSString class]])
        return [PDFold(l, _options) isEqualToString:PDFold(r, _options)];
    return [l isEqual:r];
}

- (NSComparisonResult)compare:(id)l to:(id)r ok:(BOOL *)ok {
    l = PDNilIfNull(l);
    r = PDNilIfNull(r);
    *ok = NO;
    if (l == nil || r == nil)
        return NSOrderedSame;
    if ([l isKindOfClass:[NSString class]] && [r isKindOfClass:[NSString class]]) {
        *ok = YES;
        return [PDFold(l, _options) compare:PDFold(r, _options)];
    }
    if ([l respondsToSelector:@selector(compare:)]) {
        *ok = YES;
        return [l compare:r];
    }
    return NSOrderedSame;
}

- (BOOL)evaluateValue:(id)l against:(id)r {
    BOOL ok;
    NSComparisonResult order;

    switch (_type) {
    case NSEqualToPredicateOperatorType:
        return [self compareEqual:l to:r];
    case NSNotEqualToPredicateOperatorType:
        return ![self compareEqual:l to:r];
    case NSLessThanPredicateOperatorType:
        order = [self compare:l to:r ok:&ok];
        return ok && order == NSOrderedAscending;
    case NSLessThanOrEqualToPredicateOperatorType:
        order = [self compare:l to:r ok:&ok];
        return ok && order != NSOrderedDescending;
    case NSGreaterThanPredicateOperatorType:
        order = [self compare:l to:r ok:&ok];
        return ok && order == NSOrderedDescending;
    case NSGreaterThanOrEqualToPredicateOperatorType:
        order = [self compare:l to:r ok:&ok];
        return ok && order != NSOrderedAscending;
    case NSBeginsWithPredicateOperatorType:
    case NSEndsWithPredicateOperatorType:
    case NSLikePredicateOperatorType:
    case NSMatchesPredicateOperatorType: {
        l = PDNilIfNull(l);
        r = PDNilIfNull(r);
        if (![l isKindOfClass:[NSString class]] || ![r isKindOfClass:[NSString class]])
            return NO;
        if (_type == NSMatchesPredicateOperatorType) {
            NSRegularExpressionOptions o = (_options & NSCaseInsensitivePredicateOption) ? NSRegularExpressionCaseInsensitive : 0;
            NSString *pattern = [NSString stringWithFormat:@"^(?:%@)$", r];
            NSRegularExpression *re = [NSRegularExpression regularExpressionWithPattern:pattern options:o error:NULL];

            if (re == nil)
                PDPredicateRaise(@"Can't do regex matching, reason: (Can't open pattern U_REGEX_RULE_SYNTAX (string %@, pattern %@))", l, r);
            NSString *subject = (_options & NSDiacriticInsensitivePredicateOption) ? PDFold(l, NSDiacriticInsensitivePredicateOption) : l;
            return [re numberOfMatchesInString:subject options:0 range:NSMakeRange(0, [subject length])] > 0;
        }
        NSString *fl = PDFold(l, _options), *fr = PDFold(r, _options);

        if (_type == NSBeginsWithPredicateOperatorType)
            return [fl hasPrefix:fr];
        if (_type == NSEndsWithPredicateOperatorType)
            return [fl hasSuffix:fr];
        return PDLike(fl, 0, fr, 0);
    }
    case NSContainsPredicateOperatorType: {
        l = PDNilIfNull(l);
        if (PDIsCollection(l)) {
            for (id item in PDCollectionObjects(l)) {
                if ([self compareEqual:item to:r])
                    return YES;
            }
            return NO;
        }
        r = PDNilIfNull(r);
        if (![l isKindOfClass:[NSString class]] || ![r isKindOfClass:[NSString class]])
            return NO;
        if ([r length] == 0)
            return NO;
        return [PDFold(l, _options) rangeOfString:PDFold(r, _options)].location != NSNotFound;
    }
    case NSInPredicateOperatorType: {
        r = PDNilIfNull(r);
        if (PDIsCollection(r)) {
            for (id item in PDCollectionObjects(r)) {
                if ([self compareEqual:l to:item])
                    return YES;
            }
            return NO;
        }
        l = PDNilIfNull(l);
        if ([l isKindOfClass:[NSString class]] && [r isKindOfClass:[NSString class]] && [l length] > 0)
            return [PDFold(r, _options) rangeOfString:PDFold(l, _options)].location != NSNotFound;
        return NO;
    }
    case NSBetweenPredicateOperatorType: {
        NSArray *bounds = PDCollectionObjects(PDNilIfNull(r));

        if ([bounds count] != 2)
            PDPredicateRaise(@"BETWEEN needs a two-element array, got %@", r);
        NSComparisonResult low = [self compare:l to:bounds[0] ok:&ok];

        if (!ok || low == NSOrderedAscending)
            return NO;
        NSComparisonResult high = [self compare:l to:bounds[1] ok:&ok];

        return ok && high != NSOrderedDescending;
    }
    case NSCustomSelectorPredicateOperatorType:
        return (BOOL)(intptr_t)[PDNilIfNull(l) performSelector:_selector withObject:PDNilIfNull(r)];
    default:
        return NO;
    }
}

- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables {
    NSMutableDictionary *context = variables != nil ? [[variables mutableCopy] autorelease] : [NSMutableDictionary dictionary];
    id l = [_lhs expressionValueWithObject:object context:context];
    id r = [_rhs expressionValueWithObject:object context:context];

    if (_modifier == NSDirectPredicateModifier)
        return [self evaluateValue:l against:r];
    // a missing collection is simply false, an object that is not one is a programming error
    if (PDNilIfNull(l) == nil)
        return NO;
    if (!PDIsCollection(l))
        PDPredicateRaise(@"The left hand side for an ALL or ANY operator must be either an NSArray or an NSSet.");
    for (id item in PDCollectionObjects(l)) {
        BOOL hit = [self evaluateValue:item against:r];

        if (_modifier == NSAnyPredicateModifier && hit)
            return YES;
        if (_modifier == NSAllPredicateModifier && !hit)
            return NO;
    }
    return _modifier == NSAllPredicateModifier;
}

- (NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables {
    NSComparisonPredicate *p = [[[NSComparisonPredicate alloc] initWithLeftExpression:[_lhs _expressionWithSubstitutionVariables:variables]
                                                                      rightExpression:[_rhs _expressionWithSubstitutionVariables:variables]
                                                                             modifier:_modifier type:_type options:_options] autorelease];

    p->_selector = _selector;
    return p;
}

- (NSString *)predicateFormat {
    static NSString *names[] = { @"<", @"<=", @">", @">=", @"==", @"!=", @"MATCHES", @"LIKE", @"BEGINSWITH", @"ENDSWITH", @"IN" };
    NSString *op;
    NSMutableString *flags = [NSMutableString string];

    if (_type == NSCustomSelectorPredicateOperatorType)
        return [NSString stringWithFormat:@"%@ %@ %@", PDExpressionFormat(_lhs), NSStringFromSelector(_selector), PDExpressionFormat(_rhs)];
    if (_type == NSContainsPredicateOperatorType)
        op = @"CONTAINS";
    else if (_type == NSBetweenPredicateOperatorType)
        op = @"BETWEEN";
    else
        op = _type < sizeof(names) / sizeof(names[0]) ? names[_type] : @"?";
    if (_options & NSCaseInsensitivePredicateOption)
        [flags appendString:@"c"];
    if (_options & NSDiacriticInsensitivePredicateOption)
        [flags appendString:@"d"];
    if (_options & NSNormalizedPredicateOption)
        [flags appendString:@"n"];
    return [NSString stringWithFormat:@"%@%@ %@%@ %@",
                     _modifier == NSAnyPredicateModifier ? @"ANY " : _modifier == NSAllPredicateModifier ? @"ALL " : @"",
                     PDExpressionFormat(_lhs), op, [flags length] > 0 ? [NSString stringWithFormat:@"[%@]", flags] : @"",
                     PDExpressionFormat(_rhs)];
}

@end

#pragma mark - NSCompoundPredicate

@implementation NSCompoundPredicate {
    NSCompoundPredicateType _type;
    NSArray *_subpredicates;
}

+ (NSCompoundPredicate *)andPredicateWithSubpredicates:(NSArray *)subpredicates {
    return [[[self alloc] initWithType:NSAndPredicateType subpredicates:subpredicates] autorelease];
}

+ (NSCompoundPredicate *)orPredicateWithSubpredicates:(NSArray *)subpredicates {
    return [[[self alloc] initWithType:NSOrPredicateType subpredicates:subpredicates] autorelease];
}

+ (NSCompoundPredicate *)notPredicateWithSubpredicate:(NSPredicate *)predicate {
    return [[[self alloc] initWithType:NSNotPredicateType subpredicates:@[ predicate ]] autorelease];
}

- (instancetype)initWithType:(NSCompoundPredicateType)type subpredicates:(NSArray *)subpredicates {
    if ((self = [super init]) != nil) {
        _type = type;
        _subpredicates = [subpredicates copy];
    }
    return self;
}

- (void)dealloc {
    [_subpredicates release];
    [super dealloc];
}

- (NSCompoundPredicateType)compoundPredicateType {
    return _type;
}

- (NSArray *)subpredicates {
    return _subpredicates;
}

- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables {
    switch (_type) {
    case NSNotPredicateType:
        return ![[_subpredicates firstObject] evaluateWithObject:object substitutionVariables:variables];
    case NSAndPredicateType:
        for (NSPredicate *p in _subpredicates) {
            if (![p evaluateWithObject:object substitutionVariables:variables])
                return NO;
        }
        return YES;
    case NSOrPredicateType:
        for (NSPredicate *p in _subpredicates) {
            if ([p evaluateWithObject:object substitutionVariables:variables])
                return YES;
        }
        return NO;
    }
    return NO;
}

- (NSPredicate *)predicateWithSubstitutionVariables:(NSDictionary *)variables {
    NSMutableArray *subs = [NSMutableArray array];

    for (NSPredicate *p in _subpredicates)
        [subs addObject:[p predicateWithSubstitutionVariables:variables]];
    return [[[NSCompoundPredicate alloc] initWithType:_type subpredicates:subs] autorelease];
}

- (NSString *)predicateFormat {
    NSMutableArray *parts = [NSMutableArray array];

    if ([_subpredicates count] == 0)
        return _type == NSOrPredicateType ? @"FALSEPREDICATE" : @"TRUEPREDICATE";
    for (NSPredicate *p in _subpredicates) {
        NSString *f = [p predicateFormat];

        // a nested compound is bracketed so it reads back the same
        if ([p isKindOfClass:[NSCompoundPredicate class]] && ([_subpredicates count] > 1 || _type == NSNotPredicateType))
            f = [NSString stringWithFormat:@"(%@)", f];
        [parts addObject:f];
    }
    if (_type == NSNotPredicateType)
        return [@"NOT " stringByAppendingString:parts[0]];
    return [parts componentsJoinedByString:_type == NSAndPredicateType ? @" AND " : @" OR "];
}

@end

#pragma mark - NSExpression from a format

@implementation NSExpression (PDFormat)

+ (NSExpression *)expressionWithFormat:(NSString *)format, ... {
    va_list args;
    NSExpression *e;

    va_start(args, format);
    e = [self expressionWithFormat:format arguments:args];
    va_end(args);
    return e;
}

+ (NSExpression *)expressionWithFormat:(NSString *)format arguments:(va_list)arguments {
    va_list copy;
    NSExpression *e;

    va_copy(copy, arguments);
    e = [[[[PDPredicateParser alloc] initWithFormat:format argumentArray:nil arguments:&copy] autorelease] parseExpressionToEnd];
    va_end(copy);
    return e;
}

+ (NSExpression *)expressionWithFormat:(NSString *)format argumentArray:(NSArray *)arguments {
    return [[[[PDPredicateParser alloc] initWithFormat:format argumentArray:arguments ?: @[] arguments:NULL] autorelease] parseExpressionToEnd];
}

@end

#pragma mark - collections

@implementation NSArray (NSPredicateSupport)

- (NSArray *)filteredArrayUsingPredicate:(NSPredicate *)predicate {
    NSMutableArray *result = [NSMutableArray array];

    for (id object in self) {
        if ([predicate evaluateWithObject:object])
            [result addObject:object];
    }
    return result;
}

@end

@implementation NSMutableArray (NSPredicateSupport)

- (void)filterUsingPredicate:(NSPredicate *)predicate {
    NSUInteger i = [self count];

    while (i-- > 0) {
        if (![predicate evaluateWithObject:[self objectAtIndex:i]])
            [self removeObjectAtIndex:i];
    }
}

@end

@implementation NSSet (NSPredicateSupport)

- (NSSet *)filteredSetUsingPredicate:(NSPredicate *)predicate {
    NSMutableSet *result = [NSMutableSet set];

    for (id object in self) {
        if ([predicate evaluateWithObject:object])
            [result addObject:object];
    }
    return result;
}

@end

@implementation NSMutableSet (NSPredicateSupport)

- (void)filterUsingPredicate:(NSPredicate *)predicate {
    for (id object in [self allObjects]) {
        if (![predicate evaluateWithObject:object])
            [self removeObject:object];
    }
}

@end
