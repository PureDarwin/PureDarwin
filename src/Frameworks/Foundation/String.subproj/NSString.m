/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSString.h>
#import <Foundation/NSException.h>
#import <Foundation/NSArray.h>
#include <CoreFoundation/CFBase.h>
#import <Foundation/NSData.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFCharacterSet.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFURL.h>
#include <objc/runtime.h>
#include <stdarg.h>
#include <stdlib.h>

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

extern Boolean _CFIsObjC(CFTypeID typeID, void *obj);

// a subclass that is not a CFString (Swift's string storage): the abstract methods work through its primitives
// the cluster's own classes are not: an +alloc'd NSMutableString is a placeholder whose -init returns a CFString
static inline BOOL __NSStringIsForeign(id string) {
    Class cls = object_getClass(string);

    if (cls == [NSString class] || cls == [NSMutableString class])
        return NO;
    return _CFIsObjC(CFStringGetTypeID(), (void *)string) ? YES : NO;
}

static void __NSStringAbstract(id self, SEL _cmd) {
    [NSException raise:NSInvalidArgumentException
                format:@"*** -[%s %s]: method only defined for abstract class", object_getClassName(self), sel_getName(_cmd)];
}

// a CFString holding the characters of any NSString, read through -length and -getCharacters:range:
__attribute__((visibility("hidden"))) CFMutableStringRef __NSStringCreateCFCopy(NSString *string) {
    NSUInteger length = [string length];
    UniChar *buffer = malloc(length ? length * sizeof(UniChar) : 1);
    CFMutableStringRef copy = CFStringCreateMutable(kCFAllocatorDefault, 0);

    [string getCharacters:(unichar *)buffer range:NSMakeRange(0, length)];
    CFStringAppendCharacters(copy, buffer, (CFIndex)length);
    free(buffer);
    return copy;
}

@implementation NSString

- (NSUInteger)length {
    __NSStringAbstract(self, _cmd);
    return 0;
}

- (unichar)characterAtIndex:(NSUInteger)index {
    __NSStringAbstract(self, _cmd);
    return 0;
}

- (void)getCharacters:(unichar *)buffer range:(NSRange)range {
    for (NSUInteger i = 0; i < range.length; i++) {
        buffer[i] = [self characterAtIndex:range.location + i];
    }
}

- (CFTypeID)_cfTypeID {
    return CFStringGetTypeID();
}

// NSCFString overrides both with CF's own copies
- (id)copyWithZone:(NSZone *)zone {
    CFMutableStringRef copy = __NSStringCreateCFCopy(self);
    CFStringRef result = CFStringCreateCopy(kCFAllocatorDefault, copy);

    CFRelease(copy);
    return (id)result;
}

- (id)mutableCopyWithZone:(NSZone *)zone {
    return (id)__NSStringCreateCFCopy(self);
}

// what CF sends a string that is not a CFString, built on the primitives
- (id)_createSubstringWithRange:(NSRange)range {
    UniChar *buffer = malloc(range.length ? range.length * sizeof(UniChar) : 1);
    CFStringRef result;

    [self getCharacters:(unichar *)buffer range:range];
    result = CFStringCreateWithCharacters(kCFAllocatorDefault, buffer, (CFIndex)range.length);
    free(buffer);
    return (id)result;
}

- (const char *)_fastCStringContents:(BOOL)nullTerminationRequired {
    return NULL;
}

- (const unichar *)_fastCharacterContents {
    return NULL;
}

- (BOOL)_encodingCantBeStoredInEightBitCFString {
    NSUInteger length = [self length];

    for (NSUInteger i = 0; i < length; i++) {
        if ([self characterAtIndex:i] > 0x7f)
            return YES;
    }
    return NO;
}

- (CFStringEncoding)_fastestEncodingInCFStringEncoding {
    return kCFStringEncodingUnicode;
}

