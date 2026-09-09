/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFBase.h>
#import <Foundation/NSData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFCharacterSet.h>
#include <CoreFoundation/CFData.h>
#include <objc/runtime.h>
#include <stdarg.h>

extern CFStringRef _CFStringCreateWithFormatAndArgumentsAux(
    CFAllocatorRef allocator,
    CFStringRef (*copyDescription)(void *, const void *),
    CFDictionaryRef options, CFStringRef format, va_list arguments);
extern void _CFStringAppendFormatAndArgumentsAux(
    CFMutableStringRef output,
    CFStringRef (*copyDescription)(void *, const void *),
    CFDictionaryRef options, CFStringRef format, va_list arguments);

CFStringRef _NSCopyFormattingDescription(void *value, const void *locale) {
    id object = (id)value;
    Class objectClass = object_getClass(object);

    if (objectClass != Nil && class_isMetaClass(objectClass)) {
        return CFStringCreateWithCString(kCFAllocatorDefault,
                                         class_getName((Class)object),
                                         kCFStringEncodingUTF8);
    }

    NSString *description;
    if ([object respondsToSelector:@selector(descriptionWithLocale:)]) {
        description = [object descriptionWithLocale:(id)locale];
    } else {
        description = [object description];
    }

    return description ? CFRetain((CFStringRef)description) : NULL;
}

CFStringRef _NSStringCreateWithFormatAndArguments(NSString *format,
                                                   va_list arguments) {
    return _CFStringCreateWithFormatAndArgumentsAux(
        kCFAllocatorDefault, _NSCopyFormattingDescription, NULL,
        (CFStringRef)format, arguments);
}

/* NSStringEncoding and CFStringEncoding are separate numbering schemes; only
 * the two encodings NSString.h declares are mapped. */
static CFStringEncoding
__NSStringCFEncoding(NSStringEncoding encoding)
{
    return (encoding == NSUTF8StringEncoding) ? kCFStringEncodingUTF8
                                              : kCFStringEncodingASCII;
}

@implementation NSString

+ (instancetype)stringWithUTF8String:(const char *)utf8String {
    return [[self alloc] initWithUTF8String:utf8String];
}

+ (instancetype)stringWithFormat:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    CFStringRef result = _NSStringCreateWithFormatAndArguments(format, args);
    va_end(args);
    return (id)result;
}

+ (instancetype)stringWithContentsOfFile:(NSString *)path
                                  encoding:(NSStringEncoding)encoding
                                     error:(NSError **)error {
    return [[[self alloc] initWithContentsOfFile:path
                                        encoding:encoding
                                           error:error] autorelease];
}

- (instancetype)init {
    return (id)CFStringCreateWithCString(kCFAllocatorDefault, "",
                                          kCFStringEncodingUTF8);
}

- (instancetype)initWithString:(NSString *)string {
    if (string == nil) {
        [self release];
        return nil;
    }
    return (id)CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)string);
}

- (instancetype)initWithFormat:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    CFStringRef result = _NSStringCreateWithFormatAndArguments(format, arguments);
    va_end(arguments);

    return (id)result;
}

- (instancetype)initWithFormat:(NSString *)format arguments:(va_list)arguments {
    return (id)_NSStringCreateWithFormatAndArguments(format, arguments);
}

- (instancetype)initWithCharacters:(const unichar *)characters
                            length:(NSUInteger)length {
    return (id)CFStringCreateWithCharacters(kCFAllocatorDefault,
                                            (const UniChar *)characters,
                                            (CFIndex)length);
}

- (instancetype)initWithCString:(const char *)cString
                       encoding:(NSStringEncoding)encoding {
    if (cString == NULL) {
        [self release];
        return nil;
    }
    return (id)CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                         __NSStringCFEncoding(encoding));
}

- (instancetype)initWithUTF8String:(const char *)utf8String {
    CFStringRef result = CFStringCreateWithCString(kCFAllocatorDefault, utf8String, kCFStringEncodingUTF8);
    return (id)result;
}

- (instancetype)initWithBytes:(const void *)bytes
                       length:(NSUInteger)length
                     encoding:(NSStringEncoding)encoding {
    /* Returns nil when the bytes are not valid in the encoding, which callers
     * decoding untrusted input rely on to detect a malformed string. */
    CFStringRef result = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                 (const UInt8 *)bytes,
                                                 (CFIndex)length,
                                                 __NSStringCFEncoding(encoding),
                                                 false);
    return (id)result;
}

- (instancetype)initWithContentsOfFile:(NSString *)path
                                encoding:(NSStringEncoding)encoding
                                   error:(NSError **)error {
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data == nil) {
        if (error != NULL) {
            *error = nil;
        }
        [self release];
        return nil;
    }
    CFStringRef result = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                 (const UInt8 *)[data bytes],
                                                 (CFIndex)[data length],
                                                 __NSStringCFEncoding(encoding),
                                                 false);
    [self release];
    return (id)result;
}

- (BOOL)isEqualToString:(NSString *)other {
    if (other == nil) {
        return NO;
    }
    return CFEqual((CFStringRef)self, (CFStringRef)other) ? YES : NO;
}

- (const char *)cString {
    return [self UTF8String];
}

