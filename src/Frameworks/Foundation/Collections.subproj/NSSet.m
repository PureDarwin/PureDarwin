/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSSet.h>
#include <CoreFoundation/CFBase.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSEnumerator.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <stdlib.h>

extern int __CFConstantStringClassReference[];

static BOOL ns_set_value_is_constant_string(const void *value) {
    return *(const uintptr_t *)value ==
        (uintptr_t)&__CFConstantStringClassReference;
}

static const void *ns_set_retain(CFAllocatorRef allocator, const void *value) {
    if (ns_set_value_is_constant_string(value)) {
        return CFRetain(value);
    }
    return [(id)value retain];
}

static void ns_set_release(CFAllocatorRef allocator, const void *value) {
    if (ns_set_value_is_constant_string(value)) {
        CFRelease(value);
        return;
    }
    [(id)value release];
}

static Boolean ns_set_equal(const void *left, const void *right) {
    return [(id)left isEqual:(id)right] ? true : false;
}

static CFHashCode ns_set_hash(const void *value) {
    return (CFHashCode)[(id)value hash];
}

static const CFSetCallBacks ns_set_callbacks = {
    0,
    ns_set_retain,
    ns_set_release,
    NULL,
    ns_set_equal,
    ns_set_hash
};

@implementation NSSet

+ (instancetype)new {
    // Must be +1, so this cannot go through the autoreleasing +set.
    return (id)CFSetCreate(kCFAllocatorDefault, NULL, 0, &ns_set_callbacks);
}

- (instancetype)init {
    return (id)CFSetCreate(kCFAllocatorDefault, NULL, 0, &ns_set_callbacks);
}

- (instancetype)initWithArray:(NSArray *)array {
    NSUInteger count = [array count];
    CFMutableSetRef result = CFSetCreateMutable(kCFAllocatorDefault,
                                                (CFIndex)count,
                                                &ns_set_callbacks);

    for (NSUInteger index = 0; index < count; index++) {
        CFSetAddValue(result, [array objectAtIndex:index]);
    }
    return (id)result;
}

- (instancetype)initWithSet:(NSSet *)set {
    return [self initWithArray:[set allObjects]];
}

