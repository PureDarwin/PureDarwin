/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSProxy.h>
#import <Foundation/NSInvocation.h>
#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <objc/objc-internal.h>
#include <stdlib.h>

@implementation NSProxy

+ (id)alloc {
    return [self allocWithZone:NULL];
}

+ (id)allocWithZone:(NSZone *)zone {
    return class_createInstance(self, 0);
}

+ (Class)class {
    return self;
}

+ (BOOL)respondsToSelector:(SEL)selector {
    return class_respondsToSelector(object_getClass(self), selector);
}

- (void)dealloc {
    object_dispose(self);
}

- (void)finalize {
}

/* Subclasses must answer both of these; the base class cannot know what the
 * proxy stands for. */
- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    [NSException raise:NSInvalidArgumentException
                format:@"%s must override -methodSignatureForSelector:",
                       class_getName(object_getClass(self))];
    return nil;
}

- (void)forwardInvocation:(NSInvocation *)invocation {
    [NSException raise:NSInvalidArgumentException
                format:@"%s must override -forwardInvocation:", class_getName(object_getClass(self))];
}

- (void)doesNotRecognizeSelector:(SEL)selector {
    [NSException raise:NSInvalidArgumentException
                format:@"-[%s %s]: unrecognized selector", class_getName(object_getClass(self)),
                       sel_getName(selector)];
}

- (Class)class {
    return object_getClass(self);
}

- (Class)superclass {
    return class_getSuperclass(object_getClass(self));
}

- (id)self {
    return self;
}

- (BOOL)isEqual:(id)other {
    return self == other;
}

- (NSUInteger)hash {
    return (NSUInteger)(uintptr_t)self;
}

- (BOOL)isKindOfClass:(Class)aClass {
    for (Class c = object_getClass(self); c != Nil; c = class_getSuperclass(c)) {
        if (c == aClass) {
            return YES;
        }
    }
    return NO;
}

- (BOOL)isMemberOfClass:(Class)aClass {
    return object_getClass(self) == aClass;
}

- (BOOL)conformsToProtocol:(Protocol *)protocol {
    return class_conformsToProtocol(object_getClass(self), protocol);
}

- (BOOL)respondsToSelector:(SEL)selector {
    return class_respondsToSelector(object_getClass(self), selector);
}

- (BOOL)isProxy {
    return YES;
}

- (id)retain {
    return _objc_rootRetain(self);
}

- (oneway void)release {
    _objc_rootRelease(self);
}

- (id)autorelease {
    return _objc_rootAutorelease(self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)_objc_rootRetainCount(self);
}

- (NSZone *)zone {
    return NULL;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%s: %p>", class_getName(object_getClass(self)), self];
}

- (NSString *)debugDescription {
    return [self description];
}

@end
