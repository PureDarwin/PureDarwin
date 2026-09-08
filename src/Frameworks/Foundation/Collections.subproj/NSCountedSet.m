/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSCountedSet.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSEnumerator.h>

/* Backed by a dictionary of object -> count rather than by NSSet's CF storage,
 * so it does not inherit the bridged set's behaviour. */

@implementation NSCountedSet

- (instancetype)init {
    return [self initWithCapacity:0];
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _counts = [[NSMutableDictionary alloc] init];
    return self;
}

- (instancetype)initWithArray:(NSArray *)array {
    self = [self initWithCapacity:[array count]];
    if (self == nil) {
        return nil;
    }

    NSUInteger count = [array count];
    for (NSUInteger i = 0; i < count; i++) {
        [self addObject:[array objectAtIndex:i]];
    }
    return self;
}

- (void)dealloc {
    [_counts release];
    [super dealloc];
}

- (NSUInteger)count {
    return [[_counts allKeys] count];
}

- (NSUInteger)countForObject:(id)object {
    NSNumber *count = [_counts objectForKey:object];

    return (count != nil) ? [count unsignedIntegerValue] : 0;
}

- (void)addObject:(id)object {
    if (object == nil) {
        return;
    }
    NSUInteger existing = [self countForObject:object];

    [_counts setObject:[NSNumber numberWithUnsignedInteger:existing + 1]
                forKey:object];
}

- (void)removeObject:(id)object {
    NSUInteger existing = [self countForObject:object];

    if (existing <= 1) {
        [_counts removeObjectForKey:object];
    } else {
        [_counts setObject:[NSNumber numberWithUnsignedInteger:existing - 1]
                    forKey:object];
    }
}

- (BOOL)containsObject:(id)object {
    return [self countForObject:object] > 0;
}

- (NSArray *)allObjects {
    return [_counts allKeys];
}

- (NSEnumerator *)objectEnumerator {
    return [[self allObjects] objectEnumerator];
}

@end
