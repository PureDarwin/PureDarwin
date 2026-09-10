/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import "NSCFString.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSException.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

/* Defined in NSString.m: renders %@ by asking the object for a description. */
extern CFStringRef _NSCopyFormattingDescription(void *value, const void *locale);
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFCharacterSet.h>
#include <CoreFoundation/ForFoundationOnly.h>
#include <CoreFoundation/CFRuntime.h>
#include <objc/runtime.h>

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

- (instancetype)initWithData:(NSData *)data encoding:(NSStringEncoding)encoding {
    [self release];
    if (data == nil) {
        return nil;
    }
    return (id)CFStringCreateWithBytes(kCFAllocatorDefault, [data bytes],
                                       (CFIndex)[data length],
                                       (CFStringEncoding)encoding, false);
}

- (BOOL)hasPrefix:(NSString *)prefix {
    if (prefix == nil) {
        return NO;
    }
    return CFStringHasPrefix((CFStringRef)self, (CFStringRef)prefix) ? YES : NO;
}

- (BOOL)hasSuffix:(NSString *)suffix {
    if (suffix == nil) {
        return NO;
    }
    return CFStringHasSuffix((CFStringRef)self, (CFStringRef)suffix) ? YES : NO;
}

- (BOOL)containsString:(NSString *)string {
    if (string == nil) {
        return NO;
    }
    return [self rangeOfString:string].location != NSNotFound;
}

- (NSString *)lowercaseString {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringLowercase(copy, NULL);
    return (id)CFAutorelease(copy);
}

- (NSString *)uppercaseString {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringUppercase(copy, NULL);
    return (id)CFAutorelease(copy);
}

- (NSString *)capitalizedString {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringCapitalize(copy, NULL);
    return (id)CFAutorelease(copy);
}

/* Trims from both ends, stopping at the first character not in the set. */
- (NSString *)stringByTrimmingCharactersInSet:(NSCharacterSet *)set {
    NSUInteger length = [self length];
    NSUInteger start = 0;
    NSUInteger end = length;

    while (start < end &&
           CFCharacterSetIsCharacterMember((CFCharacterSetRef)set,
                                           CFStringGetCharacterAtIndex((CFStringRef)self,
                                                                       (CFIndex)start))) {
        start++;
    }
    while (end > start &&
           CFCharacterSetIsCharacterMember((CFCharacterSetRef)set,
                                           CFStringGetCharacterAtIndex((CFStringRef)self,
                                                                       (CFIndex)(end - 1)))) {
        end--;
    }
    return [self substringWithRange:NSMakeRange(start, end - start)];
}

/* CFStringCompare dereferences its argument, so nil has to be caught here.
 * Apple raises for this rather than returning an order. */
#define PD_REQUIRE_STRING(arg) \
    do { \
        if ((arg) == nil) \
            [NSException raise:NSInvalidArgumentException \
                        format:@"-[%@ %s]: nil argument", \
                               NSStringFromClass([self class]), sel_getName(_cmd)]; \
    } while (0)

/* Numeric accessors. Their absence is quiet rather than loud: callers such as
 * -[NSUserDefaults integerForKey:] test respondsToSelector: first and simply
 * return 0, so every integer default read as zero. */
- (int)intValue {
    return (int)[self integerValue];
}

- (NSInteger)integerValue {
    const char *utf8 = [self UTF8String];

    return (utf8 != NULL) ? (NSInteger)strtol(utf8, NULL, 10) : 0;
}

- (long long)longLongValue {
    const char *utf8 = [self UTF8String];

    return (utf8 != NULL) ? strtoll(utf8, NULL, 10) : 0;
}

- (float)floatValue {
    return (float)[self doubleValue];
}

- (double)doubleValue {
    const char *utf8 = [self UTF8String];

    return (utf8 != NULL) ? strtod(utf8, NULL) : 0.0;
}

/* Cocoa: leading whitespace and sign are skipped, then Y/y/T/t or a non-zero
 * number is true. */
- (BOOL)boolValue {
    const char *utf8 = [self UTF8String];

    if (utf8 == NULL) {
        return NO;
    }
    while (*utf8 == ' ' || *utf8 == '\t' || *utf8 == '\n' || *utf8 == '\r') {
        utf8++;
    }
    if (*utf8 == '+' || *utf8 == '-') {
        utf8++;
    }
    if (*utf8 == 'Y' || *utf8 == 'y' || *utf8 == 'T' || *utf8 == 't') {
        return YES;
    }
    return (strtol(utf8, NULL, 10) != 0) ? YES : NO;
}

