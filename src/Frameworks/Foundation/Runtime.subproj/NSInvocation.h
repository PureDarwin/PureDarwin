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

/* Argument storage is a raw frame laid out by the method signature, so an
 * invocation carries scalars and structs as well as objects. -invoke builds
 * the call through libffi. */
@interface NSInvocation : NSObject {
    NSMethodSignature *_signature;
    void *_frame;
    void *_returnValue;
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
    char *_types;
    char **_argumentTypes;
    char *_returnType;
    NSUInteger *_argumentOffsets;
    NSUInteger *_argumentSizes;
    NSUInteger _argumentCount;
    NSUInteger _frameLength;
    NSUInteger _returnLength;
    BOOL _isOneway;
}

+ (NSMethodSignature *)signatureWithObjCTypes:(const char *)types;

- (NSUInteger)numberOfArguments;
- (const char *)getArgumentTypeAtIndex:(NSUInteger)index;
- (const char *)methodReturnType;
- (NSUInteger)methodReturnLength;
- (NSUInteger)frameLength;
- (BOOL)isOneway;

/* Byte offset of an argument inside an NSInvocation's frame. */
- (NSUInteger)_offsetOfArgumentAtIndex:(NSUInteger)index;
- (NSUInteger)_sizeOfArgumentAtIndex:(NSUInteger)index;
- (const char *)_types;

@end

@interface NSObject (NSMethodSignatureLookup)
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector;
+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)selector;
@end

#endif /* NSInvocation_h */
