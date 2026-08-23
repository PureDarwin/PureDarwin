/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef NSGeometry_h
#define NSGeometry_h

#import <Foundation/NSObjCRuntime.h>
#include <math.h>

/* The geometry types belong to CoreFoundation, as on Darwin; NSPoint and
 * friends are aliases of them so CG and NS code share one layout. */
#include <CoreFoundation/CFCGTypes.h>

typedef CGPoint NSPoint;
typedef CGSize NSSize;
typedef CGRect NSRect;

typedef NSPoint *NSPointPointer;
typedef NSSize *NSSizePointer;
typedef NSRect *NSRectPointer;

typedef NS_ENUM(NSUInteger, NSRectEdge) {
    NSRectEdgeMinX = 0,
    NSRectEdgeMinY = 1,
    NSRectEdgeMaxX = 2,
    NSRectEdgeMaxY = 3,
};

FOUNDATION_EXPORT const NSPoint NSZeroPoint;
FOUNDATION_EXPORT const NSSize NSZeroSize;
FOUNDATION_EXPORT const NSRect NSZeroRect;

NS_INLINE NSPoint NSMakePoint(CGFloat x, CGFloat y) {
    NSPoint p;
    p.x = x;
    p.y = y;
    return p;
}

NS_INLINE NSSize NSMakeSize(CGFloat w, CGFloat h) {
    NSSize s;
    s.width = w;
    s.height = h;
    return s;
}

NS_INLINE NSRect NSMakeRect(CGFloat x, CGFloat y, CGFloat w, CGFloat h) {
    NSRect r;
    r.origin = NSMakePoint(x, y);
    r.size = NSMakeSize(w, h);
    return r;
}

NS_INLINE CGFloat NSMinX(NSRect r) { return r.origin.x; }
NS_INLINE CGFloat NSMinY(NSRect r) { return r.origin.y; }
NS_INLINE CGFloat NSMaxX(NSRect r) { return r.origin.x + r.size.width; }
NS_INLINE CGFloat NSMaxY(NSRect r) { return r.origin.y + r.size.height; }
NS_INLINE CGFloat NSMidX(NSRect r) { return r.origin.x + r.size.width / 2; }
NS_INLINE CGFloat NSMidY(NSRect r) { return r.origin.y + r.size.height / 2; }
NS_INLINE CGFloat NSWidth(NSRect r) { return r.size.width; }
NS_INLINE CGFloat NSHeight(NSRect r) { return r.size.height; }

NS_INLINE BOOL NSEqualPoints(NSPoint a, NSPoint b) {
    return a.x == b.x && a.y == b.y;
}

NS_INLINE BOOL NSEqualSizes(NSSize a, NSSize b) {
    return a.width == b.width && a.height == b.height;
}

NS_INLINE BOOL NSEqualRects(NSRect a, NSRect b) {
    return NSEqualPoints(a.origin, b.origin) && NSEqualSizes(a.size, b.size);
}

NS_INLINE BOOL NSIsEmptyRect(NSRect r) {
    return !(r.size.width > 0 && r.size.height > 0);
}

NS_INLINE BOOL NSPointInRect(NSPoint p, NSRect r) {
    return p.x >= NSMinX(r) && p.x < NSMaxX(r) && p.y >= NSMinY(r) && p.y < NSMaxY(r);
}

NS_INLINE BOOL NSContainsRect(NSRect a, NSRect b) {
    return !NSIsEmptyRect(b) && NSMinX(b) >= NSMinX(a) && NSMinY(b) >= NSMinY(a)
        && NSMaxX(b) <= NSMaxX(a) && NSMaxY(b) <= NSMaxY(a);
}

NS_INLINE BOOL NSIntersectsRect(NSRect a, NSRect b) {
    if (NSIsEmptyRect(a) || NSIsEmptyRect(b)) {
        return NO;
    }
    return NSMaxX(a) > NSMinX(b) && NSMaxX(b) > NSMinX(a)
        && NSMaxY(a) > NSMinY(b) && NSMaxY(b) > NSMinY(a);
}

FOUNDATION_EXPORT NSRect NSIntersectionRect(NSRect a, NSRect b);
FOUNDATION_EXPORT NSRect NSUnionRect(NSRect a, NSRect b);
FOUNDATION_EXPORT NSRect NSInsetRect(NSRect r, CGFloat dx, CGFloat dy);
FOUNDATION_EXPORT NSRect NSOffsetRect(NSRect r, CGFloat dx, CGFloat dy);
FOUNDATION_EXPORT NSRect NSIntegralRect(NSRect r);
FOUNDATION_EXPORT void NSDivideRect(NSRect r, NSRect *slice, NSRect *remainder,
                                    CGFloat amount, NSRectEdge edge);

#endif /* NSGeometry_h */
