/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* A direct recursive-descent JSON reader and writer over the Foundation
 * containers. UTF-8 only, which is the encoding JSON is defined in. */

#import <Foundation/NSJSONSerialization.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSData.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSNull.h>
#import <Foundation/NSError.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ---- reading ------------------------------------------------------------ */

typedef struct {
    const char *bytes;
    size_t length;
    size_t position;
    BOOL failed;
} JSONReader;

static id _parseValue(JSONReader *reader);

static void _skipWhitespace(JSONReader *reader) {
    while (reader->position < reader->length) {
        char c = reader->bytes[reader->position];

        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        reader->position++;
    }
}

static NSString *_parseString(JSONReader *reader) {
    if (reader->position >= reader->length || reader->bytes[reader->position] != '"') {
        reader->failed = YES;
        return nil;
    }
    reader->position++;

    NSMutableData *out = [NSMutableData data];

    while (reader->position < reader->length) {
        char c = reader->bytes[reader->position++];

        if (c == '"') {
            return [[[NSString alloc] initWithData:out
                                          encoding:NSUTF8StringEncoding] autorelease];
        }
        if (c != '\\') {
            [out appendBytes:&c length:1];
            continue;
        }
        if (reader->position >= reader->length) {
            break;
        }

        char escape = reader->bytes[reader->position++];
        char decoded = 0;

        switch (escape) {
            case '"':  decoded = '"';  break;
            case '\\': decoded = '\\'; break;
            case '/':  decoded = '/';  break;
            case 'b':  decoded = '\b'; break;
            case 'f':  decoded = '\f'; break;
            case 'n':  decoded = '\n'; break;
            case 'r':  decoded = '\r'; break;
            case 't':  decoded = '\t'; break;
            case 'u': {
                if (reader->position + 4 > reader->length) {
                    reader->failed = YES;
                    return nil;
                }

                char digits[5] = {0};

                memcpy(digits, reader->bytes + reader->position, 4);
                reader->position += 4;

                unsigned long code = strtoul(digits, NULL, 16);
                unichar unit = (unichar)code;
                NSString *piece = [NSString stringWithCharacters:&unit length:1];
                NSData *utf8 = [piece dataUsingEncoding:NSUTF8StringEncoding];

                [out appendData:utf8];
                continue;
            }
            default:
                reader->failed = YES;
                return nil;
        }
        [out appendBytes:&decoded length:1];
    }
    reader->failed = YES;
    return nil;
}

static id _parseNumber(JSONReader *reader) {
    size_t start = reader->position;
    BOOL isReal = NO;

    while (reader->position < reader->length) {
        char c = reader->bytes[reader->position];

        if (c == '-' || c == '+' || (c >= '0' && c <= '9')) {
            reader->position++;
        } else if (c == '.' || c == 'e' || c == 'E') {
            isReal = YES;
            reader->position++;
        } else {
            break;
        }
    }
    if (reader->position == start) {
        reader->failed = YES;
        return nil;
    }

    char text[64];
    size_t length = reader->position - start;

    if (length >= sizeof(text)) {
        reader->failed = YES;
        return nil;
    }
    memcpy(text, reader->bytes + start, length);
    text[length] = '\0';

    return isReal ? [NSNumber numberWithDouble:strtod(text, NULL)]
                  : [NSNumber numberWithLongLong:strtoll(text, NULL, 10)];
}

static id _parseArray(JSONReader *reader) {
    reader->position++;   /* '[' */

    NSMutableArray *array = [NSMutableArray array];

    _skipWhitespace(reader);
    if (reader->position < reader->length && reader->bytes[reader->position] == ']') {
        reader->position++;
        return array;
    }

    for (;;) {
        id value = _parseValue(reader);

        if (reader->failed) {
            return nil;
        }
        [array addObject:value];

        _skipWhitespace(reader);
        if (reader->position >= reader->length) {
            break;
        }

        char c = reader->bytes[reader->position++];

        if (c == ']') {
            return array;
        }
        if (c != ',') {
            break;
        }
    }
    reader->failed = YES;
    return nil;
}

static id _parseObject(JSONReader *reader) {
    reader->position++;   /* '{' */

    NSMutableDictionary *object = [NSMutableDictionary dictionary];

    _skipWhitespace(reader);
    if (reader->position < reader->length && reader->bytes[reader->position] == '}') {
        reader->position++;
        return object;
    }

    for (;;) {
        _skipWhitespace(reader);

        NSString *key = _parseString(reader);

        if (reader->failed || key == nil) {
            return nil;
        }

        _skipWhitespace(reader);
        if (reader->position >= reader->length || reader->bytes[reader->position++] != ':') {
            break;
        }

        id value = _parseValue(reader);

        if (reader->failed) {
            return nil;
        }
        [object setObject:value forKey:key];

        _skipWhitespace(reader);
        if (reader->position >= reader->length) {
            break;
        }

        char c = reader->bytes[reader->position++];

        if (c == '}') {
            return object;
        }
        if (c != ',') {
            break;
        }
    }
    reader->failed = YES;
    return nil;
}

