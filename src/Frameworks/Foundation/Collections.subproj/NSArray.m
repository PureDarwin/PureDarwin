/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSArray.h>
#import <Foundation/NSIndexSet.h>
#import <Foundation/NSSortDescriptor.h>
#import <Foundation/NSException.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSData.h>
#include <CoreFoundation/CFBase.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdarg.h>

extern int __CFConstantStringClassReference[];

static BOOL ns_array_value_is_constant_string(const void *value) {
    /* A nil member reaches the callbacks as NULL, and reading its isa
     * to classify it is what turned a stray nil into a crash. */
    if (value == NULL) {
        return NO;
    }
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

static Boolean ns_array_equal(const void *left, const void *right) {
    if (left == right) {
        return true;
    }
    if (left == NULL || right == NULL) {
        return false;
    }
    return [(id)left isEqual:(id)right] ? true : false;
}

static const CFArrayCallBacks ns_array_callbacks = {
    0,
    ns_array_retain,
    ns_array_release,
    CFCopyDescription,
    ns_array_equal
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

extern Boolean _CFIsObjC(CFTypeID typeID, void *obj);

// a subclass that is not a CFArray (Swift's bridged arrays, NSConstantArray): the abstract methods work through its primitives
// the cluster's own classes are not: an +alloc'd NSMutableArray is a placeholder whose -init returns a CFArray
static inline BOOL __NSArrayIsForeign(id array) {
    Class cls = object_getClass(array);

    if (cls == [NSArray class] || cls == [NSMutableArray class])
        return NO;
    return _CFIsObjC(CFArrayGetTypeID(), (void *)array) ? YES : NO;
}

static void __NSArrayAbstract(id self, SEL _cmd) {
    [NSException raise:NSInvalidArgumentException
                format:@"*** -[%s %s]: method only defined for abstract class", object_getClassName(self), sel_getName(_cmd)];
}

@implementation NSArray

/* See the note in NSDictionary.m: bridged onto CF, so +new/-init must produce a
 * real CF object rather than NSObject's plain allocation. */
+ (instancetype)new {
    // a subclass outside the cluster gets a real instance of itself
    if (self != [NSArray class])
        return [[self alloc] init];
    return [self array];
}

- (instancetype)init {
    if (__NSArrayIsForeign(self))
        return [super init];
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
    if (__NSArrayIsForeign(self)) {
        __NSArrayAbstract(self, _cmd);
        return 0;
    }
    return (NSUInteger)CFArrayGetCount((CFArrayRef)self);
}

- (id)objectAtIndex:(NSUInteger)index {
    if (__NSArrayIsForeign(self)) {
        __NSArrayAbstract(self, _cmd);
        return nil;
    }
    return (id)CFArrayGetValueAtIndex((CFArrayRef)self, (CFIndex)index);
}

// CFArrayGetValues sends this to arrays that are not CFArrays
- (void)getObjects:(id __unsafe_unretained *)objects range:(NSRange)range {
    if (!__NSArrayIsForeign(self)) {
        CFArrayGetValues((CFArrayRef)self, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                         (const void **)objects);
        return;
    }
    for (NSUInteger i = 0; i < range.length; i++) {
        objects[i] = [self objectAtIndex:range.location + i];
    }
}

- (CFTypeID)_cfTypeID {
    return CFArrayGetTypeID();
}

- (id)objectAtIndexedSubscript:(NSUInteger)index {
    return [self objectAtIndex:index];
}

/* These are CF objects, not ObjC allocations: the default NSObject refcounting
 * would free CF-allocated memory, and constant strings, which CF keeps
 * immortal, are not heap objects at all. Forward to CF. */
- (id)retain {
    if (__NSArrayIsForeign(self))
        return [super retain];
    CFRetain((CFTypeRef)self);
    return self;
}

- (oneway void)release {
    if (__NSArrayIsForeign(self)) {
        [super release];
        return;
    }
    CFRelease((CFTypeRef)self);
}

- (NSUInteger)retainCount {
    if (__NSArrayIsForeign(self))
        return [super retainCount];
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

/* The read side of -writeToFile:atomically:, which existed without it - so
 * round-tripping an array plist hit an unrecognized selector and aborted the
 * process. NSDictionary's loader cannot be shared: it rejects any root that is
 * not a dictionary. */
+ (nullable instancetype)arrayWithContentsOfFile:(NSString *)path {
    if (path == nil) {
        return nil;
    }

    NSData *data = [NSData dataWithContentsOfFile:path];

    if (data == nil) {
        return nil;
    }

    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, (CFDataRef)data, kCFPropertyListImmutable, NULL, NULL);

    if (plist == NULL) {
        return nil;
    }
    if (CFGetTypeID(plist) != CFArrayGetTypeID()) {
        CFRelease(plist);   /* a plist root may legitimately be a dictionary */
        return nil;
    }

    NSArray *result = [(id)plist autorelease];

    /* Sent to NSMutableArray this has to hand back something mutable; the
     * parser only ever produces an immutable array. */
    if ([self isSubclassOfClass:[NSMutableArray class]]) {
        return [[result mutableCopy] autorelease];
    }
    return result;
}

+ (nullable instancetype)arrayWithContentsOfURL:(NSURL *)url {
    if (![url isFileURL]) {
        return nil;
    }
    return [self arrayWithContentsOfFile:[url path]];
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
    if (__NSArrayIsForeign(self) || __NSArrayIsForeign(other)) {
        NSUInteger count = [self count];

        if ([other count] != count)
            return NO;
        for (NSUInteger i = 0; i < count; i++) {
            id left = [self objectAtIndex:i], right = [other objectAtIndex:i];

            if (left != right && ![left isEqual:right])
                return NO;
        }
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

- (NSArray *)sortedArrayUsingDescriptors:(NSArray *)descriptors {
    NSMutableArray *result = [NSMutableArray arrayWithArray:self];

    [result sortUsingDescriptors:descriptors];
    return result;
}

- (NSArray *)sortedArrayUsingComparator:(NSComparator)comparator {
    NSMutableArray *result = [[self mutableCopy] autorelease];

    [result sortUsingComparator:comparator];
    return result;
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
    if (__NSArrayIsForeign(self)) {
        [self insertObject:object atIndex:[self count]];
        return;
    }
    CFArrayAppendValue((CFMutableArrayRef)self, (const void *)object);
}

- (void)insertObject:(id)object atIndex:(NSUInteger)index {
    if (__NSArrayIsForeign(self)) {
        __NSArrayAbstract(self, _cmd);
        return;
    }
    CFArrayInsertValueAtIndex((CFMutableArrayRef)self, (CFIndex)index,
                              (const void *)object);
}

// CFArraySetValueAtIndex sends this to arrays that are not CFArrays
- (void)setObject:(id)object atIndex:(NSUInteger)index {
    if (!__NSArrayIsForeign(self)) {
        CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index, object);
        return;
    }
    if (index == [self count])
        [self insertObject:object atIndex:index];
    else
        [self replaceObjectAtIndex:index withObject:object];
}

// CFArrayReplaceValues and everything built on it send this to arrays that are not CFArrays
- (void)replaceObjectsInRange:(NSRange)range withObjects:(const id *)objects count:(NSUInteger)count {
    if (!__NSArrayIsForeign(self)) {
        CFArrayReplaceValues((CFMutableArrayRef)self, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                             (const void **)objects, (CFIndex)count);
        return;
    }
    for (NSUInteger i = range.length; i > 0; i--) {
        [self removeObjectAtIndex:range.location + i - 1];
    }
    for (NSUInteger i = 0; i < count; i++) {
        [self insertObject:objects[i] atIndex:range.location + i];
    }
}

- (void)setArray:(NSArray *)array {
    CFArrayRemoveAllValues((CFMutableArrayRef)self);

    if (array != nil) {
        CFArrayAppendArray((CFMutableArrayRef)self, (CFArrayRef)array,
                           CFRangeMake(0, CFArrayGetCount((CFArrayRef)array)));
    }
}

- (void)removeObjectsInArray:(NSArray *)array {
    NSUInteger count = [array count];

    for (NSUInteger index = 0; index < count; index++) {
        [self removeObject:[array objectAtIndex:index]];
    }
}

- (void)exchangeObjectAtIndex:(NSUInteger)index1
            withObjectAtIndex:(NSUInteger)index2 {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    if ((CFIndex)index1 >= count || (CFIndex)index2 >= count) {
        [NSException raise:NSRangeException
                    format:@"-[NSMutableArray exchangeObjectAtIndex:%lu withObjectAtIndex:%lu]: "
                           @"beyond count %ld",
                           (unsigned long)index1, (unsigned long)index2, (long)count];
        return;
    }

    id first = [[(id)CFArrayGetValueAtIndex((CFArrayRef)self, (CFIndex)index1) retain] autorelease];
    id second = (id)CFArrayGetValueAtIndex((CFArrayRef)self, (CFIndex)index2);

    CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index1, second);
    CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index2, first);
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
    if (__NSArrayIsForeign(self)) {
        __NSArrayAbstract(self, _cmd);
        return;
    }
    CFArrayRemoveValueAtIndex((CFMutableArrayRef)self, (CFIndex)index);
}

- (void)removeLastObject {
    CFIndex count = CFArrayGetCount((CFArrayRef)self);

    if (__NSArrayIsForeign(self)) {
        if (count > 0)
            [self removeObjectAtIndex:(NSUInteger)(count - 1)];
        return;
    }
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
    if (__NSArrayIsForeign(self)) {
        __NSArrayAbstract(self, _cmd);
        return;
    }
    CFArraySetValueAtIndex((CFMutableArrayRef)self, (CFIndex)index, object);
}

// array[i] = object: replaces, or appends when i is the count (CF-backed arrays get mutators here, as above)
- (void)setObject:(id)object atIndexedSubscript:(NSUInteger)index {
    if (index == [self count]) {
        [self addObject:object];
    } else {
        [self replaceObjectAtIndex:index withObject:object];
    }
}

- (void)removeAllObjects {
    if (__NSArrayIsForeign(self)) {
        for (NSUInteger count = [self count]; count > 0; count--) {
            [self removeObjectAtIndex:count - 1];
        }
        return;
    }
    CFArrayRemoveAllValues((CFMutableArrayRef)self);
}

- (void)sortUsingSelector:(SEL)selector {
    CFMutableArrayRef array = (CFMutableArrayRef)self;
    CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)),
                      ns_array_compare, selector);
}

