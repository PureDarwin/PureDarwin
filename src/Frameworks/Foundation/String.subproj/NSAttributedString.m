/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSAttributedString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSException.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSValue.h>
#include <CoreFoundation/CFAttributedString.h>
#include <CoreFoundation/ForFoundationOnly.h>

/* Bridged onto CFAttributedString, so instances are CF objects: the class
 * factories return CF objects directly and -init* discards the placeholder
 * allocation, the same shape NSArray/NSDictionary use here. */

static inline CFRange NSToCFRange(NSRange range) {
    return CFRangeMake((CFIndex)range.location, (CFIndex)range.length);
}

static inline void CFToNSRange(CFRange cf, NSRangePointer out) {
    if (out != NULL) {
        out->location = (NSUInteger)cf.location;
        out->length = (NSUInteger)cf.length;
    }
}

@implementation NSAttributedString

- (instancetype)initWithString:(NSString *)string {
    return [self initWithString:string attributes:nil];
}

- (instancetype)initWithString:(NSString *)string attributes:(NSDictionary *)attributes {
    return (id)CFAttributedStringCreate(kCFAllocatorDefault, (CFStringRef)string,
                                        (CFDictionaryRef)attributes);
}

- (instancetype)initWithAttributedString:(NSAttributedString *)other {
    return (id)CFAttributedStringCreateCopy(kCFAllocatorDefault,
                                            (CFAttributedStringRef)other);
}

- (NSString *)string {
    return (NSString *)CFAttributedStringGetString((CFAttributedStringRef)self);
}

- (NSUInteger)length {
    return (NSUInteger)CFAttributedStringGetLength((CFAttributedStringRef)self);
}

- (NSDictionary *)attributesAtIndex:(NSUInteger)index effectiveRange:(NSRangePointer)range {
    CFRange effective = CFRangeMake(0, 0);
    CFDictionaryRef result = CFAttributedStringGetAttributes(
        (CFAttributedStringRef)self, (CFIndex)index,
        (range != NULL) ? &effective : NULL);

    CFToNSRange(effective, range);
    return (NSDictionary *)result;
}

- (NSDictionary *)attributesAtIndex:(NSUInteger)index
              longestEffectiveRange:(NSRangePointer)range
                            inRange:(NSRange)limit {
    CFRange longest = CFRangeMake(0, 0);
    CFDictionaryRef result = CFAttributedStringGetAttributesAndLongestEffectiveRange(
        (CFAttributedStringRef)self, (CFIndex)index, NSToCFRange(limit),
        (range != NULL) ? &longest : NULL);

    CFToNSRange(longest, range);
    return (NSDictionary *)result;
}

- (id)attribute:(NSAttributedStringKey)name
        atIndex:(NSUInteger)index
 effectiveRange:(NSRangePointer)range {
    return [[self attributesAtIndex:index effectiveRange:range]
        objectForKey:name];
}

- (id)attribute:(NSAttributedStringKey)name
              atIndex:(NSUInteger)index
longestEffectiveRange:(NSRangePointer)range
              inRange:(NSRange)limit {
    CFRange longest = CFRangeMake(0, 0);
    CFTypeRef result = CFAttributedStringGetAttributeAndLongestEffectiveRange(
        (CFAttributedStringRef)self, (CFIndex)index, (CFStringRef)name,
        NSToCFRange(limit), (range != NULL) ? &longest : NULL);

    CFToNSRange(longest, range);
    return (id)result;
}

// built from the primitives, so a subclass such as NSTextStorage works too; CF's own copy would read
// a subclass's memory as its struct (CFAttributedStringCreateWithSubstring has no ObjC dispatch)
- (NSAttributedString *)attributedSubstringFromRange:(NSRange)range {
    CFMutableAttributedStringRef result = CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);
    NSUInteger at = range.location, end = NSMaxRange(range);

    CFAttributedStringReplaceString(result, CFRangeMake(0, 0),
                                    (CFStringRef)[[self string] substringWithRange:range]);
    CFAttributedStringBeginEditing(result);
    while (at < end) {
        NSRange run = NSMakeRange(at, 0);
        NSDictionary *attributes = [self attributesAtIndex:at effectiveRange:&run];
        NSUInteger runEnd = MIN(NSMaxRange(run), end);

        // a zero-length answer would never advance, so take at least one character
        if (runEnd <= at)
            runEnd = at + 1;
        CFAttributedStringSetAttributes(result, CFRangeMake((CFIndex)(at - range.location), (CFIndex)(runEnd - at)),
                                        (CFDictionaryRef)attributes, true);
        at = runEnd;
    }
    CFAttributedStringEndEditing(result);
    return (NSAttributedString *)CFAutorelease(result);
}