- (instancetype)initWithObjects:(id)firstObject, ... {
    CFMutableSetRef result = CFSetCreateMutable(kCFAllocatorDefault, 0,
                                                &ns_set_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFSetAddValue(result, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    return (id)result;
}

- (instancetype)initWithObjects:(const id *)objects count:(NSUInteger)count {
    return (id)CFSetCreate(kCFAllocatorDefault, (const void **)objects,
                           (CFIndex)count, &ns_set_callbacks);
}

+ (instancetype)set {
    return (id)CFAutorelease(CFSetCreate(kCFAllocatorDefault, NULL, 0,
                                        &ns_set_callbacks));
}

+ (instancetype)setWithObject:(id)object {
    if (!object) {
        return [self set];
    }
    const void *value = object;
    return (id)CFAutorelease(CFSetCreate(kCFAllocatorDefault, &value, 1,
                                        &ns_set_callbacks));
}

+ (instancetype)setWithArray:(NSArray *)array {
    NSUInteger count = [array count];
    const void **values = count ? malloc(count * sizeof(*values)) : NULL;
    for (NSUInteger index = 0; index < count; index++) {
        values[index] = [array objectAtIndex:index];
    }
    CFSetRef set = CFSetCreate(kCFAllocatorDefault, values, (CFIndex)count,
                               &ns_set_callbacks);
    free(values);
    return (id)CFAutorelease(set);
}

+ (instancetype)setWithObjects:(const id [])objects count:(NSUInteger)count {
    return (id)CFAutorelease(CFSetCreate(kCFAllocatorDefault,
                                        (const void **)objects, (CFIndex)count,
                                        &ns_set_callbacks));
}

- (NSUInteger)count {
    return (NSUInteger)CFSetGetCount((CFSetRef)self);
}

- (id)member:(id)object {
    return (id)CFSetGetValue((CFSetRef)self, object);
}

- (BOOL)containsObject:(id)object {
    return CFSetContainsValue((CFSetRef)self, object);
}

- (BOOL)isEqualToSet:(NSSet *)other {
    if (other == nil) {
        return NO;
    }
    if (self == other) {
        return YES;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

- (NSArray *)allObjects {
    CFIndex count = CFSetGetCount((CFSetRef)self);
    const void **values = count ? malloc((size_t)count * sizeof(*values)) : NULL;
    CFSetGetValues((CFSetRef)self, values);
    NSArray *array = [NSArray arrayWithObjects:(const id *)values
                                         count:(NSUInteger)count];
    free(values);
    return array;
}

/* Fast enumeration: CFSet has no index-based access, so the members are
 * snapshotted once and walked from the state's extra storage. */
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained [])buffer
                                    count:(NSUInteger)length {
    NSArray *objects = (state->state == 0) ? [self allObjects]
                                           : (NSArray *)state->extra[1];

    if (state->state == 0) {
        state->extra[1] = (unsigned long)objects;
        state->mutationsPtr = &state->extra[0];
    }

    NSUInteger count = [objects count];
    NSUInteger index = (NSUInteger)state->state;

    if (index >= count || length == 0) {
        return 0;
    }

    NSUInteger produced = MIN(length, count - index);
    for (NSUInteger offset = 0; offset < produced; offset++) {
        buffer[offset] = [objects objectAtIndex:index + offset];
    }

    state->itemsPtr = buffer;
    state->state += produced;
    return produced;
}

- (NSEnumerator *)objectEnumerator {
    return [[self allObjects] objectEnumerator];
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFSetCreateCopy(kCFAllocatorDefault, (CFSetRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFSetCreateMutableCopy(kCFAllocatorDefault, 0,
                                      (CFSetRef)self);
}

/* CoreFoundation has one runtime class for immutable and mutable sets. */
- (void)addObject:(id)object {
    if (object) {
        CFSetAddValue((CFMutableSetRef)self, object);
    }
}

- (void)removeObject:(id)object {
    if (object) {
        CFSetRemoveValue((CFMutableSetRef)self, object);
    }
}

- (void)removeAllObjects {
    CFSetRemoveAllValues((CFMutableSetRef)self);
}

- (void)addObjectsFromArray:(NSArray *)array {
    NSUInteger count = [array count];
    for (NSUInteger index = 0; index < count; index++) {
        [self addObject:[array objectAtIndex:index]];
    }
}

- (void)unionSet:(NSSet *)other {
    [self addObjectsFromArray:[other allObjects]];
}

- (void)minusSet:(NSSet *)other {
    NSArray *objects = [other allObjects];
    NSUInteger count = [objects count];
    for (NSUInteger index = 0; index < count; index++) {
        [self removeObject:[objects objectAtIndex:index]];
    }
}

- (void)intersectSet:(NSSet *)other {
    NSArray *objects = [self allObjects];
    NSUInteger count = [objects count];
    for (NSUInteger index = 0; index < count; index++) {
        id object = [objects objectAtIndex:index];
        if (![other containsObject:object]) {
            [self removeObject:object];
        }
    }
}

/* Bridged to CFSetRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSSet class]]) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

@end

@implementation NSMutableSet

+ (instancetype)new {
    return [self setWithCapacity:0];
}

+ (instancetype)set {
    return [self setWithCapacity:0];
}

+ (instancetype)setWithCapacity:(NSUInteger)capacity {
    /* Hint, not ceiling - see -initWithCapacity:. */
    return (id)CFAutorelease(CFSetCreateMutable(kCFAllocatorDefault, 0,
                                               &ns_set_callbacks));
}

+ (instancetype)setWithObject:(id)object {
    CFMutableSetRef set = CFSetCreateMutable(kCFAllocatorDefault, 0,
                                             &ns_set_callbacks);
    if (object) {
        CFSetAddValue(set, object);
    }
    return (id)CFAutorelease(set);
}

+ (instancetype)setWithArray:(NSArray *)array {
    NSMutableSet *set = [self setWithCapacity:[array count]];
    [set addObjectsFromArray:array];
    return set;
}

+ (instancetype)setWithObjects:(const id [])objects count:(NSUInteger)count {
    CFMutableSetRef set = CFSetCreateMutable(kCFAllocatorDefault,
                                             (CFIndex)count,
                                             &ns_set_callbacks);
    for (NSUInteger index = 0; index < count; index++) {
        if (objects[index]) {
            CFSetAddValue(set, objects[index]);
        }
    }
    return (id)CFAutorelease(set);
}

- (instancetype)init {
    return (id)CFSetCreateMutable(kCFAllocatorDefault, 0,
                                  &ns_set_callbacks);
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    /* A non-zero CF capacity is a hard ceiling, while the Cocoa capacity is
     * only a hint; passing it through makes the collection halt once it grows
     * past the hint. See +[NSMutableData dataWithCapacity:]. */
    return (id)CFSetCreateMutable(kCFAllocatorDefault, 0,
                                  &ns_set_callbacks);
}

// Inherited from NSSet these returned an immutable CFSet, so the first
// -addObject: mutated an immutable object.
- (instancetype)initWithObjects:(id)firstObject, ... {
    CFMutableSetRef set = CFSetCreateMutable(kCFAllocatorDefault, 0,
                                             &ns_set_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFSetAddValue(set, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    return (id)set;
}

- (instancetype)initWithObjects:(const id *)objects count:(NSUInteger)count {
    CFMutableSetRef set = CFSetCreateMutable(kCFAllocatorDefault,
                                             (CFIndex)count,
                                             &ns_set_callbacks);
    for (NSUInteger index = 0; index < count; index++) {
        if (objects[index]) {
            CFSetAddValue(set, objects[index]);
        }
    }
    return (id)set;
}

- (instancetype)initWithArray:(NSArray *)array {
    CFMutableSetRef set = CFSetCreateMutable(kCFAllocatorDefault, 0,
                                             &ns_set_callbacks);
    for (id object in array) {
        CFSetAddValue(set, object);
    }
    return (id)set;
}

- (instancetype)initWithSet:(NSSet *)other {
    return [self initWithArray:[other allObjects]];
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFSetBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFSetGetTypeID(), "NSSet");
}
#endif
