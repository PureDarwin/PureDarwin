// CGGeometryExports.m - CGRect accessors as exported functions
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// apps built against Apple's headers call these as functions while CGGeometry.h keeps them inline,
// and its inlines would take the names here, so this file spells out the layouts itself

#include <math.h>
#include <stdbool.h>

typedef double CGFloat;
typedef struct { CGFloat x, y; } CGPoint;
typedef struct { CGFloat width, height; } CGSize;
typedef struct { CGPoint origin; CGSize size; } CGRect;

#define EXPORT __attribute__((visibility("default")))

// rectangles are standardized first, as Apple documents
static CGRect standardize(CGRect rect)
{
    if (rect.size.width < 0) {
        rect.origin.x += rect.size.width;
        rect.size.width = -rect.size.width;
    }
    if (rect.size.height < 0) {
        rect.origin.y += rect.size.height;
        rect.size.height = -rect.size.height;
    }
    return rect;
}

EXPORT CGFloat CGRectGetMinX(CGRect rect) { return standardize(rect).origin.x; }
EXPORT CGFloat CGRectGetMinY(CGRect rect) { return standardize(rect).origin.y; }
EXPORT CGFloat CGRectGetWidth(CGRect rect) { return fabs(rect.size.width); }
EXPORT CGFloat CGRectGetHeight(CGRect rect) { return fabs(rect.size.height); }
EXPORT CGFloat CGRectGetMaxX(CGRect rect) { return CGRectGetMinX(rect) + CGRectGetWidth(rect); }
EXPORT CGFloat CGRectGetMaxY(CGRect rect) { return CGRectGetMinY(rect) + CGRectGetHeight(rect); }
EXPORT CGFloat CGRectGetMidX(CGRect rect) { return CGRectGetMinX(rect) + CGRectGetWidth(rect) / 2; }
EXPORT CGFloat CGRectGetMidY(CGRect rect) { return CGRectGetMinY(rect) + CGRectGetHeight(rect) / 2; }

// the null rectangle sits at infinity
EXPORT bool CGRectIsNull(CGRect rect)
{
    return isinf(rect.origin.x) || isinf(rect.origin.y);
}

EXPORT bool CGRectIsInfinite(CGRect rect)
{
    return isinf(rect.size.width) && isinf(rect.size.height);
}

EXPORT bool CGRectIsEmpty(CGRect rect)
{
    return CGRectIsNull(rect) || rect.size.width == 0 || rect.size.height == 0;
}

EXPORT bool CGPointEqualToPoint(CGPoint a, CGPoint b)
{
    return a.x == b.x && a.y == b.y;
}

EXPORT bool CGRectEqualToRect(CGRect a, CGRect b)
{
    if (CGRectIsNull(a) || CGRectIsNull(b))
        return CGRectIsNull(a) && CGRectIsNull(b);
    a = standardize(a);
    b = standardize(b);
    return a.origin.x == b.origin.x && a.origin.y == b.origin.y &&
        a.size.width == b.size.width && a.size.height == b.size.height;
}

// the minimum edges are inside, the maximum edges are not
EXPORT bool CGRectContainsPoint(CGRect rect, CGPoint point)
{
    if (CGRectIsEmpty(rect))
        return false;
    return point.x >= CGRectGetMinX(rect) && point.x < CGRectGetMaxX(rect) &&
        point.y >= CGRectGetMinY(rect) && point.y < CGRectGetMaxY(rect);
}

EXPORT bool CGRectContainsRect(CGRect a, CGRect b)
{
    if (CGRectIsNull(a) || CGRectIsNull(b))
        return false;
    return CGRectGetMinX(b) >= CGRectGetMinX(a) && CGRectGetMaxX(b) <= CGRectGetMaxX(a) &&
        CGRectGetMinY(b) >= CGRectGetMinY(a) && CGRectGetMaxY(b) <= CGRectGetMaxY(a);
}

EXPORT bool CGRectIntersectsRect(CGRect a, CGRect b)
{
    if (CGRectIsEmpty(a) || CGRectIsEmpty(b))
        return false;
    return CGRectGetMinX(a) < CGRectGetMaxX(b) && CGRectGetMinX(b) < CGRectGetMaxX(a) &&
        CGRectGetMinY(a) < CGRectGetMaxY(b) && CGRectGetMinY(b) < CGRectGetMaxY(a);
}

EXPORT CGRect CGRectInset(CGRect rect, CGFloat dx, CGFloat dy)
{
    static const CGRect null = { { INFINITY, INFINITY }, { 0, 0 } };

    if (CGRectIsNull(rect))
        return rect;
    rect = standardize(rect);
    rect.origin.x += dx;
    rect.origin.y += dy;
    rect.size.width -= 2 * dx;
    rect.size.height -= 2 * dy;
    return (rect.size.width < 0 || rect.size.height < 0) ? null : rect;
}

EXPORT CGRect CGRectOffset(CGRect rect, CGFloat dx, CGFloat dy)
{
    if (CGRectIsNull(rect))
        return rect;
    rect = standardize(rect);
    rect.origin.x += dx;
    rect.origin.y += dy;
    return rect;
}
