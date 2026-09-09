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
    /* Deliberately not [super init]: NSSet is bridged to CoreFoundation and
     * its -init returns a CFSet, which would discard this allocation and leave
     * every inherited method operating on an immutable CF object. */
    _counts = [[NSMutableDictionary alloc] initWithCapacity:capacity];
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

/* Every NSSet method not overridden here would treat self as a CFSet, so the
 * rest of the inherited surface is implemented against _counts too. */
- (id)member:(id)object {
    return ([_counts objectForKey:object] != nil) ? object : nil;
}

- (NSUInteger)hash {
    return [_counts count];
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (![other isKindOfClass:[NSCountedSet class]]) {
        return NO;
    }
    return [[self allObjects] isEqualToArray:[other allObjects]];
}

- (id)copyWithZone:(NSZone *)zone {
    return [[NSCountedSet alloc] initWithArray:[self allObjects]];
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return [[NSCountedSet alloc] initWithArray:[self allObjects]];
}

- (void)removeAllObjects {
    [_counts removeAllObjects];
}

- (void)addObjectsFromArray:(NSArray *)array {
    NSUInteger count = [array count];

    for (NSUInteger index = 0; index < count; index++) {
        [self addObject:[array objectAtIndex:index]];
    }
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained [])buffer
                                    count:(NSUInteger)length {
    return [[self allObjects] countByEnumeratingWithState:state
                                                  objects:buffer
                                                    count:length];
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
