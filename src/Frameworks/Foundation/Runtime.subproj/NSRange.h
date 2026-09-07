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

NS_INLINE NSRange NSMakeRange(NSUInteger loc, NSUInteger len) {
    NSRange r;
    r.location = loc;
    r.length = len;
    return r;
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
