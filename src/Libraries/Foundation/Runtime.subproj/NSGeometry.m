/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#import <Foundation/NSGeometry.h>
#include <stddef.h>

const NSPoint NSZeroPoint = { 0, 0 };
const NSSize NSZeroSize = { 0, 0 };
const NSRect NSZeroRect = { { 0, 0 }, { 0, 0 } };

NSRect NSIntersectionRect(NSRect a, NSRect b) {
    if (!NSIntersectsRect(a, b)) {
        return NSZeroRect;
    }

    CGFloat minX = NSMinX(a) > NSMinX(b) ? NSMinX(a) : NSMinX(b);
    CGFloat minY = NSMinY(a) > NSMinY(b) ? NSMinY(a) : NSMinY(b);
    CGFloat maxX = NSMaxX(a) < NSMaxX(b) ? NSMaxX(a) : NSMaxX(b);
    CGFloat maxY = NSMaxY(a) < NSMaxY(b) ? NSMaxY(a) : NSMaxY(b);

    return NSMakeRect(minX, minY, maxX - minX, maxY - minY);
}

NSRect NSUnionRect(NSRect a, NSRect b) {
    if (NSIsEmptyRect(a)) {
        return NSIsEmptyRect(b) ? NSZeroRect : b;
    }
    if (NSIsEmptyRect(b)) {
        return a;
    }

    CGFloat minX = NSMinX(a) < NSMinX(b) ? NSMinX(a) : NSMinX(b);
    CGFloat minY = NSMinY(a) < NSMinY(b) ? NSMinY(a) : NSMinY(b);
    CGFloat maxX = NSMaxX(a) > NSMaxX(b) ? NSMaxX(a) : NSMaxX(b);
    CGFloat maxY = NSMaxY(a) > NSMaxY(b) ? NSMaxY(a) : NSMaxY(b);

    return NSMakeRect(minX, minY, maxX - minX, maxY - minY);
}

NSRect NSInsetRect(NSRect r, CGFloat dx, CGFloat dy) {
    return NSMakeRect(r.origin.x + dx, r.origin.y + dy,
                      r.size.width - 2 * dx, r.size.height - 2 * dy);
}

NSRect NSOffsetRect(NSRect r, CGFloat dx, CGFloat dy) {
    return NSMakeRect(r.origin.x + dx, r.origin.y + dy, r.size.width, r.size.height);
}

NSRect NSIntegralRect(NSRect r) {
    if (NSIsEmptyRect(r)) {
        return NSZeroRect;
    }

    CGFloat minX = floor(NSMinX(r));
    CGFloat minY = floor(NSMinY(r));
    return NSMakeRect(minX, minY, ceil(NSMaxX(r)) - minX, ceil(NSMaxY(r)) - minY);
}

void NSDivideRect(NSRect r, NSRect *slice, NSRect *remainder, CGFloat amount, NSRectEdge edge) {
    NSRect localSlice, localRemainder;

    if (slice == NULL) {
        slice = &localSlice;
    }
    if (remainder == NULL) {
        remainder = &localRemainder;
    }

    if (NSIsEmptyRect(r)) {
        *slice = NSZeroRect;
        *remainder = NSZeroRect;
        return;
    }

    switch (edge) {
        case NSRectEdgeMinX:
            if (amount > r.size.width) {
                amount = r.size.width;
            }
            *slice = NSMakeRect(NSMinX(r), NSMinY(r), amount, r.size.height);
            *remainder = NSMakeRect(NSMinX(r) + amount, NSMinY(r),
                                    r.size.width - amount, r.size.height);
            break;

        case NSRectEdgeMinY:
            if (amount > r.size.height) {
                amount = r.size.height;
            }
            *slice = NSMakeRect(NSMinX(r), NSMinY(r), r.size.width, amount);
            *remainder = NSMakeRect(NSMinX(r), NSMinY(r) + amount,
                                    r.size.width, r.size.height - amount);
            break;

        case NSRectEdgeMaxX:
            if (amount > r.size.width) {
                amount = r.size.width;
            }
            *slice = NSMakeRect(NSMaxX(r) - amount, NSMinY(r), amount, r.size.height);
            *remainder = NSMakeRect(NSMinX(r), NSMinY(r),
                                    r.size.width - amount, r.size.height);
            break;

        case NSRectEdgeMaxY:
            if (amount > r.size.height) {
                amount = r.size.height;
            }
            *slice = NSMakeRect(NSMinX(r), NSMaxY(r) - amount, r.size.width, amount);
            *remainder = NSMakeRect(NSMinX(r), NSMinY(r),
                                    r.size.width, r.size.height - amount);
            break;
    }
}