- (NSData *)dataUsingEncoding:(NSStringEncoding)encoding {
    CFDataRef result = CFStringCreateExternalRepresentation(kCFAllocatorDefault,
                                                            (CFStringRef)self,
                                                            __NSStringCFEncoding(encoding),
                                                            0);
    if (result == NULL) {
        return nil;
    }
    return (NSData *)CFAutorelease(result);
}

/* Bridged to CFStringRef, so identity comes from the CF layer. Without these the
 * NSObject versions apply and compare pointers, which makes any dictionary or
 * set keyed by value fail to find an equal-but-distinct object. */
- (NSUInteger)hash {
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSString class]]) {
        return NO;
    }
    return CFEqual((CFTypeRef)self, (CFTypeRef)other) ? YES : NO;
}


+ (instancetype)stringWithCharacters:(const unichar *)characters length:(NSUInteger)length {
    CFStringRef string = CFStringCreateWithCharacters(kCFAllocatorDefault,
                                                      (const UniChar *)characters,
                                                      (CFIndex)length);

    return (id)CFAutorelease(string);
}


+ (instancetype)stringWithString:(NSString *)string {
    if (string == nil) {
        return nil;
    }
    CFStringRef copy = CFStringCreateCopy(kCFAllocatorDefault, (CFStringRef)string);

    return (id)CFAutorelease(copy);
}


+ (instancetype)string {
    return [self stringWithUTF8String:""];
}

- (NSRange)rangeOfCharacterFromSet:(NSCharacterSet *)set {
    CFRange found;

    if (CFStringFindCharacterFromSet((CFStringRef)self, (CFCharacterSetRef)set,
                                     CFRangeMake(0, CFStringGetLength((CFStringRef)self)),
                                     0, &found)) {
        return NSMakeRange((NSUInteger)found.location, (NSUInteger)found.length);
    }
    return NSMakeRange(NSNotFound, 0);
}

- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically {
    return [[self dataUsingEncoding:NSUTF8StringEncoding] writeToFile:path
                                                           atomically:atomically];
}


- (NSArray *)componentsSeparatedByString:(NSString *)separator {
    NSMutableArray *parts = [NSMutableArray array];
    NSUInteger length = [self length];
    NSUInteger start = 0;

    if ([separator length] == 0) {
        return [NSArray arrayWithObject:self];
    }
    while (start <= length) {
        NSRange search = NSMakeRange(start, length - start);
        NSRange found = [self rangeOfString:separator options:0 range:search];

        if (found.location == NSNotFound) {
            [parts addObject:[self substringFromIndex:start]];
            break;
        }
        [parts addObject:[self substringWithRange:
            NSMakeRange(start, found.location - start)]];
        start = found.location + found.length;
    }
    return parts;
}

- (NSArray *)componentsSeparatedByCharactersInSet:(NSCharacterSet *)set {
    NSMutableArray *parts = [NSMutableArray array];
    NSUInteger length = [self length];
    NSUInteger start = 0;

    for (NSUInteger i = 0; i < length; i++) {
        if (CFCharacterSetIsCharacterMember((CFCharacterSetRef)set,
                                            CFStringGetCharacterAtIndex((CFStringRef)self,
                                                                        (CFIndex)i))) {
            [parts addObject:[self substringWithRange:NSMakeRange(start, i - start)]];
            start = i + 1;
        }
    }
    [parts addObject:[self substringFromIndex:start]];
    return parts;
}

- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringFindAndReplace(copy, (CFStringRef)target, (CFStringRef)replacement,
                           CFRangeMake(0, CFStringGetLength(copy)), 0);
    return (id)CFAutorelease(copy);
}


+ (instancetype)stringWithCString:(const char *)cString encoding:(NSStringEncoding)encoding {
    if (cString == NULL) {
        return nil;
    }

    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                   (CFStringEncoding)encoding);

    return (id)CFAutorelease(string);
}

+ (instancetype)stringWithCString:(const char *)cString {
    return [self stringWithCString:cString encoding:NSUTF8StringEncoding];
}

- (const char *)cStringUsingEncoding:(NSStringEncoding)encoding {
    return CFStringGetCStringPtr((CFStringRef)self, (CFStringEncoding)encoding);
}


+ (NSStringEncoding)defaultCStringEncoding {
    return NSUTF8StringEncoding;
}

@end

/* CFStringCreateMutable takes maxLength, not a capacity hint: a non-zero value
 * is a hard limit on the string's length. NSMutableString has no such limit, so
 * the requested capacity is only a hint and 0 is passed through. */
@implementation NSMutableString

+ (instancetype)string {
    return [self stringWithCapacity:0];
}

+ (instancetype)stringWithCapacity:(NSUInteger)capacity {
    (void)capacity;
    CFMutableStringRef result = CFStringCreateMutable(kCFAllocatorDefault, 0);
    return (id)CFAutorelease(result);
}

- (void)appendString:(NSString *)string {
    CFStringAppend((CFMutableStringRef)self, (CFStringRef)string);
}

- (void)appendFormat:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    _CFStringAppendFormatAndArgumentsAux((CFMutableStringRef)self,
                                         _NSCopyFormattingDescription, NULL,
                                         (CFStringRef)format, args);
    va_end(args);
}

- (void)setString:(NSString *)string {
    CFStringReplaceAll((CFMutableStringRef)self, (CFStringRef)string);
}

@end
