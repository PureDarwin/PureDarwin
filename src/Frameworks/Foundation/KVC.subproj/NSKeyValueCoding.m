/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSKeyValueObserving.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSInvocation.h>
#include <stdlib.h>
#import <Foundation/NSException.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#import <Foundation/NSNull.h>

NSString *const NSUndefinedKeyException = @"NSUndefinedKeyException";

/* Getter lookup order matches Apple's: -getKey, -key, -isKey, then the _key
 * and key instance variables. Only object-typed and the common scalar returns
 * are handled; a scalar comes back boxed in NSNumber/NSValue. */

static SEL selectorForGetter(id self, NSString *key) {
    const char *name = [key UTF8String];
    size_t length = strlen(name);
    char buffer[256];

    if (length == 0 || length + 8 >= sizeof(buffer)) {
        return NULL;
    }

    snprintf(buffer, sizeof(buffer), "get%c%s", toupper(name[0]), name + 1);
    if ([self respondsToSelector:sel_getUid(buffer)]) {
        return sel_getUid(buffer);
    }
    if ([self respondsToSelector:sel_getUid(name)]) {
        return sel_getUid(name);
    }
    snprintf(buffer, sizeof(buffer), "is%c%s", toupper(name[0]), name + 1);
    if ([self respondsToSelector:sel_getUid(buffer)]) {
        return sel_getUid(buffer);
    }
    return NULL;
}

static Ivar ivarForKey(id self, NSString *key) {
    const char *name = [key UTF8String];
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "_%s", name);
    Ivar ivar = class_getInstanceVariable([self class], buffer);
    if (ivar != NULL) {
        return ivar;
    }
    return class_getInstanceVariable([self class], name);
}