- (CFStringEncoding)_smallestEncodingInCFStringEncoding {
    return [self _encodingCantBeStoredInEightBitCFString] ? kCFStringEncodingUnicode : kCFStringEncodingASCII;
}

- (BOOL)_getCString:(char *)buffer maxLength:(NSUInteger)maxLength encoding:(CFStringEncoding)encoding {
    CFMutableStringRef copy = __NSStringCreateCFCopy(self);
    Boolean ok = CFStringGetCString(copy, buffer, (CFIndex)maxLength + 1, encoding);

    CFRelease(copy);
    return ok ? YES : NO;
}

/* Derived from the -getCharacters:range: primitive, so every subclass gets it.
   The key-event path in -[NSResponder interpretKeyEvents:] uses this form, and
   without it any keystroke reaching a text view aborts. No NUL is written. */
- (void)getCharacters:(unichar *)buffer {
    [self getCharacters:buffer range:NSMakeRange(0, [self length])];
}

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

- (NSString *)stringByAppendingFormat:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    CFStringRef formatted = _NSStringCreateWithFormatAndArguments(format, args);
    va_end(args);

    NSString *result = [self stringByAppendingString:(NSString *)formatted];
    CFRelease(formatted);
    return result;
}

+ (instancetype)stringWithContentsOfFile:(NSString *)path
                                  encoding:(NSStringEncoding)encoding
                                     error:(NSError **)error {
    return [[[self alloc] initWithContentsOfFile:path
                                        encoding:encoding
                                           error:error] autorelease];
}

/* The deprecated loaders, still used by plenty of working code. They have no
 * encoding argument, so guess: UTF-8 if the bytes decode, Latin-1 otherwise -
 * which always decodes, so a file is never unreadable here. */
+ (instancetype)stringWithContentsOfFile:(NSString *)path {
    return [[[self alloc] initWithContentsOfFile:path] autorelease];
}

- (instancetype)initWithContentsOfFile:(NSString *)path {
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (data == nil) {
        [self release];
        return nil;
    }
    CFStringRef result = CFStringCreateWithBytes(kCFAllocatorDefault,
                                                 (const UInt8 *)[data bytes],
                                                 (CFIndex)[data length],
                                                 kCFStringEncodingUTF8, false);
    if (result == NULL) {
        result = CFStringCreateWithBytes(kCFAllocatorDefault,
                                         (const UInt8 *)[data bytes],
                                         (CFIndex)[data length],
                                         kCFStringEncodingISOLatin1, false);
    }
    [self release];
    return (id)result;
}

/* Also on NSString, not only NSCFString: code caches IMPs with
 * +[NSString instanceMethodForSelector:] and invokes them on real instances,
 * which finds the forwarding stub if the method lives solely on the bridged
 * subclass. Every instance is a CFString, so calling CF here is valid. */
- (NSComparisonResult)compare:(NSString *)other {
    if (other == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[NSString compare:]: nil argument"];
        return NSOrderedSame;
    }
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other, 0);
}

- (NSComparisonResult)caseInsensitiveCompare:(NSString *)other {
    if (other == nil) {
        [NSException raise:NSInvalidArgumentException
                    format:@"-[NSString caseInsensitiveCompare:]: nil argument"];
        return NSOrderedSame;
    }
    return (NSComparisonResult)CFStringCompare((CFStringRef)self,
                                               (CFStringRef)other,
                                               kCFCompareCaseInsensitive);
}

- (instancetype)init {
    // a subclass outside the cluster gets a real instance of itself
    if (__NSStringIsForeign(self))
        return [super init];
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

// formats are not localized here, so a locale formats like none
- (instancetype)initWithFormat:(NSString *)format locale:(id)locale arguments:(va_list)arguments {
    return [self initWithFormat:format arguments:arguments];
}

- (instancetype)initWithFormat:(NSString *)format locale:(id)locale, ... {
    va_list arguments;

    va_start(arguments, locale);
    id result = [self initWithFormat:format arguments:arguments];
    va_end(arguments);
    return result;
}

+ (instancetype)localizedStringWithFormat:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    id result = [[self alloc] initWithFormat:format arguments:arguments];
    va_end(arguments);
    return [result autorelease];
}

