/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSScanner.h>
#import <Foundation/NSString.h>
#import <Foundation/NSCharacterSet.h>
#include <stdlib.h>
#include <string.h>

/* Scans over the UTF-8 form of the string. That is exact for the numeric and
 * ASCII-literal scanning AppKit does here; a full implementation would work in
 * unichars against the character sets. */

@implementation NSScanner

+ (instancetype)scannerWithString:(NSString *)string {
    return [[[self alloc] initWithString:string] autorelease];
}

/* No locale-aware number parsing yet: this is the plain scanner. */
+ (instancetype)localizedScannerWithString:(NSString *)string {
    return [self scannerWithString:string];
}

- (instancetype)initWithString:(NSString *)string {
    self = [super init];
    if (self == nil) {
        return nil;
    }
    _string = [string copy];
    _skipSet = [[NSCharacterSet whitespaceAndNewlineCharacterSet] retain];
    return self;
}

- (void)dealloc {
    [_string release];
    [_skipSet release];
    [super dealloc];
}

- (NSString *)string {
    return _string;
}

- (NSUInteger)scanLocation {
    return _location;
}

- (void)setScanLocation:(NSUInteger)location {
    _location = location;
}

- (BOOL)isAtEnd {
    return _location >= [_string length];
}

- (NSCharacterSet *)charactersToBeSkipped {
    return _skipSet;
}

- (void)setCharactersToBeSkipped:(NSCharacterSet *)set {
    set = [set retain];
    [_skipSet release];
    _skipSet = set;
}

- (BOOL)caseSensitive {
    return _caseSensitive;
}

- (void)setCaseSensitive:(BOOL)caseSensitive {
    _caseSensitive = caseSensitive;
}

- (void)_skip {
    NSUInteger length = [_string length];

    while (_location < length) {
        unichar c = [_string characterAtIndex:_location];

        if (_skipSet != nil && ![_skipSet characterIsMember:c]) {
            break;
        }
        if (_skipSet == nil) {
            break;
        }
        _location++;
    }
}

/* Hands the tail of the string to a strtoX-style parser and advances by
 * however much it consumed. */
- (BOOL)_scanNumber:(void *)value kind:(int)kind {
    [self _skip];

    const char *utf8 = [_string UTF8String];
    if (utf8 == NULL) {
        return NO;
    }
    size_t total = strlen(utf8);
    if (_location >= total) {
        return NO;
    }

    const char *start = utf8 + _location;
    char *end = NULL;

    switch (kind) {
        case 0: {
            long v = strtol(start, &end, 10);
            if (end != start && value != NULL) { *(int *)value = (int)v; }
            break;
        }
        case 1: {
            long long v = strtoll(start, &end, 10);
            if (end != start && value != NULL) { *(NSInteger *)value = (NSInteger)v; }
            break;
        }
        case 2: {
            long long v = strtoll(start, &end, 10);
            if (end != start && value != NULL) { *(long long *)value = v; }
            break;
        }
        case 3: {
            unsigned long v = strtoul(start, &end, 16);
            if (end != start && value != NULL) { *(unsigned *)value = (unsigned)v; }
            break;
        }
        case 4: {
            float v = strtof(start, &end);
            if (end != start && value != NULL) { *(float *)value = v; }
            break;
        }
        case 5: {
            double v = strtod(start, &end);
            if (end != start && value != NULL) { *(double *)value = v; }
            break;
        }
        default:
            return NO;
    }

    if (end == start) {
        return NO;
    }
    _location += (NSUInteger)(end - start);
    return YES;
}

- (BOOL)scanInt:(int *)value { return [self _scanNumber:value kind:0]; }
- (BOOL)scanInteger:(NSInteger *)value { return [self _scanNumber:value kind:1]; }
- (BOOL)scanLongLong:(long long *)value { return [self _scanNumber:value kind:2]; }
- (BOOL)scanHexInt:(unsigned *)value { return [self _scanNumber:value kind:3]; }
- (BOOL)scanFloat:(float *)value { return [self _scanNumber:value kind:4]; }
- (BOOL)scanDouble:(double *)value { return [self _scanNumber:value kind:5]; }

- (BOOL)scanString:(NSString *)string intoString:(NSString **)result {
    [self _skip];

    NSUInteger length = [string length];
    NSUInteger available = [_string length];

    if (_location + length > available) {
        return NO;
    }
    for (NSUInteger i = 0; i < length; i++) {
        unichar a = [_string characterAtIndex:_location + i];
        unichar b = [string characterAtIndex:i];

        if (!_caseSensitive) {
            if (a >= 'A' && a <= 'Z') { a = (unichar)(a - 'A' + 'a'); }
            if (b >= 'A' && b <= 'Z') { b = (unichar)(b - 'A' + 'a'); }
        }
        if (a != b) {
            return NO;
        }
    }
    if (result != NULL) {
        *result = string;
    }
    _location += length;
    return YES;
}

- (BOOL)scanCharactersFromSet:(NSCharacterSet *)set intoString:(NSString **)result {
    [self _skip];

    NSUInteger start = _location;
    NSUInteger length = [_string length];

    while (_location < length &&
           [set characterIsMember:[_string characterAtIndex:_location]]) {
        _location++;
    }
    if (_location == start) {
        return NO;
    }
    if (result != NULL) {
        *result = [_string substringWithRange:NSMakeRange(start, _location - start)];
    }
    return YES;
}

- (BOOL)scanUpToString:(NSString *)string intoString:(NSString **)result {
    [self _skip];

    NSUInteger start = _location;
    NSUInteger length = [_string length];
    NSUInteger targetLength = [string length];

    while (_location < length) {
        NSUInteger saved = _location;
        BOOL matched = YES;

        if (_location + targetLength <= length) {
            for (NSUInteger i = 0; i < targetLength; i++) {
                if ([_string characterAtIndex:_location + i] !=
                    [string characterAtIndex:i]) {
                    matched = NO;
                    break;
                }
            }
        } else {
            matched = NO;
        }
        if (matched) {
            _location = saved;
            break;
        }
        _location++;
    }
    if (_location == start) {
        return NO;
    }
    if (result != NULL) {
        *result = [_string substringWithRange:NSMakeRange(start, _location - start)];
    }
    return YES;
}

- (BOOL)scanUpToCharactersFromSet:(NSCharacterSet *)set intoString:(NSString **)result {
    [self _skip];

    NSUInteger start = _location;
    NSUInteger length = [_string length];

    while (_location < length &&
           ![set characterIsMember:[_string characterAtIndex:_location]]) {
        _location++;
    }
    if (_location == start) {
        return NO;
    }
    if (result != NULL) {
        *result = [_string substringWithRange:NSMakeRange(start, _location - start)];
    }
    return YES;
}

- (id)copyWithZone:(NSZone *)zone {
    NSScanner *copy = [[NSScanner alloc] initWithString:_string];

    [copy setScanLocation:_location];
    [copy setCharactersToBeSkipped:_skipSet];
    [copy setCaseSensitive:_caseSensitive];
    return copy;
}

@end
