/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSSocket_bsd.h>
#include <unistd.h>

@implementation NSSocket

- (int)fileDescriptor {
    return -1;
}

- (void)close {
}

@end

@implementation NSSocket_bsd

+ (instancetype)socketWithDescriptor:(int)descriptor {
    return [[[self alloc] initWithDescriptor:descriptor] autorelease];
}

- (instancetype)initWithDescriptor:(int)descriptor {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _descriptor = descriptor;
    return self;
}

/* The descriptor is owned by whoever opened it; wrapping it does not transfer
 * ownership, so -dealloc must not close it. */
- (void)dealloc {
    if (_closeOnDealloc && _descriptor >= 0) {
        close(_descriptor);
    }
    [super dealloc];
}

- (int)fileDescriptor {
    return _descriptor;
}

- (void)close {
    if (_descriptor >= 0) {
        close(_descriptor);
        _descriptor = -1;
    }
}

@end
