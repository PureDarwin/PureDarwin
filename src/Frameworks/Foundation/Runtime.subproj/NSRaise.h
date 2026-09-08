/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRaise_h
#define NSRaise_h

#import <Foundation/NSException.h>
#import <Foundation/NSObject.h>
#import <Foundation/NSString.h>
#import <objc/runtime.h>

/* The three "you should not have called this" raises. They are macros so the
 * message names the calling class and selector. */

#define NSUnimplementedMethod() \
    [NSException raise:NSInternalInconsistencyException \
                format:@"-[%@ %s] is not implemented", \
                       NSStringFromClass([self class]), sel_getName(_cmd)]

#define NSUnsupportedMethod() \
    [NSException raise:NSInternalInconsistencyException \
                format:@"-[%@ %s] is not supported", \
                       NSStringFromClass([self class]), sel_getName(_cmd)]

/* Sent from an abstract class's method body: the concrete subclass was
 * supposed to override it. */
#define NSInvalidAbstractInvocation() \
    [NSException raise:NSInvalidArgumentException \
                format:@"-[%@ %s] is abstract and must be overridden", \
                       NSStringFromClass([self class]), sel_getName(_cmd)]

#define NSUnimplementedFunction() \
    [NSException raise:NSInternalInconsistencyException \
                format:@"%s is not implemented", __PRETTY_FUNCTION__]

#endif /* NSRaise_h */
