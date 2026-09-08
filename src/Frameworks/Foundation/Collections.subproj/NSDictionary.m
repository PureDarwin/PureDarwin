/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSDictionary.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSError.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFPropertyList.h>
#include <CoreFoundation/CFURL.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFStream.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

extern int __CFConstantStringClassReference[];

static BOOL ns_dictionary_value_is_constant_string(const void *value) {
    return *(const uintptr_t *)value ==
        (uintptr_t)&__CFConstantStringClassReference;
}

static const void *ns_dictionary_value_retain(CFAllocatorRef allocator,
                                               const void *value) {
    if (ns_dictionary_value_is_constant_string(value)) {
        return CFRetain(value);
    }
    return [(id)value retain];
}

static void ns_dictionary_value_release(CFAllocatorRef allocator,
                                        const void *value) {
    if (ns_dictionary_value_is_constant_string(value)) {
        CFRelease(value);
        return;
    }
    [(id)value release];
}

static Boolean ns_dictionary_value_equal(const void *left,
                                         const void *right) {
    return [(id)left isEqual:(id)right] ? true : false;
}

static CFHashCode ns_dictionary_key_hash(const void *value) {
    return (CFHashCode)[(id)value hash];
}

static const CFDictionaryKeyCallBacks ns_dictionary_key_callbacks = {
    0,
    ns_dictionary_value_retain,
    ns_dictionary_value_release,
    NULL,
    ns_dictionary_value_equal,
    ns_dictionary_key_hash
};

static const CFDictionaryValueCallBacks ns_dictionary_value_callbacks = {
    0,
    ns_dictionary_value_retain,
    ns_dictionary_value_release,
    CFCopyDescription,
    ns_dictionary_value_equal
};

/* Read a whole file into a CFData. CFReadStream would do, but plists are small
 * and stdio keeps this independent of the stream machinery. */
static CFDataRef pd_read_file(CFStringRef path) {
    char buf[1024];
    if (!CFStringGetCString(path, buf, sizeof(buf), kCFStringEncodingUTF8)) {
        return NULL;
    }

    FILE *f = fopen(buf, "rb");
    if (f == NULL) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    UInt8 *bytes = malloc((size_t)size);
    if (bytes == NULL) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(bytes, 1, (size_t)size, f);
    fclose(f);

    CFDataRef data = CFDataCreate(kCFAllocatorDefault, bytes, (CFIndex)got);
    free(bytes);
    return data;
}

static CFPropertyListRef pd_plist_from_path(CFStringRef path) {
    CFDataRef data = pd_read_file(path);
    if (data == NULL) {
        return NULL;
    }
    CFPropertyListRef plist = CFPropertyListCreateWithData(
        kCFAllocatorDefault, data, kCFPropertyListImmutable, NULL, NULL);
    CFRelease(data);

    if (plist != NULL && CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
        CFRelease(plist);
        return NULL;
    }
    return plist;
}

static void pd_add_dictionary_entry(const void *key, const void *value,
                                    void *context) {
    CFDictionarySetValue((CFMutableDictionaryRef)context, key, value);
}

@implementation NSDictionary

/* These classes are bridged onto CF, so an instance has to be a CF object.
 * NSObject's +new/-init would hand back a plain ObjC allocation, and the first
 * CFDictionaryGetValue() on it crashes. */
+ (instancetype)new {
    return [self dictionary];
}

- (instancetype)init {
    return [NSDictionary dictionary];
}

+ (instancetype)dictionary {
    return (id)CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
                                  &ns_dictionary_key_callbacks,
                                  &ns_dictionary_value_callbacks);
}

+ (instancetype)dictionaryWithObject:(id)object forKey:(id)key {
    const void *keys[] = { (const void *)key };
    const void *objects[] = { (const void *)object };
    return (id)CFDictionaryCreate(kCFAllocatorDefault, keys, objects, 1,
                                  &ns_dictionary_key_callbacks,
                                  &ns_dictionary_value_callbacks);
}

+ (instancetype)dictionaryWithObjectsAndKeys:(id)firstObject, ... {
    CFMutableDictionaryRef result = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &ns_dictionary_key_callbacks,
        &ns_dictionary_value_callbacks);
    if (result == NULL || firstObject == nil)
        return (id)result;

    va_list arguments;
    va_start(arguments, firstObject);
    id object = firstObject;
    while (object != nil) {
        id key = va_arg(arguments, id);
        if (key == nil)
            break;
        CFDictionarySetValue(result, (const void *)key, (const void *)object);
        object = va_arg(arguments, id);
    }
    va_end(arguments);
    return (id)result;
}

+ (instancetype)dictionaryWithObjects:(NSArray *)objects
                               forKeys:(NSArray *)keys {
    NSUInteger count = [objects count];
    if ([keys count] != count)
        return nil;

    CFMutableDictionaryRef result = CFDictionaryCreateMutable(
        kCFAllocatorDefault, (CFIndex)count, &ns_dictionary_key_callbacks,
        &ns_dictionary_value_callbacks);
    for (NSUInteger i = 0; i < count; i++) {
        CFDictionarySetValue(result, [keys objectAtIndex:i],
                             [objects objectAtIndex:i]);
    }
    return (id)result;
}

+ (instancetype)dictionaryWithDictionary:(NSDictionary *)dictionary {
    return (id)CFDictionaryCreateCopy(kCFAllocatorDefault,
                                      (CFDictionaryRef)dictionary);
}

