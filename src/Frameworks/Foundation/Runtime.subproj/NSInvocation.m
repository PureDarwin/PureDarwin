/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSInvocation.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSException.h>
#import <Foundation/NSString.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <string.h>

@implementation NSMethodSignature

+ (NSMethodSignature *)signatureWithObjCTypes:(const char *)types {
    NSMethodSignature *signature = [[[self alloc] init] autorelease];

    signature->_types = types;

    /* Count the argument codes; the first entry is the return type. */
    NSUInteger count = 0;
    for (const char *p = types; p != NULL && *p != '\0'; p++) {
        if (*p >= '0' && *p <= '9') {
            continue;
        }
        count++;
    }
    signature->_argumentCount = (count > 0) ? count - 1 : 0;
    return signature;
}

- (NSUInteger)numberOfArguments {
    return _argumentCount;
}

- (const char *)getArgumentTypeAtIndex:(NSUInteger)index {
    return "@";
}

- (const char *)methodReturnType {
    return (_types != NULL) ? _types : "v";
}

- (NSUInteger)methodReturnLength {
    return sizeof(id);
}

@end

@implementation NSInvocation

+ (NSInvocation *)invocationWithMethodSignature:(NSMethodSignature *)signature {
    NSInvocation *invocation = [[[self alloc] init] autorelease];

    invocation->_signature = [signature retain];
    invocation->_arguments = [[NSMutableArray alloc] init];
    return invocation;
}

- (void)dealloc {
    [_signature release];
    [_arguments release];
    [_returnValue release];
    [super dealloc];
}

- (NSMethodSignature *)methodSignature {
    return _signature;
}

- (id)target {
    return _target;
}

- (void)setTarget:(id)target {
    if (_argumentsRetained) {
        [target retain];
        [_target release];
    }
    _target = target;
}

- (SEL)selector {
    return _selector;
}

- (void)setSelector:(SEL)selector {
    _selector = selector;
}

/* Index 0 and 1 are self and _cmd, so a caller's first argument is index 2. */
- (void)setArgument:(void *)buffer atIndex:(NSInteger)index {
    if (index < 2) {
        if (index == 0) { [self setTarget:*(id *)buffer]; }
        if (index == 1) { [self setSelector:*(SEL *)buffer]; }
        return;
    }

    NSUInteger slot = (NSUInteger)(index - 2);
    id object = (buffer != NULL) ? *(id *)buffer : nil;

    while ([_arguments count] <= slot) {
        [_arguments addObject:[NSNull null]];
    }
    [_arguments replaceObjectAtIndex:slot
                          withObject:(object != nil) ? object : (id)[NSNull null]];
}

- (void)getArgument:(void *)buffer atIndex:(NSInteger)index {
    if (buffer == NULL) {
        return;
    }
    if (index == 0) { *(id *)buffer = _target; return; }
    if (index == 1) { *(SEL *)buffer = _selector; return; }

    NSUInteger slot = (NSUInteger)(index - 2);
    if (slot >= [_arguments count]) {
        *(id *)buffer = nil;
        return;
    }
    id object = [_arguments objectAtIndex:slot];
    *(id *)buffer = (object == (id)[NSNull null]) ? nil : object;
}

- (void)getReturnValue:(void *)buffer {
    if (buffer != NULL) {
        *(id *)buffer = _returnValue;
    }
}

- (void)setReturnValue:(void *)buffer {
    id value = (buffer != NULL) ? *(id *)buffer : nil;

    [value retain];
    [_returnValue release];
    _returnValue = value;
}

- (void)retainArguments {
    if (_argumentsRetained) {
        return;
    }
    _argumentsRetained = YES;
    [_target retain];
}

- (BOOL)argumentsRetained {
    return _argumentsRetained;
}

- (void)invokeWithTarget:(id)target {
    [self setTarget:target];
    [self invoke];
}

- (void)invoke {
    if (_target == nil || _selector == NULL) {
        return;
    }

    id args[4] = { nil, nil, nil, nil };
    NSUInteger count = [_arguments count];

    if (count > 4) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[NSInvocation invoke] supports at most 4 object arguments, got %lu",
                           (unsigned long)count];
        return;
    }
    for (NSUInteger i = 0; i < count; i++) {
        id object = [_arguments objectAtIndex:i];

        args[i] = (object == (id)[NSNull null]) ? nil : object;
    }

    id result = nil;
    switch (count) {
        case 0:
            result = ((id (*)(id, SEL))objc_msgSend)(_target, _selector);
            break;
        case 1:
            result = ((id (*)(id, SEL, id))objc_msgSend)(_target, _selector, args[0]);
            break;
        case 2:
            result = ((id (*)(id, SEL, id, id))objc_msgSend)(_target, _selector,
                                                             args[0], args[1]);
            break;
        case 3:
            result = ((id (*)(id, SEL, id, id, id))objc_msgSend)(_target, _selector,
                                                                 args[0], args[1], args[2]);
            break;
        default:
            result = ((id (*)(id, SEL, id, id, id, id))objc_msgSend)(_target, _selector,
                                                                     args[0], args[1],
                                                                     args[2], args[3]);
            break;
    }
    [self setReturnValue:&result];
}

@end

@implementation NSObject (NSMethodSignatureLookup)

- (NSMethodSignature *)methodSignatureForSelector:(SEL)selector {
    Method method = class_getInstanceMethod([self class], selector);

    if (method == NULL) {
        return nil;
    }
    return [NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
}

+ (NSMethodSignature *)instanceMethodSignatureForSelector:(SEL)selector {
    Method method = class_getInstanceMethod(self, selector);

    if (method == NULL) {
        return nil;
    }
    return [NSMethodSignature signatureWithObjCTypes:method_getTypeEncoding(method)];
}

@end
