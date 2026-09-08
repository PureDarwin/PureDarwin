/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import "NSCFString.h"
#import <Foundation/NSArray.h>
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
    UniChar character = 0;

    _CFStringCheckAndGetCharacterAtIndex((CFStringRef)self,
                                          (CFIndex)index,
                                          &character);
    return (unichar)character;
}

- (void)getCharacters:(unichar *)buffer range:(NSRange)range {
    _CFStringCheckAndGetCharacters((CFStringRef)self,
                                   CFRangeMake((CFIndex)range.location,
                                               (CFIndex)range.length),
                                   (UniChar *)buffer);
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

- (const char *)fileSystemRepresentation {
    return [self UTF8String];
}

- (NSString *)description {
    return self;
}

- (NSComparisonResult)compare:(NSString *)other {
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other, 0);
}

- (NSComparisonResult)caseInsensitiveCompare:(NSString *)other {
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other,
                                               kCFCompareCaseInsensitive);
}

/* Expands range to whole lines, the way the text system expects: back to the
 * start of the line containing range.location, forward past the terminator
 * that ends the line containing its last character. */
- (NSRange)lineRangeForRange:(NSRange)range {
    CFIndex start = 0, end = 0, contentsEnd = 0;

    CFStringGetLineBounds((CFStringRef)self,
        CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
        &start, &end, &contentsEnd);
    return NSMakeRange((NSUInteger)start, (NSUInteger)(end - start));
}

/* CF has no paragraph-bounds call, and for the separators CFStringGetLineBounds
 * recognises the two agree on everything except U+2028; close enough for the
 * text system until a real paragraph walk exists. */
- (NSRange)paragraphRangeForRange:(NSRange)range {
    return [self lineRangeForRange:range];
}

- (NSRange)rangeOfString:(NSString *)string {
    return [self rangeOfString:string options:0];
}

- (NSRange)rangeOfString:(NSString *)string options:(NSStringCompareOptions)options {
    return [self rangeOfString:string
                       options:options
                         range:NSMakeRange(0, [self length])];
}

/* NSStringCompareOptions and CFStringCompareFlags share their bit values for
 * the flags that exist in both, so the mask passes straight through. */
- (NSRange)rangeOfString:(NSString *)string
                 options:(NSStringCompareOptions)options
                   range:(NSRange)searchRange {
    CFRange found;
    Boolean ok = CFStringFindWithOptions((CFStringRef)self, (CFStringRef)string,
        CFRangeMake((CFIndex)searchRange.location, (CFIndex)searchRange.length),
        (CFStringCompareFlags)options, &found);

    if (!ok) {
        return NSMakeRange(NSNotFound, 0);
    }
    return NSMakeRange((NSUInteger)found.location, (NSUInteger)found.length);
}

- (NSArray *)componentsSeparatedByString:(NSString *)separator {
    CFArrayRef result = CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault,
        (CFStringRef)self, (CFStringRef)separator);

    return (NSArray *)CFAutorelease(result);
}

- (NSString *)substringWithRange:(NSRange)range {
    CFStringRef result = CFStringCreateWithSubstring(kCFAllocatorDefault,
        (CFStringRef)self, CFRangeMake((CFIndex)range.location, (CFIndex)range.length));

    return (NSString *)CFAutorelease(result);
}

- (NSString *)stringByAppendingString:(NSString *)string {
    CFMutableStringRef result = CFStringCreateMutableCopy(kCFAllocatorDefault,
        0, (CFStringRef)self);

    CFStringAppend(result, (CFStringRef)string);
    return (NSString *)CFAutorelease(result);
}

- (NSString *)substringFromIndex:(NSUInteger)index {
    return [self substringWithRange:NSMakeRange(index, [self length] - index)];
}

- (NSString *)substringToIndex:(NSUInteger)index {
    return [self substringWithRange:NSMakeRange(0, index)];
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

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string {
    __CFStringCheckAndReplace((CFMutableStringRef)self,
                              CFRangeMake((CFIndex)range.location,
                                          (CFIndex)range.length),
                              (CFStringRef)string);
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
