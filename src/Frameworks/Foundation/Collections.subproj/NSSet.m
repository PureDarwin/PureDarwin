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
#include <objc/runtime.h>
#import <Foundation/NSException.h>

extern int __CFConstantStringClassReference[];

static BOOL ns_set_value_is_constant_string(const void *value) {
    /* A nil member reaches the callbacks as NULL, and reading its isa
     * to classify it is what turned a stray nil into a crash. */
    if (value == NULL) {
        return NO;
    }
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

extern Boolean _CFIsObjC(CFTypeID typeID, void *obj);

// a subclass that is not a CFSet (Swift's bridged sets, NSCountedSet): the abstract methods work through its primitives
// the cluster's own classes are not: an +alloc'd NSMutableSet is a placeholder whose -init returns a CFSet
static inline BOOL __NSSetIsForeign(id set) {
    Class cls = object_getClass(set);

    if (cls == [NSSet class] || cls == [NSMutableSet class])
        return NO;
    return _CFIsObjC(CFSetGetTypeID(), (void *)set) ? YES : NO;
}

static void __NSSetAbstract(id self, SEL _cmd) {
    [NSException raise:NSInvalidArgumentException
                format:@"*** -[%s %s]: method only defined for abstract class", object_getClassName(self), sel_getName(_cmd)];
}

@implementation NSSet

+ (instancetype)new {
    // a subclass outside the cluster gets a real instance of itself
    if (self != [NSSet class])
        return [[self alloc] init];
    // Must be +1, so this cannot go through the autoreleasing +set.
    return (id)CFSetCreate(kCFAllocatorDefault, NULL, 0, &ns_set_callbacks);
}

- (instancetype)init {
    if (__NSSetIsForeign(self))
        return [super init];
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
    if (__NSSetIsForeign(self)) {
        __NSSetAbstract(self, _cmd);
        return 0;
    }
    return (NSUInteger)CFSetGetCount((CFSetRef)self);
}

- (id)member:(id)object {
    if (__NSSetIsForeign(self)) {
        __NSSetAbstract(self, _cmd);
        return nil;
    }
    return (id)CFSetGetValue((CFSetRef)self, object);
}

- (BOOL)containsObject:(id)object {
    if (__NSSetIsForeign(self))
        return [self member:object] != nil;
    return CFSetContainsValue((CFSetRef)self, object);
}

- (CFTypeID)_cfTypeID {
    return CFSetGetTypeID();
}

// what CFSet's functions send a set that is not a CFSet, built on the primitives
- (void)getObjects:(id __unsafe_unretained *)objects {
    NSUInteger i = 0;

    if (!__NSSetIsForeign(self)) {
        CFSetGetValues((CFSetRef)self, (const void **)objects);
        return;
    }
    for (id object in [[self objectEnumerator] allObjects]) {
        objects[i++] = object;
    }
}

- (NSUInteger)countForObject:(id)object {
    return [self member:object] != nil ? 1 : 0;
}

- (BOOL)__getValue:(id *)value forObj:(id)object {
    id found = [self member:object];

    if (found != nil && value != NULL)
        *value = found;
    return found != nil;
}

- (void)__applyValues:(void (*)(const void *, void *))applier context:(void *)context {
    for (id object in [[self objectEnumerator] allObjects]) {
        applier(object, context);
    }
}

- (void)replaceObject:(id)object {
    if ([self member:object] != nil) {
        [self removeObject:object];
        [self addObject:object];
    }
}

- (void)setObject:(id)object {
    [self removeObject:object];
    [self addObject:object];
}

- (BOOL)isEqualToSet:(NSSet *)other {
    if (other == nil) {
        return NO;
    }
    if (self == other) {
        return YES;
    }
    if (__NSSetIsForeign(self) || __NSSetIsForeign(other)) {
        if ([other count] != [self count])
            return NO;
        for (id object in [[self objectEnumerator] allObjects]) {
            if ([other member:object] == nil)
                return NO;
        }
        return YES;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

- (void)makeObjectsPerformSelector:(SEL)selector {
    [[self allObjects] makeObjectsPerformSelector:selector];
}

- (void)makeObjectsPerformSelector:(SEL)selector withObject:(id)object {
    [[self allObjects] makeObjectsPerformSelector:selector withObject:object];
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
    if (__NSSetIsForeign(self)) {
        __NSSetAbstract(self, _cmd);
        return nil;
    }
    return [[self allObjects] objectEnumerator];
}

- (id)copyWithZone:(NSZone *)zone {
    // CFSetCreateCopy sends -copyWithZone: back to a non-CF set, so copy its members instead
    if (__NSSetIsForeign(self)) {
        CFMutableSetRef members = CFSetCreateMutableCopy(kCFAllocatorDefault, 0, (CFSetRef)self);
        CFSetRef copy = CFSetCreateCopy(kCFAllocatorDefault, members);

        CFRelease(members);
        return (id)copy;
    }
    return (id)CFSetCreateCopy(kCFAllocatorDefault, (CFSetRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFSetCreateMutableCopy(kCFAllocatorDefault, 0,
                                      (CFSetRef)self);
}

/* CoreFoundation has one runtime class for immutable and mutable sets. */
- (void)addObject:(id)object {
    if (__NSSetIsForeign(self)) {
        __NSSetAbstract(self, _cmd);
        return;
    }
    if (object) {
        CFSetAddValue((CFMutableSetRef)self, object);
    }
}

- (void)removeObject:(id)object {
    if (__NSSetIsForeign(self)) {
        __NSSetAbstract(self, _cmd);
        return;
    }
    if (object) {
        CFSetRemoveValue((CFMutableSetRef)self, object);
    }
}

- (void)removeAllObjects {
    if (__NSSetIsForeign(self)) {
        for (id object in [[self objectEnumerator] allObjects]) {
            [self removeObject:object];
        }
        return;
    }
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
    // CF hashes a set by its count too
    if (__NSSetIsForeign(self))
        return [self count];
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSSet class]]) {
        return NO;
    }
    return [self isEqualToSet:other];
}


- (NSSet *)setByAddingObjectsFromArray:(NSArray *)other {
    NSMutableSet *result = [[[NSMutableSet alloc] initWithSet:self] autorelease];

    [result addObjectsFromArray:other];
    return result;
}

@end

@implementation NSMutableSet

+ (instancetype)new {
    if (self != [NSMutableSet class])
        return [[self alloc] init];
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
    if (__NSSetIsForeign(self))
        return [super init];
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