/* Boxes whatever the getter returns, based on its encoded return type. */
static id boxedReturnValue(id self, SEL selector) {
    Method method = class_getInstanceMethod([self class], selector);
    char type[64];

    if (method == NULL) {
        return nil;
    }
    method_getReturnType(method, type, sizeof(type));

    switch (type[0]) {
        case '@':
        case '#':
            return ((id (*)(id, SEL))objc_msgSend)(self, selector);
        case 'c':
            return [NSNumber numberWithChar:((char (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'i':
            return [NSNumber numberWithInt:((int (*)(id, SEL))objc_msgSend)(self, selector)];
        case 's':
            return [NSNumber numberWithShort:((short (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'l':
        case 'q':
            return [NSNumber numberWithLongLong:((long long (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'C':
            return [NSNumber numberWithUnsignedChar:((unsigned char (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'I':
            return [NSNumber numberWithUnsignedInt:((unsigned (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'S':
            return [NSNumber numberWithUnsignedShort:((unsigned short (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'L':
        case 'Q':
            return [NSNumber numberWithUnsignedLongLong:((unsigned long long (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'f':
            return [NSNumber numberWithFloat:((float (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'd':
            return [NSNumber numberWithDouble:((double (*)(id, SEL))objc_msgSend)(self, selector)];
        case 'B':
            return [NSNumber numberWithBool:((BOOL (*)(id, SEL))objc_msgSend)(self, selector)];
        case '{': {
            // structs such as CGPoint and CGRect come back in an NSValue of their own type
            NSMethodSignature *signature = [self methodSignatureForSelector:selector];
            NSUInteger length = [signature methodReturnLength];
            NSInvocation *invocation;
            NSValue *value;
            void *bytes;

            if (signature == nil || length == 0 || (bytes = calloc(1, length)) == NULL)
                return nil;
            invocation = [NSInvocation invocationWithMethodSignature:signature];
            [invocation setTarget:self];
            [invocation setSelector:selector];
            [invocation invoke];
            [invocation getReturnValue:bytes];
            value = [NSValue valueWithBytes:bytes objCType:[signature methodReturnType]];
            free(bytes);
            return value;
        }
        default:
            return nil;
    }
}

// scalar and struct setters take the unboxed value, the way the getter boxed it
// returns NO for object setters, which take the value as it is
static BOOL setThroughSetter(id self, SEL setter, id value, NSString *key) {
    NSMethodSignature *signature = [self methodSignatureForSelector:setter];
    const char *type;
    NSUInteger size = 0, alignment = 0;
    NSInvocation *invocation;
    void *bytes;

    if (signature == nil || [signature numberOfArguments] != 3)
        return NO;
    type = [signature getArgumentTypeAtIndex:2];
    while (*type == 'r' || *type == 'n' || *type == 'N' || *type == 'o' || *type == 'O' || *type == 'R' || *type == 'V')
        type++;
    if (type[0] == '@' || type[0] == '#')
        return NO;
    if (value == nil) {
        [self setNilValueForKey:key];
        return YES;
    }
    NSGetSizeAndAlignment(type, &size, &alignment);
    if (size == 0 || (bytes = calloc(1, size)) == NULL)
        return NO;
    switch (type[0]) {
        case 'c': *(char *)bytes = [value charValue]; break;
        case 'C': *(unsigned char *)bytes = [value unsignedCharValue]; break;
        case 's': *(short *)bytes = [value shortValue]; break;
        case 'S': *(unsigned short *)bytes = [value unsignedShortValue]; break;
        case 'i': *(int *)bytes = [value intValue]; break;
        case 'I': *(unsigned *)bytes = [value unsignedIntValue]; break;
        case 'l': *(long *)bytes = [value longValue]; break;
        case 'L': *(unsigned long *)bytes = [value unsignedLongValue]; break;
        case 'q': *(long long *)bytes = [value longLongValue]; break;
        case 'Q': *(unsigned long long *)bytes = [value unsignedLongLongValue]; break;
        case 'f': *(float *)bytes = [value floatValue]; break;
        case 'd': *(double *)bytes = [value doubleValue]; break;
        case 'B': *(BOOL *)bytes = [value boolValue]; break;
        case '{': [value getValue:bytes]; break;
        default:
            free(bytes);
            return NO;
    }
    invocation = [NSInvocation invocationWithMethodSignature:signature];
    [invocation setTarget:self];
    [invocation setSelector:setter];
    [invocation setArgument:bytes atIndex:2];
    [invocation invoke];
    free(bytes);
    return YES;
}

@implementation NSObject (NSKeyValueCoding)

+ (BOOL)accessInstanceVariablesDirectly {
    return YES;
}

- (id)valueForKey:(NSString *)key {
    SEL getter = selectorForGetter(self, key);

    if (getter != NULL) {
        return boxedReturnValue(self, getter);
    }

    if ([[self class] accessInstanceVariablesDirectly]) {
        Ivar ivar = ivarForKey(self, key);

        if (ivar != NULL) {
            const char *type = ivar_getTypeEncoding(ivar);

            if (type != NULL && (type[0] == '@' || type[0] == '#')) {
                return object_getIvar(self, ivar);
            }
        }
    }
    return [self valueForUndefinedKey:key];
}

- (void)setValue:(id)value forKey:(NSString *)key {
    const char *name = [key UTF8String];

    /* No isa-swizzling here, so this is where automatic KVO notification
     * happens: anything routed through KVC notifies its observers. */
    BOOL notifies = [[self class] automaticallyNotifiesObserversForKey:key];
    if (notifies) {
        [self willChangeValueForKey:key];
    }
    char buffer[256];

    snprintf(buffer, sizeof(buffer), "set%c%s:", toupper(name[0]), name + 1);
    SEL setter = sel_getUid(buffer);

    if ([self respondsToSelector:setter]) {
        if (!setThroughSetter(self, setter, value, key))
            ((void (*)(id, SEL, id))objc_msgSend)(self, setter, value);
        if (notifies) {
            [self didChangeValueForKey:key];
        }
        return;
    }

    if ([[self class] accessInstanceVariablesDirectly]) {
        Ivar ivar = ivarForKey(self, key);

        if (ivar != NULL) {
            const char *type = ivar_getTypeEncoding(ivar);

            if (type != NULL && (type[0] == '@' || type[0] == '#')) {
                id existing = object_getIvar(self, ivar);

                if (existing != value) {
                    [value retain];
                    [existing release];
                    object_setIvar(self, ivar, value);
                }
                if (notifies) {
                    [self didChangeValueForKey:key];
                }
                return;
            }
        }
    }
    if (notifies) {
        [self didChangeValueForKey:key];
    }
    [self setValue:value forUndefinedKey:key];
}

- (id)valueForKeyPath:(NSString *)keyPath {
    NSArray *components = [keyPath componentsSeparatedByString:@"."];
    NSUInteger count = [components count];
    id current = self;

    for (NSUInteger i = 0; i < count && current != nil; i++) {
        current = [current valueForKey:[components objectAtIndex:i]];
    }
    return current;
}

- (void)setValue:(id)value forKeyPath:(NSString *)keyPath {
    NSArray *components = [keyPath componentsSeparatedByString:@"."];
    NSUInteger count = [components count];
    id current = self;

    for (NSUInteger i = 0; i + 1 < count && current != nil; i++) {
        current = [current valueForKey:[components objectAtIndex:i]];
    }
    [current setValue:value forKey:[components lastObject]];
}

- (id)valueForUndefinedKey:(NSString *)key {
    [NSException raise:NSUndefinedKeyException
                format:@"[%@ valueForKey:] this class is not key value coding-compliant for the key %@",
                       NSStringFromClass([self class]), key];
    return nil;
}

- (void)setValue:(id)value forUndefinedKey:(NSString *)key {
    [NSException raise:NSUndefinedKeyException
                format:@"[%@ setValue:forKey:] this class is not key value coding-compliant for the key %@",
                       NSStringFromClass([self class]), key];
}

- (void)setNilValueForKey:(NSString *)key {
    [NSException raise:NSInvalidArgumentException
                format:@"[%@ setNilValueForKey:] cannot set nil for key %@",
                       NSStringFromClass([self class]), key];
}

- (NSDictionary *)dictionaryWithValuesForKeys:(NSArray *)keys {
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    NSUInteger count = [keys count];

    for (NSUInteger i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];
        id value = [self valueForKey:key];

        [result setObject:(value != nil) ? value : (id)[NSNull null] forKey:key];
    }
    return result;
}

- (void)setValuesForKeysWithDictionary:(NSDictionary *)values {
    NSArray *keys = [values allKeys];
    NSUInteger count = [keys count];

    for (NSUInteger i = 0; i < count; i++) {
        NSString *key = [keys objectAtIndex:i];

        [self setValue:[values objectForKey:key] forKey:key];
    }
}

@end