static id _parseValue(JSONReader *reader) {
    _skipWhitespace(reader);
    if (reader->position >= reader->length) {
        reader->failed = YES;
        return nil;
    }

    char c = reader->bytes[reader->position];

    if (c == '{') {
        return _parseObject(reader);
    }
    if (c == '[') {
        return _parseArray(reader);
    }
    if (c == '"') {
        return _parseString(reader);
    }
    if (reader->length - reader->position >= 4 &&
        strncmp(reader->bytes + reader->position, "true", 4) == 0) {
        reader->position += 4;
        return [NSNumber numberWithBool:YES];
    }
    if (reader->length - reader->position >= 5 &&
        strncmp(reader->bytes + reader->position, "false", 5) == 0) {
        reader->position += 5;
        return [NSNumber numberWithBool:NO];
    }
    if (reader->length - reader->position >= 4 &&
        strncmp(reader->bytes + reader->position, "null", 4) == 0) {
        reader->position += 4;
        return [NSNull null];
    }
    return _parseNumber(reader);
}

/* ---- writing ------------------------------------------------------------ */

static BOOL _writeValue(id object, NSMutableData *out, BOOL sorted);

static void _writeString(NSString *string, NSMutableData *out) {
    [out appendBytes:"\"" length:1];

    const char *utf8 = [string UTF8String];

    for (const char *p = utf8; p != NULL && *p != '\0'; p++) {
        unsigned char c = (unsigned char)*p;

        switch (c) {
            case '"':  [out appendBytes:"\\\"" length:2]; break;
            case '\\': [out appendBytes:"\\\\" length:2]; break;
            case '\b': [out appendBytes:"\\b" length:2]; break;
            case '\f': [out appendBytes:"\\f" length:2]; break;
            case '\n': [out appendBytes:"\\n" length:2]; break;
            case '\r': [out appendBytes:"\\r" length:2]; break;
            case '\t': [out appendBytes:"\\t" length:2]; break;
            default:
                if (c < 0x20) {
                    char escape[7];

                    snprintf(escape, sizeof(escape), "\\u%04x", c);
                    [out appendBytes:escape length:6];
                } else {
                    [out appendBytes:p length:1];
                }
                break;
        }
    }
    [out appendBytes:"\"" length:1];
}

static BOOL _writeValue(id object, NSMutableData *out, BOOL sorted) {
    if (object == nil || [object isKindOfClass:[NSNull class]]) {
        [out appendBytes:"null" length:4];
        return YES;
    }
    if ([object isKindOfClass:[NSString class]]) {
        _writeString(object, out);
        return YES;
    }
    if ([object isKindOfClass:[NSNumber class]]) {
        const char *type = [object objCType];
        char text[64];

        if (type != NULL && (*type == 'c' || *type == 'B')) {
            const char *literal = [object boolValue] ? "true" : "false";

            [out appendBytes:literal length:strlen(literal)];
            return YES;
        }
        if (type != NULL && (*type == 'f' || *type == 'd')) {
            snprintf(text, sizeof(text), "%.17g", [object doubleValue]);
        } else {
            snprintf(text, sizeof(text), "%lld", [object longLongValue]);
        }
        [out appendBytes:text length:strlen(text)];
        return YES;
    }
    if ([object isKindOfClass:[NSArray class]]) {
        [out appendBytes:"[" length:1];

        NSUInteger index = 0;

        for (id value in object) {
            if (index++ > 0) {
                [out appendBytes:"," length:1];
            }
            if (!_writeValue(value, out, sorted)) {
                return NO;
            }
        }
        [out appendBytes:"]" length:1];
        return YES;
    }
    if ([object isKindOfClass:[NSDictionary class]]) {
        [out appendBytes:"{" length:1];

        NSArray *keys = [object allKeys];

        if (sorted) {
            keys = [keys sortedArrayUsingSelector:@selector(compare:)];
        }

        NSUInteger index = 0;

        for (id key in keys) {
            if (![key isKindOfClass:[NSString class]]) {
                return NO;
            }
            if (index++ > 0) {
                [out appendBytes:"," length:1];
            }
            _writeString(key, out);
            [out appendBytes:":" length:1];
            if (!_writeValue([object objectForKey:key], out, sorted)) {
                return NO;
            }
        }
        [out appendBytes:"}" length:1];
        return YES;
    }
    return NO;
}

@implementation NSJSONSerialization

+ (BOOL)isValidJSONObject:(id)object {
    if (![object isKindOfClass:[NSArray class]] &&
        ![object isKindOfClass:[NSDictionary class]]) {
        return NO;
    }

    NSMutableData *scratch = [NSMutableData data];

    return _writeValue(object, scratch, NO);
}

+ (NSData *)dataWithJSONObject:(id)object
                       options:(NSJSONWritingOptions)options
                         error:(NSError **)error {
    NSMutableData *out = [NSMutableData data];

    if (!_writeValue(object, out, (options & NSJSONWritingSortedKeys) != 0)) {
        if (error != NULL) {
            *error = nil;
        }
        return nil;
    }
    return out;
}

+ (id)JSONObjectWithData:(NSData *)data
                 options:(NSJSONReadingOptions)options
                   error:(NSError **)error {
    if (data == nil) {
        if (error != NULL) {
            *error = nil;
        }
        return nil;
    }

    JSONReader reader = { (const char *)[data bytes], [data length], 0, NO };
    id result = _parseValue(&reader);

    if (reader.failed) {
        if (error != NULL) {
            *error = nil;
        }
        return nil;
    }
    return result;
}

@end
