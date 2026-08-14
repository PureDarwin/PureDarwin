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
#include <objc/runtime.h>

@implementation NSArray

+ (instancetype)array {
    return (id)CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks);
}

+ (instancetype)arrayWithObjects:(const id *)objects count:(NSUInteger)count {
    return (id)CFArrayCreate(kCFAllocatorDefault, (const void **)objects,
                             (CFIndex)count, &kCFTypeArrayCallBacks);
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

@end

@implementation NSMutableArray

+ (instancetype)arrayWithCapacity:(NSUInteger)capacity {
    return (id)CFArrayCreateMutable(kCFAllocatorDefault, (CFIndex)capacity,
                                    &kCFTypeArrayCallBacks);
}

+ (instancetype)array {
    return [self arrayWithCapacity:0];
}

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

- (void)removeAllObjects {
    CFArrayRemoveAllValues((CFMutableArrayRef)self);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFArrayBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFArrayGetTypeID(), "NSArray");
}
#endif
