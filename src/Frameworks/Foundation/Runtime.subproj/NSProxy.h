/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSProxy_h
#define NSProxy_h

#import <Foundation/NSObject.h>

@class NSInvocation, NSMethodSignature, NSString;

/* A root class, like NSObject: it deliberately implements almost nothing so
 * that messages fall through to -forwardInvocation:. */
__attribute__((objc_root_class))
@interface NSProxy <NSObject> {
    Class isa;
}

+ (id)alloc;
+ (id)allocWithZone:(NSZone *)zone;
+ (Class)class;
+ (BOOL)respondsToSelector:(SEL)selector;

- (void)dealloc;
- (void)finalize;

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector;
- (void)forwardInvocation:(NSInvocation *)invocation;
- (void)doesNotRecognizeSelector:(SEL)selector;

- (NSString *)description;
- (NSString *)debugDescription;

@end

#endif /* NSProxy_h */
