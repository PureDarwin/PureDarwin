/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSString.h>
#import <Foundation/NSData.h>
#include <CoreFoundation/CFString.h>
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