/* The block form. CFArraySortValues passes the context straight through, so
 * the comparator travels as that context. */
static CFComparisonResult ns_array_compare_block(const void *left,
                                                 const void *right,
                                                 void *context) {
    NSComparator comparator = (NSComparator)context;

    return (CFComparisonResult)comparator((id)left, (id)right);
}

- (void)sortUsingComparator:(NSComparator)comparator {
    if (comparator == nil) {
        return;
    }

    CFMutableArrayRef array = (CFMutableArrayRef)self;

    CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)),
                      ns_array_compare_block, comparator);
}

- (void)sortWithOptions:(NSSortOptions)options usingComparator:(NSComparator)comparator {
    [self sortUsingComparator:comparator];
}

/* Each descriptor breaks the previous one's ties, which is the order the
 * descriptors are given in. */
- (void)sortUsingDescriptors:(NSArray *)descriptors {
    if ([descriptors count] == 0) {
        return;
    }

    [self sortUsingComparator:^NSComparisonResult(id first, id second) {
        for (NSSortDescriptor *descriptor in descriptors) {
            NSComparisonResult order = [descriptor compareObject:first
                                                        toObject:second];

            if (order != NSOrderedSame) {
                return order;
            }
        }
        return NSOrderedSame;
    }];
}

/* Bridged to CFArrayRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    // CF hashes an array by its count too
    if (__NSArrayIsForeign(self))
        return [self count];
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSArray class]]) {
        return NO;
    }
    return [self isEqualToArray:other];
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

// the block enumerations, written over -count and -objectAtIndex: so every subclass gets them
@implementation NSArray (NSArrayBlocks)

- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)options
                         usingBlock:(void (^)(id object, NSUInteger index, BOOL *stop))block {
    NSUInteger count = [self count];
    BOOL stop = NO;

    for (NSUInteger i = 0; i < count && !stop; i++) {
        NSUInteger index = (options & NSEnumerationReverse) ? count - 1 - i : i;

        block([self objectAtIndex:index], index, &stop);
    }
}

- (void)enumerateObjectsUsingBlock:(void (^)(id object, NSUInteger index, BOOL *stop))block {
    [self enumerateObjectsWithOptions:0 usingBlock:block];
}

- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)options
                           passingTest:(BOOL (^)(id object, NSUInteger index, BOOL *stop))predicate {
    __block NSUInteger found = NSNotFound;

    [self enumerateObjectsWithOptions:options usingBlock:^(id object, NSUInteger index, BOOL *stop) {
        if (predicate(object, index, stop)) {
            found = index;
            *stop = YES;
        }
    }];
    return found;
}

- (NSUInteger)indexOfObjectPassingTest:(BOOL (^)(id object, NSUInteger index, BOOL *stop))predicate {
    return [self indexOfObjectWithOptions:0 passingTest:predicate];
}

// the older accessors AppKit still sends, also over -count and -objectAtIndex:
- (void)getObjects:(id __unsafe_unretained *)objects {
    NSUInteger count = [self count];

    for (NSUInteger i = 0; i < count; i++)
        objects[i] = [self objectAtIndex:i];
}

- (NSArray *)objectsAtIndexes:(NSIndexSet *)indexes {
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:[indexes count]];

    for (NSUInteger i = [indexes firstIndex]; i != NSNotFound; i = [indexes indexGreaterThanIndex:i])
        [result addObject:[self objectAtIndex:i]];
    return result;
}

- (NSEnumerator *)reverseObjectEnumerator {
    NSUInteger count = [self count];
    NSMutableArray *reversed = [NSMutableArray arrayWithCapacity:count];

    for (NSUInteger i = count; i > 0; i--)
        [reversed addObject:[self objectAtIndex:i - 1]];
    return [reversed objectEnumerator];
}

- (id)firstObjectCommonWithArray:(NSArray *)other {
    NSUInteger count = [self count];

    for (NSUInteger i = 0; i < count; i++) {
        id object = [self objectAtIndex:i];

        if ([other containsObject:object])
            return object;
    }
    return nil;
}

@end

@implementation NSMutableArray

+ (instancetype)new {
    if (self != [NSMutableArray class])
        return [[self alloc] init];
    return [self arrayWithCapacity:0];
}

+ (instancetype)arrayWithCapacity:(NSUInteger)capacity {
    /* A non-zero CF capacity is a hard ceiling, while the Cocoa capacity is
     * only a hint; passing it through makes the collection halt once it grows
     * past the hint. See +[NSMutableData dataWithCapacity:]. */
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                    &ns_array_callbacks);
}