- (instancetype)initWithCharacters:(const unichar *)characters
                            length:(NSUInteger)length {
    return (id)CFStringCreateWithCharacters(kCFAllocatorDefault,
                                            (const UniChar *)characters,
                                            (CFIndex)length);
}

// copied, so the buffer can be freed at once
- (instancetype)initWithCharactersNoCopy:(unichar *)characters length:(NSUInteger)length freeWhenDone:(BOOL)freeBuffer {
    id result = [self initWithCharacters:characters length:length];

    if (freeBuffer)
        free(characters);
    return result;
}

- (NSString *)stringByRemovingPercentEncoding {
    CFStringRef result = CFURLCreateStringByReplacingPercentEscapes(kCFAllocatorDefault, (CFStringRef)self, CFSTR(""));

    return result ? (NSString *)CFAutorelease(result) : nil;
}

- (NSString *)stringByReplacingPercentEscapesUsingEncoding:(NSStringEncoding)encoding {
    CFStringRef result = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault, (CFStringRef)self,
                                                                                  CFSTR(""), __NSStringCFEncoding(encoding));

    return result ? (NSString *)CFAutorelease(result) : nil;
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

- (instancetype)initWithData:(NSData *)data encoding:(NSStringEncoding)encoding {
    if (data == nil) {
        [self release];
        return nil;
    }
    return [self initWithBytes:[data bytes]
                        length:[data length]
                      encoding:encoding];
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

+ (nullable instancetype)stringWithContentsOfURL:(NSURL *)url
                                         encoding:(NSStringEncoding)encoding
                                            error:(NSError **)error {
    return [[[self alloc] initWithContentsOfURL:url encoding:encoding error:error] autorelease];
}

// file URLs only, like the path reader
- (nullable instancetype)initWithContentsOfURL:(NSURL *)url
                                       encoding:(NSStringEncoding)encoding
                                          error:(NSError **)error {
    CFStringRef path = url ? CFURLCopyFileSystemPath((CFURLRef)url, kCFURLPOSIXPathStyle) : NULL;
    id result;

    if (path == NULL) {
        if (error != NULL) {
            *error = nil;
        }
        [self release];
        return nil;
    }
    result = [self initWithContentsOfFile:(NSString *)path encoding:encoding error:error];
    CFRelease(path);
    return result;
}

- (BOOL)isEqualToString:(NSString *)other {
    if (other == nil) {
        return NO;
    }
    if (__NSStringIsForeign(self) || __NSStringIsForeign(other))
        return CFStringCompare((CFStringRef)self, (CFStringRef)other, 0) == kCFCompareEqualTo;
    return CFEqual((CFStringRef)self, (CFStringRef)other) ? YES : NO;
}

- (const char *)cString {
    return [self UTF8String];
}

// a lossy conversion writes '?' for characters the encoding lacks
- (NSData *)dataUsingEncoding:(NSStringEncoding)encoding allowLossyConversion:(BOOL)lossy {
    if (!lossy)
        return [self dataUsingEncoding:encoding];

    CFStringRef string = (CFStringRef)self;
    CFDataRef result;

    if (__NSStringIsForeign(self))
        string = (CFStringRef)CFAutorelease(__NSStringCreateCFCopy(self));
    result = CFStringCreateExternalRepresentation(kCFAllocatorDefault, string, __NSStringCFEncoding(encoding), '?');
    return result ? (NSData *)CFAutorelease(result) : nil;
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
    // hashed as the CFString with the same characters, so equal strings hash alike
    if (__NSStringIsForeign(self)) {
        CFMutableStringRef copy = __NSStringCreateCFCopy(self);
        NSUInteger hash = (NSUInteger)CFHash(copy);

        CFRelease(copy);
        return hash;
    }
    return (NSUInteger)CFHash((CFTypeRef)self);
}

- (BOOL)isEqual:(id)other {
    if (self == other) {
        return YES;
    }
    if (other == nil || ![other isKindOfClass:[NSString class]]) {
        return NO;
    }
    return [self isEqualToString:other];
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

/* The NSStringCompareOptions bits line up with CFStringCompareFlags, with one
 * exception: NSLiteralSearch has no CF bit because CF compares literally by
 * default (kCFCompareNonliteral opts out), so it maps to no flag at all. */
static CFOptionFlags __NSStringCFCompareFlags(NSStringCompareOptions options) {
    return (CFOptionFlags)(options & ~(NSStringCompareOptions)NSLiteralSearch);
}

- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement {
    return [self stringByReplacingOccurrencesOfString:target
                                           withString:replacement
                                              options:0
                                                range:NSMakeRange(0, [self length])];
}

- (NSString *)stringByReplacingCharactersInRange:(NSRange)range
                                      withString:(NSString *)replacement {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringReplace(copy, CFRangeMake((CFIndex)range.location,
                                      (CFIndex)range.length),
                    (CFStringRef)(replacement != nil ? replacement : @""));
    return (id)CFAutorelease(copy);
}

- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)target
                                        withString:(NSString *)replacement
                                           options:(NSStringCompareOptions)options
                                             range:(NSRange)range {
    CFMutableStringRef copy = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                        (CFStringRef)self);

    CFStringFindAndReplace(copy, (CFStringRef)target, (CFStringRef)replacement,
                           CFRangeMake((CFIndex)range.location,
                                       (CFIndex)range.length),
                           __NSStringCFCompareFlags(options));
    return (id)CFAutorelease(copy);
}


