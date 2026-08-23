/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSNull.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>

@implementation NSNull

/* NSNull is a singleton; equality is identity. */
+ (NSNull *)null {
    static NSNull *shared = nil;
    if (shared == nil) {
        shared = [[self alloc] init];
    }
    return shared;
}

- (NSString *)description {
    return (NSString *)CFSTR("<null>");
}

@end
