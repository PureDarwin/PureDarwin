/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import "NSCFString.h"
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <CoreFoundation/CFRuntime.h>

/* The compiler emits every @"..." with its isa pointing at
 * ___CFConstantStringClassReference, so that symbol has to *be* a class.
 * CoreFoundation only references it (CFRuntime.c) - Foundation owns the class,
 * matching Apple, where __NSCFConstantString lives in Foundation. */
__asm__(".globl ___CFConstantStringClassReference\n\t"
        ".set ___CFConstantStringClassReference, _OBJC_CLASS_$_NSCFString");

CF_EXPORT void *__CFConstantStringClassReferencePtr;
extern int __CFConstantStringClassReference[];

@implementation NSCFString

- (NSUInteger)length {
    return (NSUInteger)CFStringGetLength((CFStringRef)self);
}

- (unichar)characterAtIndex:(NSUInteger)index {
    return (unichar)CFStringGetCharacterAtIndex((CFStringRef)self, (CFIndex)index);
}

- (const char *)UTF8String {
    const char *direct = CFStringGetCStringPtr((CFStringRef)self, kCFStringEncodingUTF8);
    if (direct) {
        return direct;
    }

    CFIndex length = CFStringGetLength((CFStringRef)self);
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char *buffer = (char *)CFAllocatorAllocate(CFGetAllocator((CFStringRef)self), maxSize, 0);
    if (!CFStringGetCString((CFStringRef)self, buffer, maxSize, kCFStringEncodingUTF8)) {
        CFAllocatorDeallocate(CFGetAllocator((CFStringRef)self), buffer);
        return NULL;
    }
    return buffer;
}

- (NSString *)description {
    return self;
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
    return (id)CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFStringCreateMutableCopy(kCFAllocatorDefault, 0, (CFStringRef)self);
}

- (BOOL)getBytes:(void *)buffer
       maxLength:(NSUInteger)maxBufferCount
      usedLength:(NSUInteger *)usedBufferCount
        encoding:(NSStringEncoding)encoding
         options:(NSStringEncodingConversionOptions)options
           range:(NSRange)range
  remainingRange:(NSRange *)leftover {
    CFStringEncoding cfEncoding = (encoding == NSUTF8StringEncoding)
        ? kCFStringEncodingUTF8 : kCFStringEncodingASCII;
    CFRange cfRange = CFRangeMake((CFIndex)range.location, (CFIndex)range.length);
    CFIndex used = 0;
    CFIndex converted = CFStringGetBytes((CFStringRef)self, cfRange, cfEncoding,
        '?', false, (UInt8 *)buffer, (CFIndex)maxBufferCount, &used);
    if (usedBufferCount) {
        *usedBufferCount = (NSUInteger)used;
    }
    if (leftover) {
        leftover->location = range.location + (NSUInteger)converted;
        leftover->length = range.length - (NSUInteger)converted;
    }
    return converted == range.length;
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFStringBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFStringGetTypeID(), "NSCFString");
    /* CF_IS_OBJC compares an object's isa against this to spot a constant
     * string; left NULL it treats every literal as a foreign ObjC object. */
    __CFConstantStringClassReferencePtr = __CFConstantStringClassReference;
}
#endif
