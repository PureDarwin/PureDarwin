/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdarg.h>

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

static CFComparisonResult ns_array_compare(const void *left,
                                           const void *right,
                                           void *context) {
    SEL selector = (SEL)context;
    return (CFComparisonResult)((NSInteger (*)(id, SEL, id))objc_msgSend)(
        (id)left, selector, (id)right);
}

/* clang emits a reference to this shared empty array for the @[] literal
 * rather than calling into the runtime. */
id __NSArray0__ = nil;

__attribute__((constructor))
static void __NSArray0Init(void) {
    __NSArray0__ = (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0,
                                     &kCFTypeArrayCallBacks);
}

@implementation NSArray

/* See the note in NSDictionary.m: bridged onto CF, so +new/-init must produce a
 * real CF object rather than NSObject's plain allocation. */
+ (instancetype)new {
    return [self array];
}

- (instancetype)init {
    return [NSArray array];
}

- (instancetype)initWithObjects:(id)firstObject, ... {
    CFMutableArrayRef values = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                     &ns_array_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFArrayAppendValue(values, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)result;
}

- (instancetype)initWithArray:(NSArray *)array {
    if (array == nil) {
        return (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0, &ns_array_callbacks);
    }
    return (id)CFArrayCreateCopy(kCFAllocatorDefault, (CFArrayRef)array);
}

- (instancetype)initWithObjects:(const id *)objects count:(NSUInteger)count {
    return (id)CFArrayCreate(kCFAllocatorDefault, (const void **)objects,
                             (CFIndex)count, &ns_array_callbacks);
}

+ (instancetype)array {
    return (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0,
                             &ns_array_callbacks);
}

+ (instancetype)arrayWithObject:(id)object {
    return [self arrayWithObjects:&object count:1];
}

+ (instancetype)arrayWithObjects:(id)firstObject, ... {
    CFMutableArrayRef values = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                     &ns_array_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFArrayAppendValue(values, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)result;
}

+ (instancetype)arrayWithObjects:(const id *)objects count:(NSUInteger)count {
    return (id)CFArrayCreate(kCFAllocatorDefault, (const void **)objects,
                             (CFIndex)count, &ns_array_callbacks);
}

+ (instancetype)arrayWithArray:(NSArray *)array {
    // A nil argument is an empty array, not a crash in CFArrayGetCount.
    if (array == nil) {
        return (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0,
                                 &ns_array_callbacks);
    }
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

/* These are CF objects, not ObjC allocations: the default NSObject refcounting
 * would free CF-allocated memory, and constant strings, which CF keeps
 * immortal, are not heap objects at all. Forward to CF. */
- (id)retain {
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    return (NSUInteger)CFGetRetainCount((CFTypeRef)self);
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFArrayCreateCopy(kCFAllocatorDefault, (CFArrayRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, (CFArrayRef)self);
}

- (id)firstObject {
    CFIndex n = CFArrayGetCount((CFArrayRef)self);
    return n > 0 ? (id)CFArrayGetValueAtIndex((CFArrayRef)self, 0) : nil;
}

- (id)lastObject {
    CFIndex n = CFArrayGetCount((CFArrayRef)self);
    return n > 0 ? (id)CFArrayGetValueAtIndex((CFArrayRef)self, n - 1) : nil;
}

- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained [])buffer
                                    count:(NSUInteger)length {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);
    CFIndex index = (CFIndex)state->state;

    if (index >= count || length == 0) {
        return 0;
    }

    NSUInteger result = MIN(length, (NSUInteger)(count - index));
    for (NSUInteger offset = 0; offset < result; ++offset) {
        buffer[offset] = (id)CFArrayGetValueAtIndex((CFArrayRef)self,
                                                    index + (CFIndex)offset);
    }

    state->itemsPtr = buffer;
    state->mutationsPtr = &state->extra[0];
    state->state += result;
    return result;
}

- (BOOL)containsObject:(id)object {
    if (object == nil) {
        return NO;
    }
    CFIndex n = CFArrayGetCount((CFArrayRef)self);
    return CFArrayContainsValue((CFArrayRef)self, CFRangeMake(0, n),
                                (const void *)object) ? YES : NO;
}

/* By equality; -indexOfObjectIdenticalTo: is the pointer-identity variant. */
- (NSUInteger)indexOfObject:(id)object {
    if (object == nil) {
        return NSNotFound;
    }
    CFIndex count = CFArrayGetCount((CFArrayRef)self);
    CFIndex index = CFArrayGetFirstIndexOfValue((CFArrayRef)self,
                                                CFRangeMake(0, count), object);

    return (index == kCFNotFound) ? NSNotFound : (NSUInteger)index;
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

/* Property-list serialisation, matching NSDictionary's. */
- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically {
    extern BOOL pd_write_plist(CFPropertyListRef plist, NSString *path,
                               BOOL atomically);

    return pd_write_plist((CFPropertyListRef)self, path, atomically);
}

- (NSArray *)arrayByAddingObjectsFromArray:(NSArray *)other {
    CFMutableArrayRef values = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFArrayRef)self);
    if (other != nil) {
        CFArrayAppendArray(values, (CFArrayRef)other,
                           CFRangeMake(0, CFArrayGetCount((CFArrayRef)other)));
    }

    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)CFAutorelease(result);
}

- (NSArray *)subarrayWithRange:(NSRange)range {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    if (range.location > (NSUInteger)count ||
        range.location + range.length > (NSUInteger)count) {
        [NSException raise:NSRangeException
                    format:@"-[NSArray subarrayWithRange:]: range {%lu, %lu} "
                           @"beyond count %ld",
                           (unsigned long)range.location,
                           (unsigned long)range.length, (long)count];
        return nil;
    }

    CFMutableArrayRef values = CFArrayCreateMutable(kCFAllocatorDefault,
                                                     (CFIndex)range.length,
                                                     &ns_array_callbacks);
    for (NSUInteger i = 0; i < range.length; i++) {
        CFArrayAppendValue(values,
            CFArrayGetValueAtIndex((CFArrayRef)self,
                                   (CFIndex)(range.location + i)));
    }

    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)CFAutorelease(result);
}

