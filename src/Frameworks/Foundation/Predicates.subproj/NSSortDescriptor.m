/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSSortDescriptor.h>
#import <Foundation/NSString.h>
#import <Foundation/NSKeyValueCoding.h>
#include <objc/message.h>

@implementation NSSortDescriptor

+ (instancetype)sortDescriptorWithKey:(NSString *)key ascending:(BOOL)ascending {
    return [[[self alloc] initWithKey:key ascending:ascending] autorelease];
}

+ (instancetype)sortDescriptorWithKey:(NSString *)key
                            ascending:(BOOL)ascending
                             selector:(SEL)selector {
    return [[[self alloc] initWithKey:key
                            ascending:ascending
                             selector:selector] autorelease];
}

- (instancetype)initWithKey:(NSString *)key ascending:(BOOL)ascending {
    return [self initWithKey:key ascending:ascending selector:@selector(compare:)];
}

- (instancetype)initWithKey:(NSString *)key
                  ascending:(BOOL)ascending
                   selector:(SEL)selector {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _key = [key copy];
    _ascending = ascending;
    _selector = selector;
    return self;
}

- (void)dealloc {
    [_key release];
    [super dealloc];
}

- (NSString *)key {
    return _key;
}

- (BOOL)ascending {
    return _ascending;
}

- (SEL)selector {
    return _selector;
}

- (NSSortDescriptor *)reversedSortDescriptor {
    return [[self class] sortDescriptorWithKey:_key
                                     ascending:!_ascending
                                      selector:_selector];
}

- (NSComparisonResult)compareObject:(id)first toObject:(id)second {
    /* A nil key means compare the objects themselves, which is what
     * +sortDescriptorWithKey: callers get when they pass no key path. */
    id left = (_key != nil) ? [first valueForKey:_key] : first;
    id right = (_key != nil) ? [second valueForKey:_key] : second;

    NSComparisonResult result =
        ((NSComparisonResult (*)(id, SEL, id))objc_msgSend)(left, _selector, right);

    return _ascending ? result : -result;
}

- (id)copyWithZone:(NSZone *)zone {
    return [self retain];
}

@end
