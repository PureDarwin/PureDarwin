/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * Backed by POSIX extended regular expressions from libSystem. That covers the
 * pattern syntax in use here (character classes, alternation, capture groups,
 * quantifiers) but is not ICU: named groups, lookaround and \d-style
 * shorthands are not part of ERE and will fail to compile rather than
 * silently mis-match.
 *
 * Ranges are in UTF-8 bytes internally and converted back to UTF-16 units,
 * which is what NSString ranges count in.
 */

#import <Foundation/NSRegularExpression.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>
#import <Foundation/NSError.h>
#include <regex.h>
#include <stdlib.h>
#include <string.h>

@implementation NSRegularExpression

+ (NSRegularExpression *)regularExpressionWithPattern:(NSString *)pattern
                                              options:(NSRegularExpressionOptions)options
                                                error:(NSError **)error {
    return [[[self alloc] initWithPattern:pattern options:options error:error] autorelease];
}

- (instancetype)initWithPattern:(NSString *)pattern
                        options:(NSRegularExpressionOptions)options
                          error:(NSError **)error {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _pattern = [pattern copy];
    _options = options;

    regex_t *compiled = calloc(1, sizeof(regex_t));

    if (compiled == NULL) {
        [self release];
        return nil;
    }

    int flags = REG_EXTENDED;

    if (options & NSRegularExpressionCaseInsensitive) {
        flags |= REG_ICASE;
    }
    if (options & NSRegularExpressionAnchorsMatchLines) {
        flags |= REG_NEWLINE;
    }

    if (regcomp(compiled, [pattern UTF8String], flags) != 0) {
        free(compiled);
        if (error != NULL) {
            *error = nil;
        }
        [self release];
        return nil;
    }
    _compiled = compiled;
    return self;
}

- (void)dealloc {
    if (_compiled != NULL) {
        regfree((regex_t *)_compiled);
        free(_compiled);
    }
    [_pattern release];
    [super dealloc];
}

- (NSString *)pattern { return _pattern; }
- (NSRegularExpressionOptions)options { return _options; }

- (NSUInteger)numberOfCaptureGroups {
    return (_compiled != NULL) ? (NSUInteger)((regex_t *)_compiled)->re_nsub : 0;
}

/* Converts a byte offset in the UTF-8 form back to a UTF-16 index. */
static NSUInteger _utf16IndexForByteOffset(const char *utf8, size_t offset) {
    NSUInteger index = 0;

    for (size_t i = 0; i < offset && utf8[i] != '\0'; i++) {
        unsigned char c = (unsigned char)utf8[i];

        if ((c & 0xC0) == 0x80) {
            continue;   /* continuation byte */
        }
        /* Anything needing four UTF-8 bytes is a surrogate pair in UTF-16. */
        index += (c >= 0xF0) ? 2 : 1;
    }
    return index;
}

- (NSArray *)matchesInString:(NSString *)string
                     options:(NSMatchingOptions)options
                       range:(NSRange)range {
    if (_compiled == NULL || string == nil) {
        return [NSArray array];
    }

    NSString *subject = (range.location == 0 && range.length >= [string length])
        ? string : [string substringWithRange:range];
    const char *utf8 = [subject UTF8String];

    if (utf8 == NULL) {
        return [NSArray array];
    }

    NSMutableArray *results = [NSMutableArray array];
    size_t groups = ((regex_t *)_compiled)->re_nsub + 1;
    regmatch_t *found = calloc(groups, sizeof(regmatch_t));

    if (found == NULL) {
        return results;
    }

    size_t cursor = 0;

    while (regexec((regex_t *)_compiled, utf8 + cursor, groups, found, 0) == 0) {
        NSMutableArray *ranges = [NSMutableArray arrayWithCapacity:groups];

        for (size_t g = 0; g < groups; g++) {
            NSRange groupRange;

            if (found[g].rm_so < 0) {
                groupRange = NSMakeRange(NSNotFound, 0);
            } else {
                NSUInteger start = _utf16IndexForByteOffset(utf8, cursor + (size_t)found[g].rm_so);
                NSUInteger end = _utf16IndexForByteOffset(utf8, cursor + (size_t)found[g].rm_eo);

                groupRange = NSMakeRange(range.location + start, end - start);
            }
            [ranges addObject:[NSValue valueWithRange:groupRange]];
        }
        [results addObject:
            [NSTextCheckingResult regularExpressionCheckingResultWithRanges:ranges]];

        /* Step past this match; an empty match still has to advance. */
        size_t advance = (size_t)found[0].rm_eo;

        if (advance == (size_t)found[0].rm_so) {
            advance += 1;
        }
        cursor += advance;
        if (utf8[cursor] == '\0') {
            break;
        }
    }
    free(found);
    return results;
}

- (NSUInteger)numberOfMatchesInString:(NSString *)string
                              options:(NSMatchingOptions)options
                                range:(NSRange)range {
    return [[self matchesInString:string options:options range:range] count];
}

- (NSTextCheckingResult *)firstMatchInString:(NSString *)string
                                     options:(NSMatchingOptions)options
                                       range:(NSRange)range {
    NSArray *matches = [self matchesInString:string options:options range:range];

    return ([matches count] > 0) ? [matches objectAtIndex:0] : nil;
}

- (NSRange)rangeOfFirstMatchInString:(NSString *)string
                             options:(NSMatchingOptions)options
                               range:(NSRange)range {
    NSTextCheckingResult *match = [self firstMatchInString:string options:options range:range];

    return (match != nil) ? [match range] : NSMakeRange(NSNotFound, 0);
}

@end