- (NSComparisonResult)compare:(NSString *)other {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other, 0);
}

/* NSStringCompareOptions are laid out to match the CFStringCompareFlags. */
- (NSComparisonResult)compare:(NSString *)other options:(NSStringCompareOptions)options {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompare((CFStringRef)self, (CFStringRef)other,
                                               (CFStringCompareFlags)options);
}

- (NSComparisonResult)compare:(NSString *)other
                      options:(NSStringCompareOptions)options
                        range:(NSRange)range {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompareWithOptions((CFStringRef)self,
        (CFStringRef)other, CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
        (CFStringCompareFlags)options);
}

- (NSComparisonResult)localizedCompare:(NSString *)other {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompare((CFStringRef)self, (CFStringRef)other,
                                               kCFCompareLocalized);
}

- (NSComparisonResult)localizedCaseInsensitiveCompare:(NSString *)other {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompare((CFStringRef)self, (CFStringRef)other,
                                               kCFCompareLocalized | kCFCompareCaseInsensitive);
}

- (NSComparisonResult)caseInsensitiveCompare:(NSString *)other {
    PD_REQUIRE_STRING(other);
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other,
                                               kCFCompareCaseInsensitive);
}

/* Expands range to whole lines, the way the text system expects: back to the
 * start of the line containing range.location, forward past the terminator
 * that ends the line containing its last character. */
/* The out-parameter forms of the line and paragraph bounds. CFStringGetLineBounds
 * gives all three positions directly. */
- (void)getLineStart:(NSUInteger *)startPtr
                 end:(NSUInteger *)endPtr
         contentsEnd:(NSUInteger *)contentsEndPtr
            forRange:(NSRange)range {
    CFIndex start = 0, end = 0, contentsEnd = 0;

    CFStringGetLineBounds((CFStringRef)self,
        CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
        &start, &end, &contentsEnd);

    if (startPtr != NULL) {
        *startPtr = (NSUInteger)start;
    }
    if (endPtr != NULL) {
        *endPtr = (NSUInteger)end;
    }
    if (contentsEndPtr != NULL) {
        *contentsEndPtr = (NSUInteger)contentsEnd;
    }
}

/* CF has no paragraph-bounds call; for the separators it recognises the two
 * agree on everything except U+2028, matching -paragraphRangeForRange:. */
- (void)getParagraphStart:(NSUInteger *)startPtr
                      end:(NSUInteger *)endPtr
              contentsEnd:(NSUInteger *)contentsEndPtr
                 forRange:(NSRange)range {
    [self getLineStart:startPtr end:endPtr contentsEnd:contentsEndPtr
              forRange:range];
}

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
    PD_REQUIRE_STRING(string);

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

/* Mutable operations live here rather than on NSMutableString: a CFString is
 * bridged to this one class whether or not it is mutable, so methods declared
 * only on NSMutableString are never found at runtime. */
- (void)appendString:(NSString *)string {
    if (string == nil) {
        return;
    }
    CFStringAppend((CFMutableStringRef)self, (CFStringRef)string);
}

- (void)appendFormat:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    _CFStringAppendFormatAndArgumentsAux((CFMutableStringRef)self,
                                         _NSCopyFormattingDescription, NULL,
                                         (CFStringRef)format, arguments);
    va_end(arguments);
}

- (void)setString:(NSString *)string {
    CFStringReplaceAll((CFMutableStringRef)self,
                       (CFStringRef)(string != nil ? string : @""));
}

- (void)insertString:(NSString *)string atIndex:(NSUInteger)index {
    if (string == nil) {
        return;
    }
    CFStringInsert((CFMutableStringRef)self, (CFIndex)index, (CFStringRef)string);
}

- (void)deleteCharactersInRange:(NSRange)range {
    CFStringDelete((CFMutableStringRef)self,
                   CFRangeMake((CFIndex)range.location, (CFIndex)range.length));
}

- (NSUInteger)replaceOccurrencesOfString:(NSString *)target
                              withString:(NSString *)replacement
                                 options:(NSStringCompareOptions)options
                                   range:(NSRange)searchRange {
    if (target == nil || replacement == nil) {
        return 0;
    }
    return (NSUInteger)CFStringFindAndReplace((CFMutableStringRef)self,
        (CFStringRef)target, (CFStringRef)replacement,
        CFRangeMake((CFIndex)searchRange.location, (CFIndex)searchRange.length),
        (CFStringCompareFlags)options);
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
