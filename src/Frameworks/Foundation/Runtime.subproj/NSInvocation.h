/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSInvocation_h
#define NSInvocation_h

#import <Foundation/NSObject.h>
#import <Foundation/NSObjCRuntime.h>

@class NSMethodSignature, NSMutableArray;

/* Object arguments only. That covers every call site here - the panels use it
 * to defer a selector with id arguments - but an invocation carrying scalars
 * or structs needs a real frame builder (libffi). -setArgument: raises for
 * anything else. */
@interface NSInvocation : NSObject {
    NSMethodSignature *_signature;
    id _target;
    SEL _selector;
    NSMutableArray *_arguments;
    id _returnValue;
    BOOL _argumentsRetained;
}

+ (NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature;

- (NSMethodSignature *)methodSignature;

- (id)target;
- (void)setTarget:(id)target;
- (SEL)selector;
- (void)setSelector:(SEL)selector;

- (void)getArgument:(void *)buffer atIndex:(NSInteger)index;
- (void)setArgument:(void *)buffer atIndex:(NSInteger)index;
- (void)getReturnValue:(void *)buffer;
- (void)setReturnValue:(void *)buffer;

- (void)retainArguments;
- (BOOL)argumentsRetained;

- (void)invoke;
- (void)invokeWithTarget:(id)target;

@end

@interface NSMethodSignature : NSObject {
    const char *_types;
    NSUInteger _argumentCount;
}

+ (NSMethodSignature *)signatureWithObjCTypes:(const char *)types;

- (NSUInteger)numberOfArguments;
- (const char *)getArgumentTypeAtIndex:(NSUInteger)index;
- (const char *)methodReturnType;
- (NSUInteger)methodReturnLength;

@end

@interface NSObject (NSMethodSignatureLookup)
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector;
+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)selector;
@end

#endif /* NSInvocation_h */