+ (instancetype)array {
    return [self arrayWithCapacity:0];
}

- (instancetype)init {
    if (__NSArrayIsForeign(self))
        return [super init];
    // Must be +1: callers reach here through -alloc/-init and +new.
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, 0, &ns_array_callbacks);
}

- (instancetype)initWithCapacity:(NSUInteger)capacity {
    /* A non-zero CF capacity is a hard ceiling, while the Cocoa capacity is
     * only a hint; passing it through makes the collection halt once it grows
     * past the hint. See +[NSMutableData dataWithCapacity:]. */
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                    &ns_array_callbacks);
}

/* Inherited from NSArray these would hand back an immutable CFArray, and the
 * first -addObject: on it is undefined behaviour. */
+ (instancetype)arrayWithObject:(id)object {
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 1,
                                                     &ns_array_callbacks);
    if (object != nil) {
        CFArrayAppendValue(result, object);
    }
    return (id)result;
}

+ (instancetype)arrayWithObjects:(id)firstObject, ... {
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                     &ns_array_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFArrayAppendValue(result, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    return (id)result;
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

// Inheriting these from NSArray returned an immutable CFArray, so the first
// -addObject: appended to an immutable object and left a NULL slot behind.
- (instancetype)initWithObjects:(id)firstObject, ... {
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                    &ns_array_callbacks);
    va_list arguments;
    id object = firstObject;

    va_start(arguments, firstObject);
    while (object != nil) {
        CFArrayAppendValue(result, object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);

    return (id)result;
}

- (instancetype)initWithObjects:(const id *)objects count:(NSUInteger)count {
    CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                    &ns_array_callbacks);
    for (NSUInteger index = 0; index < count; index++) {
        CFArrayAppendValue(result, objects[index]);
    }
    return (id)result;
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFArrayBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFArrayGetTypeID(), "NSArray");
}
#endif
