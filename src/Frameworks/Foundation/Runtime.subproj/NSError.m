/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>

@implementation NSError {
    NSString *_domain;
    NSInteger _code;
}

+ (instancetype)errorWithDomain:(NSString *)domain code:(NSInteger)code {
    NSError *err = [[self alloc] init];
    if (err != nil) {
        err->_domain = domain;
        err->_code = code;
    }
    return err;
}

- (NSInteger)code {
    return _code;
}

- (NSString *)domain {
    return _domain;
}

- (NSString *)localizedDescription {
    return (NSString *)CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                CFSTR("%@ error %ld"),
                                                (CFStringRef)_domain, (long)_code);
}

@end