+ (instancetype)stringWithCString:(const char *)cString encoding:(NSStringEncoding)encoding {
    if (cString == NULL) {
        return nil;
    }

    CFStringRef string = CFStringCreateWithCString(kCFAllocatorDefault, cString,
                                                   __NSStringCFEncoding(encoding));

    // a byte sequence the encoding rejects is nil, not a NULL handed to CFAutorelease
    if (string == NULL) {
        return nil;
    }

    return (id)CFAutorelease(string);
}

+ (instancetype)stringWithCString:(const char *)cString {
    return [self stringWithCString:cString encoding:NSUTF8StringEncoding];
}

- (const char *)cStringUsingEncoding:(NSStringEncoding)encoding {
    return CFStringGetCStringPtr((CFStringRef)self, __NSStringCFEncoding(encoding));
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

+ (instancetype)stringWithFormat:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    CFStringRef formatted = _NSStringCreateWithFormatAndArguments(format, arguments);
    va_end(arguments);

    CFMutableStringRef result = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                           formatted);
    CFRelease(formatted);
    return (id)CFAutorelease(result);
}

+ (instancetype)stringWithCapacity:(NSUInteger)capacity {
    (void)capacity;
    CFMutableStringRef result = CFStringCreateMutable(kCFAllocatorDefault, 0);
    return (id)CFAutorelease(result);
}

- (instancetype)initWithFormat:(NSString *)format, ... {
    va_list arguments;

    va_start(arguments, format);
    CFStringRef formatted = _NSStringCreateWithFormatAndArguments(format, arguments);
    va_end(arguments);

    CFMutableStringRef result = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                           formatted);
    CFRelease(formatted);
    return (id)result;
}

- (instancetype)initWithFormat:(NSString *)format arguments:(va_list)arguments {
    CFStringRef formatted = _NSStringCreateWithFormatAndArguments(format, arguments);
    CFMutableStringRef result = CFStringCreateMutableCopy(kCFAllocatorDefault, 0,
                                                           formatted);
    CFRelease(formatted);
    return (id)result;
}