- (BOOL)isEqualToArray:(NSArray *)other {
    if (other == nil) {
        return NO;
    }
    if (self == other) {
        return YES;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

/* Key-value coding over a collection: the result is the values of that key
 * from each element, which is what callers use to pluck a field out. */
- (id)valueForKey:(NSString *)key {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);
    CFMutableArrayRef values = CFArrayCreateMutable(kCFAllocatorDefault, count,
                                                     &ns_array_callbacks);

    for (CFIndex i = 0; i < count; i++) {
        id object = (id)CFArrayGetValueAtIndex((CFArrayRef)self, i);
        id value = [object valueForKey:key];

        CFArrayAppendValue(values, (value != nil) ? value : (id)[NSNull null]);
    }

    CFArrayRef result = CFArrayCreateCopy(kCFAllocatorDefault, values);
    CFRelease(values);
    return (id)CFAutorelease(result);
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

- (NSArray *)sortedArrayUsingSelector:(SEL)selector {
    CFMutableArrayRef result = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFArrayRef)self);
    CFArraySortValues(result, CFRangeMake(0, CFArrayGetCount(result)),
                      ns_array_compare, selector);
    return (id)CFAutorelease(result);
}

- (void)makeObjectsPerformSelector:(SEL)selector {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    for (CFIndex index = 0; index < count; ++index) {
        id value = (id)CFArrayGetValueAtIndex((CFArrayRef)self, index);
        ((void (*)(id, SEL))objc_msgSend)(value, selector);
    }
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
    /* The array callbacks dereference the value; nil must not reach them. */
    if (object == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[%@ addObject:]: object is nil",
                           NSStringFromClass([self class])];
        return;
    }
    CFArrayAppendValue((CFMutableArrayRef)self, (const void *)object);
}

- (void)insertObject:(id)object atIndex:(NSUInteger)index {
    CFArrayInsertValueAtIndex((CFMutableArrayRef)self, (CFIndex)index,
                              (const void *)object);
}

- (void)addObjectsFromArray:(NSArray *)array {
    // Appending nothing is a no-op; CF would dereference the nil.
    if (array == nil) {
        return;
    }
    CFIndex n = CFArrayGetCount((CFArrayRef)array);
    CFArrayAppendArray((CFMutableArrayRef)self, (CFArrayRef)array,
                       CFRangeMake(0, n));
}

- (void)removeObjectAtIndex:(NSUInteger)index {
    CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, (CFIndex)index);
}

- (void)removeLastObject {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);
    if (count > 0) {
        CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, count - 1);
    }
}

- (void)removeObjectIdenticalTo:(id)object {
    CFIndex index;

    while ((index = (CFIndex)[self indexOfObjectIdenticalTo:object]) !=
           (CFIndex)NSNotFound) {
        CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, index);
    }
}

/* Removes by equality, unlike -removeObjectIdenticalTo: which is by pointer. */
/* Insertion sort: these arrays are small (icon lists, directory entries) and
 * it keeps the comparator contract simple. */
- (void)sortUsingFunction:(NSInteger (*)(id, id, void *))comparator context:(void *)context {
    NSUInteger count = [self count];

    for (NSUInteger i = 1; i < count; i++) {
        id value = [[self objectAtIndex:i] retain];
        NSUInteger j = i;

        while (j > 0 && comparator([self objectAtIndex:j - 1], value, context) > 0) {
            [self replaceObjectAtIndex:j withObject:[self objectAtIndex:j - 1]];
            j--;
        }
        [self replaceObjectAtIndex:j withObject:value];
        [value release];
    }
}

- (void)removeObject:(id)object {
    NSUInteger index;

    while ((index = [self indexOfObject:object]) != NSNotFound) {
        CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, (CFIndex)index);
    }
}

- (void)replaceObjectAtIndex:(NSUInteger)index withObject:(id)object {
    CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index, object);
}

- (void)removeAllObjects {
    CFArrayRemoveAllValues((CFMutableArrayRef)self);
}

- (void)sortUsingSelector:(SEL)selector {
    CFMutableArrayRef array = (CFMutableArrayRef)self;
    CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)),
                      ns_array_compare, selector);
}

/* Bridged to CFArrayRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSArray class]]) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}


- (NSString *)componentsJoinedByString:(NSString *)separator {
    NSMutableString *result = [NSMutableString string];
    NSUInteger count = [self count];

    for (NSUInteger i = 0; i < count; i++) {
        if (i > 0 && separator != nil) {
            [result appendString:separator];
        }
        [result appendString:[[self objectAtIndex:i] description]];
    }
    return result;
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

- (instancetype)init {
    // Must be +1: callers reach here through -alloc/-init and +new.
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, 0, &ns_array_callbacks);
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)capacity,
                                    &ns_array_callbacks);
}

+ (instancetype)arrayWithArray:(NSArray *)array {
    // NSArray's version copies immutably; a mutable array must stay mutable.
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                    &ns_array_callbacks);
    if (array != nil) {
        CFIndex count = CFArrayGetCount((CFArrayRef)array);
        CFArrayAppendArray(result, (CFArrayRef)array,
                           CFRangeMake(0, count));
    }
    return (id)result;
}

- (instancetype)initWithArray:(NSArray *)array {
    if (array == nil) {
        return (id)CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                        &ns_array_callbacks);
    }
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
