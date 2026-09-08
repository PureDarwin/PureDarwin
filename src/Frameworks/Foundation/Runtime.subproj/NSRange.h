/*
 * Copyright (C) 2026, Samuel Zormeister.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSRange_h
#define NSRange_h

#import <Foundation/NSObjCRuntime.h>

typedef struct _NSRange {
    NSUInteger location;
    NSUInteger length;
} NSRange;

typedef NSRange *NSRangePointer;

NS_INLINE NSRange NSMakeRange(NSUInteger loc, NSUInteger len) {
    NSRange r;
    r.location = loc;
    r.length = len;
    return r;
}

NS_INLINE NSRange NSIntersectionRange(NSRange a, NSRange b) {
    NSUInteger start = (a.location > b.location) ? a.location : b.location;
    NSUInteger endA = a.location + a.length;
    NSUInteger endB = b.location + b.length;
    NSUInteger end = (endA < endB) ? endA : endB;

    if (end <= start) {
        return (NSRange){ 0, 0 };
    }
    return (NSRange){ start, end - start };
}

NS_INLINE NSRange NSUnionRange(NSRange a, NSRange b) {
    NSUInteger start = (a.location < b.location) ? a.location : b.location;
    NSUInteger endA = a.location + a.length;
    NSUInteger endB = b.location + b.length;
    NSUInteger end = (endA > endB) ? endA : endB;

    return (NSRange){ start, end - start };
}

NS_INLINE NSUInteger NSMaxRange(NSRange range) {
    return range.location + range.length;
}

NS_INLINE BOOL NSLocationInRange(NSUInteger loc, NSRange range) {
    return loc >= range.location && loc < NSMaxRange(range);
}

NS_INLINE BOOL NSEqualRanges(NSRange a, NSRange b) {
    return a.location == b.location && a.length == b.length;
}

#endif /* NSRange_h */