- (void)appendString:(NSString *)string {
    if (__NSStringIsForeign(self)) {
        [self replaceCharactersInRange:NSMakeRange([self length], 0) withString:string];
        return;
    }
    CFStringAppend((CFMutableStringRef)self, (CFStringRef)string);
}

- (void)appendFormat:(NSString *)format, ... {
    va_list args;
    va_start(args, format);
    CFStringRef formatted = _NSStringCreateWithFormatAndArguments(format, args);
    va_end(args);

    if (formatted != NULL) {
        CFStringAppend((CFMutableStringRef)self, formatted);
        CFRelease(formatted);
    }
}

- (void)setString:(NSString *)string {
    if (__NSStringIsForeign(self)) {
        [self replaceCharactersInRange:NSMakeRange(0, [self length]) withString:string];
        return;
    }
    CFStringReplaceAll((CFMutableStringRef)self, (CFStringRef)string);
}

// the primitive, NSCFString overrides it
- (void)replaceCharactersInRange:(NSRange)range withString:(NSString *)string {
    __NSStringAbstract(self, _cmd);
}

- (void)insertString:(NSString *)string atIndex:(NSUInteger)index {
    [self replaceCharactersInRange:NSMakeRange(index, 0) withString:string];
}

- (void)deleteCharactersInRange:(NSRange)range {
    [self replaceCharactersInRange:range withString:@""];
}

- (void)appendCharacters:(const unichar *)characters length:(NSUInteger)length {
    CFStringRef string = CFStringCreateWithCharacters(kCFAllocatorDefault, (const UniChar *)characters, (CFIndex)length);

    [self appendString:(NSString *)string];
    CFRelease(string);
}

// CF's in-place edits on a string that is not a CFString: edit a CF copy, then put it back through -setString:
static void __NSMutableStringEdit(NSMutableString *self, void (^edit)(CFMutableStringRef copy)) {
    CFMutableStringRef copy = __NSStringCreateCFCopy(self);

    edit(copy);
    [self setString:(NSString *)copy];
    CFRelease(copy);
}

- (NSUInteger)replaceOccurrencesOfString:(NSString *)target
                              withString:(NSString *)replacement
                                 options:(NSStringCompareOptions)options
                                   range:(NSRange)range {
    __block CFIndex replaced = 0;

    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) {
        replaced = CFStringFindAndReplace(copy, (CFStringRef)target, (CFStringRef)replacement,
                                          CFRangeMake((CFIndex)range.location, (CFIndex)range.length),
                                          (CFStringCompareFlags)options);
    });
    return (NSUInteger)replaced;
}

- (void)_cfAppendCString:(const unsigned char *)cString length:(NSInteger)length {
    CFStringRef string = CFStringCreateWithBytes(kCFAllocatorDefault, cString, (CFIndex)length,
                                                 kCFStringEncodingASCII, false);

    [self appendString:(NSString *)string];
    CFRelease(string);
}

- (void)_cfPad:(CFStringRef)padString length:(uint32_t)length padIndex:(uint32_t)padIndex {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringPad(copy, padString, length, padIndex); });
}

- (void)_cfTrim:(CFStringRef)trimString {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringTrim(copy, trimString); });
}

- (void)_cfTrimWS {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringTrimWhitespace(copy); });
}

- (void)_cfLowercase:(const void *)locale {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringLowercase(copy, (CFLocaleRef)locale); });
}

- (void)_cfUppercase:(const void *)locale {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringUppercase(copy, (CFLocaleRef)locale); });
}

- (void)_cfCapitalize:(const void *)locale {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringCapitalize(copy, (CFLocaleRef)locale); });
}

- (void)_cfNormalize:(CFStringNormalizationForm)form {
    __NSMutableStringEdit(self, ^(CFMutableStringRef copy) { CFStringNormalize(copy, form); });
}

@end
