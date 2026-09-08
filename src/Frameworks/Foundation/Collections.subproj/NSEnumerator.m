/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSEnumerator.h>
#import <Foundation/NSArray.h>

/* Walks a snapshot of the array, so the enumerator survives mutation of the
 * original the way callers expect. */
@interface NSArrayEnumerator : NSEnumerator {
    NSArray *_array;
    NSUInteger _index;
}
- (instancetype)initWithArray:(NSArray *)array;
@end

@implementation NSArrayEnumerator

- (instancetype)initWithArray:(NSArray *)array {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _array = [array copy];
    return self;
}

- (void)dealloc {
    [_array release];
    [super dealloc];
}

- (id)nextObject {
    if (_index >= [_array count]) {
        return nil;
    }
    return [_array objectAtIndex:_index++];
}

@end

@implementation NSArray (NSEnumeration)

- (NSEnumerator *)objectEnumerator {
    return [[[NSArrayEnumerator alloc] initWithArray:self] autorelease];
}

@end

@implementation NSEnumerator

/* Abstract: subclasses supply the iteration. */
- (id)nextObject {
    return nil;
}

- (NSArray *)allObjects {
    NSMutableArray *result = [NSMutableArray array];
    id object;

    while ((object = [self nextObject]) != nil) {
        [result addObject:object];
    }
    return result;
}

@end
