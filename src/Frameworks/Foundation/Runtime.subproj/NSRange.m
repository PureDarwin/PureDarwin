// NSRange.m - NSRange intersection, union and string conversion
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0

#import <Foundation/NSRange.h>
#import <Foundation/NSString.h>
#include <stdlib.h>

NSRange NSIntersectionRange(NSRange a, NSRange b) {
    NSUInteger start = (a.location > b.location) ? a.location : b.location;
    NSUInteger endA = a.location + a.length;
    NSUInteger endB = b.location + b.length;
    NSUInteger end = (endA < endB) ? endA : endB;

    if (end <= start) {
        return (NSRange){ 0, 0 };
    }
    return (NSRange){ start, end - start };
}

NSRange NSUnionRange(NSRange a, NSRange b) {
    NSUInteger start = (a.location < b.location) ? a.location : b.location;
    NSUInteger endA = a.location + a.length;
    NSUInteger endB = b.location + b.length;
    NSUInteger end = (endA > endB) ? endA : endB;

    return (NSRange){ start, end - start };
}

NSString *NSStringFromRange(NSRange range) {
    return [NSString stringWithFormat:@"{%lu, %lu}", (unsigned long)range.location, (unsigned long)range.length];
}

// macOS takes the first two numbers wherever they are, so "{3, 4}" and "3 4" both work
NSRange NSRangeFromString(NSString *string) {
    NSRange result = NSMakeRange(0, 0);
    const char *p = [string UTF8String];
    unsigned long values[2] = { 0, 0 };
    int found = 0;

    while (p != NULL && *p != 0 && found < 2) {
        if (*p >= '0' && *p <= '9') {
            char *end;

            values[found++] = strtoul(p, &end, 10);
            p = end;
        } else
            p++;
    }
    if (found > 0)
        result.location = values[0];
    if (found > 1)
        result.length = values[1];
    return result;
}
