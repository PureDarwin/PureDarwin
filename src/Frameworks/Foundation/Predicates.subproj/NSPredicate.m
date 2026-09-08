/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSPredicate.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>

@implementation NSPredicate {
    BOOL _value;
}

+ (NSPredicate *)predicateWithFormat:(NSString *)format, ... {
    [NSException raise:NSInternalInconsistencyException
                format:@"+[NSPredicate predicateWithFormat:] is not implemented"];
    return nil;
}

+ (NSPredicate *)predicateWithValue:(BOOL)value {
    NSPredicate *predicate = [[[self alloc] init] autorelease];

    predicate->_value = value;
    return predicate;
}

- (instancetype)init {
    self = [super init];
    if (self != nil) {
        _value = YES;
    }
    return self;
}

- (BOOL)evaluateWithObject:(id)object {
    return _value;
}

- (BOOL)evaluateWithObject:(id)object substitutionVariables:(NSDictionary *)variables {
    return [self evaluateWithObject:object];
}

- (NSString *)predicateFormat {
    return _value ? @"TRUEPREDICATE" : @"FALSEPREDICATE";
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end

@implementation NSArray (NSPredicateFiltering)

- (NSArray *)filteredArrayUsingPredicate:(NSPredicate *)predicate {
    NSMutableArray *result = [NSMutableArray array];
    NSUInteger count = [self count];

    for (NSUInteger i = 0; i < count; i++) {
        id object = [self objectAtIndex:i];

        if ([predicate evaluateWithObject:object]) {
            [result addObject:object];
        }
    }
    return result;
}

@end