- (BOOL)isEqualToAttributedString:(NSAttributedString *)other {
    if (other == nil) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

- (id)copyWithZone:(NSZone *)zone {
    return (id)CFAttributedStringCreateCopy(kCFAllocatorDefault,
                                            (CFAttributedStringRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)CFAttributedStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                   (CFAttributedStringRef)self);
}

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

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (id)initWithCoder:(NSCoder *)coder {
    return [self initWithString:@""];
}

- (NSMutableString *)mutableString {
    return (NSMutableString *)CFAttributedStringGetMutableString(
        (CFMutableAttributedStringRef)self);
}

- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string {
    CFAttributedStringReplaceString((CFMutableAttributedStringRef)self,
                                    NSToCFRange(range), (CFStringRef)string);
}

- (void)replaceCharactersInRange:(NSRange)range
            withAttributedString:(NSAttributedString *)string {
    CFAttributedStringReplaceAttributedString((CFMutableAttributedStringRef)self,
                                              NSToCFRange(range),
                                              (CFAttributedStringRef)string);
}

- (void)deleteCharactersInRange:(NSRange)range {
    [self replaceCharactersInRange:range withString:@""];
}

- (void)setAttributes:(NSDictionary *)attributes range:(NSRange)range {
    CFAttributedStringSetAttributes((CFMutableAttributedStringRef)self,
                                    NSToCFRange(range),
                                    (CFDictionaryRef)attributes, true);
}

- (void)addAttribute:(NSAttributedStringKey)name value:(id)value range:(NSRange)range {
    CFAttributedStringSetAttribute((CFMutableAttributedStringRef)self,
                                   NSToCFRange(range), (CFStringRef)name,
                                   (CFTypeRef)value);
}

/* clearOtherAttributes:false is what makes this "add" rather than "set". */
- (void)addAttributes:(NSDictionary *)attributes range:(NSRange)range {
    CFAttributedStringSetAttributes((CFMutableAttributedStringRef)self,
                                    NSToCFRange(range),
                                    (CFDictionaryRef)attributes, false);
}

- (void)removeAttribute:(NSAttributedStringKey)name range:(NSRange)range {
    CFAttributedStringRemoveAttribute((CFMutableAttributedStringRef)self,
                                      NSToCFRange(range), (CFStringRef)name);
}

- (void)appendAttributedString:(NSAttributedString *)string {
    CFIndex length = CFAttributedStringGetLength((CFAttributedStringRef)self);

    [self replaceCharactersInRange:NSMakeRange((NSUInteger)length, 0)
              withAttributedString:string];
}

- (void)insertAttributedString:(NSAttributedString *)string atIndex:(NSUInteger)index {
    [self replaceCharactersInRange:NSMakeRange(index, 0) withAttributedString:string];
}

- (void)setAttributedString:(NSAttributedString *)string {
    CFIndex length = CFAttributedStringGetLength((CFAttributedStringRef)self);

    [self replaceCharactersInRange:NSMakeRange(0, (NSUInteger)length)
              withAttributedString:string];
}

- (void)beginEditing {
    CFAttributedStringBeginEditing((CFMutableAttributedStringRef)self);
}

- (void)endEditing {
    CFAttributedStringEndEditing((CFMutableAttributedStringRef)self);
}

// collects the runs first so the block may mutate the receiver, reverse walks them backwards
- (void)enumerateAttributesInRange:(NSRange)range
                           options:(NSAttributedStringEnumerationOptions)options
                        usingBlock:(void (^)(NSDictionary *attributes, NSRange range, BOOL *stop))block {
    NSMutableArray *runs = [NSMutableArray array];
    NSMutableArray *values = [NSMutableArray array];
    NSUInteger location = range.location, end = NSMaxRange(range);
    BOOL stop = NO;

    while (location < end) {
        NSRange effective;
        NSDictionary *attributes = [self attributesAtIndex:location longestEffectiveRange:&effective inRange:NSMakeRange(location, end - location)];

        if (effective.length == 0)
            effective = NSMakeRange(location, 1);
        [runs addObject:[NSValue valueWithRange:effective]];
        [values addObject:attributes != nil ? attributes : [NSDictionary dictionary]];
        location = NSMaxRange(effective);
    }
    for (NSUInteger n = 0; n < [runs count] && !stop; n++) {
        NSUInteger k = (options & NSAttributedStringEnumerationReverse) ? [runs count] - 1 - n : n;

        block([values objectAtIndex:k], [[runs objectAtIndex:k] rangeValue], &stop);
    }
}

- (void)enumerateAttribute:(NSString *)name
                   inRange:(NSRange)range
                   options:(NSAttributedStringEnumerationOptions)options
                usingBlock:(void (^)(id value, NSRange range, BOOL *stop))block {
    NSMutableArray *runs = [NSMutableArray array];
    NSMutableArray *values = [NSMutableArray array];
    NSUInteger location = range.location, end = NSMaxRange(range);
    BOOL stop = NO;

    while (location < end) {
        NSRange effective;
        id value = [self attribute:name atIndex:location longestEffectiveRange:&effective inRange:NSMakeRange(location, end - location)];

        if (effective.length == 0)
            effective = NSMakeRange(location, 1);
        [runs addObject:[NSValue valueWithRange:effective]];
        [values addObject:value != nil ? value : [NSNull null]];
        location = NSMaxRange(effective);
    }
    for (NSUInteger n = 0; n < [runs count] && !stop; n++) {
        NSUInteger k = (options & NSAttributedStringEnumerationReverse) ? [runs count] - 1 - n : n;
        id value = [values objectAtIndex:k];

        block(value == [NSNull null] ? nil : value, [[runs objectAtIndex:k] rangeValue], &stop);
    }
}

@end

@implementation NSMutableAttributedString

- (instancetype)initWithString:(NSString *)string attributes:(NSDictionary *)attributes {
    CFMutableAttributedStringRef result =
        CFAttributedStringCreateMutable(kCFAllocatorDefault, 0);

    if (string != nil) {
        CFAttributedStringReplaceString(result, CFRangeMake(0, 0), (CFStringRef)string);
    }
    if (attributes != nil && string != nil) {
        CFAttributedStringSetAttributes(result,
            CFRangeMake(0, CFAttributedStringGetLength(result)),
            (CFDictionaryRef)attributes, true);
    }
    return (id)result;
}

- (instancetype)initWithAttributedString:(NSAttributedString *)other {
    return (id)CFAttributedStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                   (CFAttributedStringRef)other);
}

@end

#if DEPLOYMENT_RUNTIME_OBJC
__attribute__((constructor))
static void __NSCFAttributedStringBridgeInit(void) {
    _CFRuntimeBridgeClasses(CFAttributedStringGetTypeID(), "NSAttributedString");
}
#endif
