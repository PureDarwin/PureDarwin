/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSZone.h>
#import <Foundation/NSString.h>
#include <CoreFoundation/CFString.h>
#include <objc/runtime.h>
#include <stdlib.h>
#include <string.h>

/* NSZone is opaque everywhere else; give it a body here so the stand-in below
 * is a complete type. */
struct _NSZone {
    int unused;
};

/* A single stand-in zone. It is never dereferenced - callers only ever pass it
 * back to us - but it must be non-NULL so that "zone ?: default" idioms in
 * ported code take the branch they expect. */
static struct _NSZone __NSDefaultZone;

NSZone *NSDefaultMallocZone(void) {
    return &__NSDefaultZone;
}

NSZone *NSCreateZone(NSUInteger startSize, NSUInteger granularity, BOOL canFree) {
    (void)startSize; (void)granularity; (void)canFree;
    return &__NSDefaultZone;
}

void NSRecycleZone(NSZone *zone) {
    (void)zone;
}

/* Without per-zone bookkeeping every pointer belongs to the default zone. */
NSZone *NSZoneFromPointer(void *ptr) {
    (void)ptr;
    return &__NSDefaultZone;
}

void *NSZoneMalloc(NSZone *zone, NSUInteger size) {
    (void)zone;
    return malloc((size_t)size);
}

void *NSZoneCalloc(NSZone *zone, NSUInteger numElems, NSUInteger byteSize) {
    (void)zone;
    return calloc((size_t)numElems, (size_t)byteSize);
}

void *NSZoneRealloc(NSZone *zone, void *ptr, NSUInteger size) {
    (void)zone;
    return realloc(ptr, (size_t)size);
}

void NSZoneFree(NSZone *zone, void *ptr) {
    (void)zone;
    free(ptr);
}

void NSSetZoneName(NSZone *zone, NSString *name) {
    (void)zone; (void)name;
}

NSString *NSZoneName(NSZone *zone) {
    (void)zone;
    return (NSString *)CFSTR("default");
}

id NSAllocateObject(Class aClass, NSUInteger extraBytes, NSZone *zone) {
    (void)zone;
    return class_createInstance(aClass, (size_t)extraBytes);
}

void NSDeallocateObject(id object) {
    if (object) {
        object_dispose(object);
    }
}

/* class_createInstance zeroes the new instance, so only the source bytes need
 * copying; the extra bytes past instanceSize stay zeroed, as they do on Apple. */
id NSCopyObject(id object, NSUInteger extraBytes, NSZone *zone) {
    (void)zone;
    if (!object) {
        return nil;
    }

    Class cls = object_getClass(object);
    size_t size = class_getInstanceSize(cls);
    id copy = class_createInstance(cls, (size_t)extraBytes);
    if (copy) {
        memcpy((char *)copy + sizeof(Class), (const char *)object + sizeof(Class),
               size - sizeof(Class));
    }
    return copy;
}
