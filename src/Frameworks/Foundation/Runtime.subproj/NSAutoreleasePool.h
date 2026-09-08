/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSAutoreleasePool_h
#define NSAutoreleasePool_h

#import <Foundation/NSObject.h>

@interface NSAutoreleasePool : NSObject {
    void *_token;
}

+ (void)addObject:(id)object;

- (void)addObject:(id)object;
- (void)drain;

@end

#endif /* NSAutoreleasePool_h */