/* CFDictionaryCreate takes keys first, the ObjC spelling takes objects first. */
+ (instancetype)dictionaryWithObjects:(const id *)objects
                              forKeys:(const id *)keys
                                count:(NSUInteger)count {
    return (id)CFDictionaryCreate(kCFAllocatorDefault,
                                  (const void **)keys, (const void **)objects,
                                  (CFIndex)count,
                                  &ns_dictionary_key_callbacks,
                                  &ns_dictionary_value_callbacks);
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
    return (id)CFDictionaryCreateCopy(kCFAllocatorDefault, (CFDictionaryRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0,
                                             (CFDictionaryRef)self);
}

/* One class is bridged per CFTypeID, so even a CFMutableDictionary reports
 * NSDictionary. The mutators have to live here or they are unreachable, the
 * same way NSArray.m keeps addObject: and friends. */
- (void)setObject:(id)object forKey:(id)key {
    CFDictionarySetValue((CFMutableDictionaryRef)self, (const void *)key,
                         (const void *)object);
}

- (void)setObject:(id)object forKeyedSubscript:(id)key {
    [self setObject:object forKey:key];
}

- (void)removeObjectForKey:(id)key {
    CFDictionaryRemoveValue((CFMutableDictionaryRef)self, (const void *)key);
}

- (void)addEntriesFromDictionary:(NSDictionary *)dictionary {
    CFDictionaryApplyFunction((CFDictionaryRef)dictionary,
                              pd_add_dictionary_entry, self);
}

- (void)removeAllObjects {
    CFDictionaryRemoveAllValues((CFMutableDictionaryRef)self);
}

- (void)removeObjectsForKeys:(NSArray *)keys {
    for (NSUInteger i = 0; i < [keys count]; i++)
        [self removeObjectForKey:[keys objectAtIndex:i]];
}

- (void)setDictionary:(NSDictionary *)dictionary {
    [self removeAllObjects];
    [self addEntriesFromDictionary:dictionary];
}

- (void)enumerateKeysAndObjectsUsingBlock:(void (^)(id, id, BOOL *))block {
    CFIndex n = CFDictionaryGetCount((CFDictionaryRef)self);
    if (n <= 0 || block == NULL) {
        return;
    }
    const void **keys = malloc((size_t)n * sizeof(*keys));
    const void **values = malloc((size_t)n * sizeof(*values));
    if (keys == NULL || values == NULL) {
        free(keys);
        free(values);
        return;
    }
    /* Snapshot first: the block is allowed to mutate a mutable receiver. */
    CFDictionaryGetKeysAndValues((CFDictionaryRef)self, keys, values);
    BOOL stop = NO;
    for (CFIndex i = 0; i < n && !stop; i++) {
        block((__bridge id)keys[i], (__bridge id)values[i], &stop);
    }
    free(keys);
    free(values);
}

+ (nullable instancetype)dictionaryWithContentsOfURL:(NSURL *)url
                                               error:(NSError **)error {
    if (error != NULL) {
        *error = nil;
    }
    if (url == nil) {
        return nil;
    }

    CFStringRef path = CFURLCopyFileSystemPath((CFURLRef)url, kCFURLPOSIXPathStyle);
    if (path == NULL) {
        return nil;
    }
    CFPropertyListRef plist = pd_plist_from_path(path);
    CFRelease(path);
    return (id)plist;
}

+ (nullable instancetype)dictionaryWithContentsOfFile:(NSString *)path {
    if (path == nil) {
        return nil;
    }
    return (id)pd_plist_from_path((CFStringRef)path);
}

- (NSUInteger)count {
    return (NSUInteger)CFDictionaryGetCount((CFDictionaryRef)self);
}

- (nullable id)objectForKey:(id)key {
    return (id)CFDictionaryGetValue((CFDictionaryRef)self, (const void *)key);
}

- (nullable id)objectForKeyedSubscript:(id)key {
    return [self objectForKey:key];
}

/* Enumerates keys, which is what for..in over a dictionary yields. The key
 * array is created once on the first call and parked in the state so the rest
 * of the loop is just indexing. */
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained [])buffer
                                    count:(NSUInteger)length {
    NSArray *keys;

    if (state->state == 0) {
        keys = [[self allKeys] autorelease];
        state->extra[1] = (unsigned long)keys;
    } else {
        keys = (NSArray *)state->extra[1];
    }

    if (keys == nil) {
        return 0;
    }
    return [keys countByEnumeratingWithState:state objects:buffer count:length];
}

- (NSArray *)allKeys {
    CFIndex n = CFDictionaryGetCount((CFDictionaryRef)self);
    const void **keys = malloc(sizeof(void *) * (size_t)(n > 0 ? n : 1));
    if (keys == NULL) {
        return nil;
    }
    CFDictionaryGetKeysAndValues((CFDictionaryRef)self, keys, NULL);
    CFArrayRef array = CFArrayCreate(kCFAllocatorDefault, keys, n,
                                     &kCFTypeArrayCallBacks);
    free(keys);
    return (NSArray *)array;
}

@end

@implementation NSMutableDictionary

+ (instancetype)new {
    return [self dictionaryWithCapacity:0];
}

- (instancetype)init {
    return [NSMutableDictionary dictionaryWithCapacity:0];
}

+ (instancetype)dictionaryWithCapacity:(NSUInteger)capacity {
    return (id)CFDictionaryCreateMutable(kCFAllocatorDefault, (CFIndex)capacity,
                                         &ns_dictionary_key_callbacks,
                                         &ns_dictionary_value_callbacks);
}

+ (instancetype)dictionary {
    return [self dictionaryWithCapacity:0];
}

+ (instancetype)dictionaryWithDictionary:(NSDictionary *)dictionary {
    return (id)CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0,
                                             (CFDictionaryRef)dictionary);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFDictionaryBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFDictionaryGetTypeID(), "NSDictionary");
}
#endif
