/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSArray.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/message.h>
#include <objc/runtime.h>

extern int __CFConstantStringClassReference[];

static BOOL ns_array_value_is_constant_string(const void *value) {
    return *(const uintptr_t *)value ==
        (uintptr_t)&__CFConstantStringClassReference;
}

static const void *ns_array_retain(CFAllocatorRef allocator,
                                    const void *value) {
    if (ns_array_value_is_constant_string(value)) {
        return CFRetain(value);
    }
    return [(id)value retain];
}

static void ns_array_release(CFAllocatorRef allocator, const void *value) {
    if (ns_array_value_is_constant_string(value)) {
        CFRelease(value);
        return;
    }
    [(id)value release];
}

static const CFArrayCallBacks ns_array_callbacks = {
    0,
    ns_array_retain,
    ns_array_release,
    CFCopyDescription,
    CFEqual
};

@implementation NSArray

+ (instancetype)array {
    return (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0,
                             &ns_array_callbacks);
}

+ (instancetype)arrayWithObjects:(const id *)objects count:(NSUInteger)count {
    return (id)CFArrayCreate(kCFAllocatorDefault, (const void **)objects,
                             (CFIndex)count, &ns_array_callbacks);
}

+ (instancetype)arrayWithArray:(NSArray *)array {
    return (id)CFArrayCreateCopy(kCFAllocatorDefault, (CFArrayRef)array);
}

- (NSUInteger)count {
    return (NSUInteger)CFArrayGetCount((CFArrayRef)self);
}

- (id)objectAtIndex:(NSUInteger)index {
    return (id)CFArrayGetValueAtIndex((CFArrayRef)self, (CFIndex)index);
}

- (id)objectAtIndexedSubscript:(NSUInteger)index {
    return [self objectAtIndex:index];
}

- (BOOL)containsObject:(id)object {
    CFIndex n = CFArrayGetCount((CFArrayRef)self);
    return CFArrayContainsValue((CFArrayRef)self, CFRangeMake(0, n),
                                (const void *)object) ? YES : NO;
}

- (NSUInteger)indexOfObjectIdenticalTo:(id)object {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    for (CFIndex index = 0; index < count; ++index) {
        if (CFArrayGetValueAtIndex((CFArrayRef)self, index) == object) {
            return (NSUInteger)index;
        }
    }
    return NSNotFound;
}

- (NSArray *)arrayByAddingObject:(id)object {
    CFMutableArrayRef values = CFArrayCreateMutableCopy(kCFAllocatorDefault,
                                                        0,
                                                        (CFArrayRef)self);
    CFArrayAppendValue(values, object);
    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)result;
}

- (void)makeObjectsPerformSelector:(SEL)selector withObject:(id)object {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    for (CFIndex index = 0; index < count; ++index) {
        id value = (id)CFArrayGetValueAtIndex((CFArrayRef)self, index);
        ((void (*)(id, SEL, id))objc_msgSend)(value, selector, object);
    }
}

/* CoreFoundation has one runtime class for immutable and mutable arrays. */
- (void)addObject:(id)object {
    CFArrayAppendValue((CFMutableArrayRef)self, (const void *)object);
}

- (void)addObjectsFromArray:(NSArray *)array {
    CFIndex n = CFArrayGetCount((CFArrayRef)array);
    CFArrayAppendArray((CFMutableArrayRef)self, (CFArrayRef)array,
                       CFRangeMake(0, n));
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, (CFIndex)index);
}

- (void)removeObjectIdenticalTo:(id)object {
    CFIndex index;

    while ((index = (CFIndex)[self indexOfObjectIdenticalTo:object]) !=
           (CFIndex)NSNotFound) {
        CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, index);
    }
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)object {
    CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index, object);
}

- (void)removeAllObjects {
    CFArrayRemoveAllValues((CFMutableArrayRef)self);
}

@end

@implementation NSMutableArray

+ (instancetype)new {
    return [self arrayWithCapacity:0];
}

+ (instancetype)arrayWithCapacity:(NSUInteger)capacity {
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)capacity,
                                    &ns_array_callbacks);
}

+ (instancetype)array {
    return [self arrayWithCapacity:0];
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)capacity,
                                    &ns_array_callbacks);
}

- (instancetype)initWithArray:(NSArray *)array {
    return (id)CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                                        (CFArrayRef)array);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFArrayBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFArrayGetTypeID(), "NSArray");
}
#endif
