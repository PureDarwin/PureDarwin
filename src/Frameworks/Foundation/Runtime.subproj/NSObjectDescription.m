/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <Foundation/NSObjCRuntime.h>

@implementation NSObject (PureDarwinDescription)

+ (NSString *)description {
    return NSStringFromClass(self);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %p>",
                     NSStringFromClass([self class]), self];
}

@end
