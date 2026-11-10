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
#include <objc/runtime.h>

/* Bridged onto CFAttributedString, so instances are CF objects: the class
 * factories return CF objects directly and -init* discards the placeholder
 * allocation, the same shape NSArray/NSDictionary use here. */

static inline CFRange NSToCFRange(NSRange range) {
    return CFRangeMake((CFIndex)range.location, (CFIndex)range.length);
}

// CF instances carry NSAttributedString itself as their class, a subclass such as NSTextStorage keeps
// its own storage and only answers the primitives, so CF must not read its memory
static inline BOOL isCFInstance(id object) {
    return object_getClass(object) == [NSAttributedString class];
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
    if (!isCFInstance(self)) {
        NSRange run;
        NSDictionary *attributes = [self attributesAtIndex:index effectiveRange:&run];
        NSUInteger start = run.location, end = NSMaxRange(run);

        // widen across neighbours with equal attributes, inside the limit
        while (start > limit.location) {
            NSRange before;
            if (![[self attributesAtIndex:start - 1 effectiveRange:&before] isEqualToDictionary:attributes])
                break;
            start = before.location;
        }
        while (end < NSMaxRange(limit)) {
            NSRange after;
            if (![[self attributesAtIndex:end effectiveRange:&after] isEqualToDictionary:attributes])
                break;
            end = NSMaxRange(after);
        }
        if (range != NULL)
            *range = NSIntersectionRange(NSMakeRange(start, end - start), limit);
        return attributes;
    }
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
    if (!isCFInstance(self)) {
        NSRange run;
        id value = [self attribute:name atIndex:index effectiveRange:&run];
        NSUInteger start = run.location, end = NSMaxRange(run);

        while (start > limit.location) {
            NSRange before;
            id other = [self attribute:name atIndex:start - 1 effectiveRange:&before];
            if (other != value && ![other isEqual:value])
                break;
            start = before.location;
        }
        while (end < NSMaxRange(limit)) {
            NSRange after;
            id other = [self attribute:name atIndex:end effectiveRange:&after];
            if (other != value && ![other isEqual:value])
                break;
            end = NSMaxRange(after);
        }
        if (range != NULL)
            *range = NSIntersectionRange(NSMakeRange(start, end - start), limit);
        return value;
    }
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
    if (!isCFInstance(self) || !isCFInstance(other)) {
        NSUInteger at = 0, length = [self length];

        if (length != [other length] || ![[self string] isEqualToString:[other string]])
            return NO;
        while (at < length) {
            NSRange mine, theirs;
            if (![[self attributesAtIndex:at effectiveRange:&mine] isEqualToDictionary:[other attributesAtIndex:at effectiveRange:&theirs]])
                return NO;
            at = MAX(at + 1, MIN(NSMaxRange(mine), NSMaxRange(theirs)));
        }
        return YES;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}

- (id)copyWithZone:(NSZone *)zone {
    if (!isCFInstance(self))
        return [[self attributedSubstringFromRange:NSMakeRange(0, [self length])] retain];
    return (id)CFAttributedStringCreateCopy(kCFAllocatorDefault,
                                            (CFAttributedStringRef)self);
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    if (!isCFInstance(self))
        return (id)CFAttributedStringCreateMutableCopy(kCFAllocatorDefault, 0,
            (CFAttributedStringRef)[self attributedSubstringFromRange:NSMakeRange(0, [self length])]);
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

// a subclass gets each run's attributes changed through its own primitives
static void editRuns(NSMutableAttributedString *self, NSRange range, void (^edit)(NSMutableDictionary *attributes)) {
    NSUInteger at = range.location, end = NSMaxRange(range);

    [self beginEditing];
    while (at < end) {
        NSRange run;
        NSMutableDictionary *attributes = [[[self attributesAtIndex:at effectiveRange:&run] mutableCopy] autorelease] ?: [NSMutableDictionary dictionary];
        NSUInteger runEnd = MIN(MAX(NSMaxRange(run), at + 1), end);

        edit(attributes);
        [self setAttributes:attributes range:NSMakeRange(at, runEnd - at)];
        at = runEnd;
    }
    [self endEditing];
}

- (void)addAttribute:(NSAttributedStringKey)name value:(id)value range:(NSRange)range {
    if (!isCFInstance(self)) {
        editRuns(self, range, ^(NSMutableDictionary *attributes) { [attributes setObject:value forKey:name]; });
        return;
    }
    CFAttributedStringSetAttribute((CFMutableAttributedStringRef)self,
                                   NSToCFRange(range), (CFStringRef)name,
                                   (CFTypeRef)value);
}

/* clearOtherAttributes:false is what makes this "add" rather than "set". */
- (void)addAttributes:(NSDictionary *)attributes range:(NSRange)range {
    if (!isCFInstance(self)) {
        editRuns(self, range, ^(NSMutableDictionary *existing) { [existing addEntriesFromDictionary:attributes]; });
        return;
    }
    CFAttributedStringSetAttributes((CFMutableAttributedStringRef)self,
                                    NSToCFRange(range),
                                    (CFDictionaryRef)attributes, false);
}

- (void)removeAttribute:(NSAttributedStringKey)name range:(NSRange)range {
    if (!isCFInstance(self)) {
        editRuns(self, range, ^(NSMutableDictionary *attributes) { [attributes removeObjectForKey:name]; });
        return;
    }
    CFAttributedStringRemoveAttribute((CFMutableAttributedStringRef)self,
                                      NSToCFRange(range), (CFStringRef)name);
}

- (void)appendAttributedString:(NSAttributedString *)string {
    NSUInteger length = [self length];

    [self replaceCharactersInRange:NSMakeRange(length, 0)
              withAttributedString:string];
}

- (void)insertAttributedString:(NSAttributedString *)string atIndex:(NSUInteger)index {
    [self replaceCharactersInRange:NSMakeRange(index, 0) withAttributedString:string];
}

- (void)setAttributedString:(NSAttributedString *)string {
    NSUInteger length = [self length];

    [self replaceCharactersInRange:NSMakeRange(0, length)
              withAttributedString:string];
}

// a subclass that batches its edits overrides these, one that does not has nothing to batch
- (void)beginEditing {
    if (isCFInstance(self))
        CFAttributedStringBeginEditing((CFMutableAttributedStringRef)self);
}

- (void)endEditing {
    if (isCFInstance(self))
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
