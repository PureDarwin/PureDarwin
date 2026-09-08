/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSAutoreleasePool.h>
#include <objc/objc.h>

/* libobjc owns the pool stack; NSAutoreleasePool is a wrapper over its
 * push/pop pair so -init and -release/-drain nest the same way. */
extern void *objc_autoreleasePoolPush(void);
extern void objc_autoreleasePoolPop(void *token);

@implementation NSAutoreleasePool

+ (void)addObject:(id)object {
    [object autorelease];
}

- (instancetype)init {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _token = objc_autoreleasePoolPush();
    return self;
}

- (void)addObject:(id)object {
    [object autorelease];
}

- (void)drain {
    [self release];
}

- (void)dealloc {
    if (_token != NULL) {
        objc_autoreleasePoolPop(_token);
        _token = NULL;
    }
    [super dealloc];
}

/* A pool is not a general object: retaining or autoreleasing one would break
 * the stack discipline libobjc relies on. */
- (id)retain {
    return self;
}

- (id)autorelease {
    return self;
}

@end
