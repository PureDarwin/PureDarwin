// NSLayoutConstraint.m - Auto Layout constraints, kept as data
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <AppKit/NSLayoutConstraint.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSString.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSValue.h>
#import <objc/runtime.h>
#include <math.h>

const NSLayoutPriority NSLayoutPriorityRequired = 1000;
const NSLayoutPriority NSLayoutPriorityDefaultHigh = 750;
const NSLayoutPriority NSLayoutPriorityDefaultLow = 250;



@interface NSLayoutConstraint (Owner)
- (void)_setOwner:(id)owner;
@end

// one view's edges while solving, in the coordinates of the view that holds the constraints
typedef struct {
    NSView *view;
    CGFloat minX, maxX, minY, maxY;
    // edges a constraint has set in this pass: sizes give way to them
    BOOL pinMinX, pinMaxX, pinMinY, pinMaxY;
} PDLayoutItem;

static PDLayoutItem *itemFor(PDLayoutItem *items, NSUInteger count, id view) {
    for (NSUInteger i = 0; i < count; i++) {
        if (items[i].view == view)
            return &items[i];
    }
    return NULL;
}

// leading and trailing are left and right: this AppKit lays out left to right only
static CGFloat attributeValue(PDLayoutItem *item, NSLayoutAttribute attribute, BOOL flipped) {
    switch (attribute) {
    case NSLayoutAttributeLeft: case NSLayoutAttributeLeading: return item->minX;
    case NSLayoutAttributeRight: case NSLayoutAttributeTrailing: return item->maxX;
    case NSLayoutAttributeTop: return flipped ? item->minY : item->maxY;
    case NSLayoutAttributeBottom: case NSLayoutAttributeLastBaseline: return flipped ? item->maxY : item->minY;
    case NSLayoutAttributeWidth: return item->maxX - item->minX;
    case NSLayoutAttributeHeight: return item->maxY - item->minY;
    case NSLayoutAttributeCenterX: return (item->minX + item->maxX) / 2;
    case NSLayoutAttributeCenterY: return (item->minY + item->maxY) / 2;
    default: return 0;
    }
}

// moving one edge: the view keeps its size unless its other edge is pinned
static void setEdge(CGFloat *edge, CGFloat *other, BOOL *pin, BOOL otherPinned, CGFloat value) {
    if (!otherPinned)
        *other += value - *edge;
    *edge = value;
    *pin = YES;
}

// a size: the free edge moves, a view pinned on both sides keeps what the edges say
static void setSize(CGFloat *min, CGFloat *max, BOOL *pinMin, BOOL *pinMax, CGFloat value) {
    if (*pinMin && *pinMax)
        return;
    if (*pinMax)
        *min = *max - value;
    else
        *max = *min + value;
}

static void setCenter(CGFloat *min, CGFloat *max, BOOL pinMin, BOOL pinMax, CGFloat value) {
    CGFloat shift = value - (*min + *max) / 2;

    if (pinMin || pinMax)
        return;
    *min += shift;
    *max += shift;
}

static void setAttribute(PDLayoutItem *item, NSLayoutAttribute attribute, CGFloat value, BOOL flipped) {
    switch (attribute) {
    case NSLayoutAttributeLeft: case NSLayoutAttributeLeading:
        setEdge(&item->minX, &item->maxX, &item->pinMinX, item->pinMaxX, value); break;
    case NSLayoutAttributeRight: case NSLayoutAttributeTrailing:
        setEdge(&item->maxX, &item->minX, &item->pinMaxX, item->pinMinX, value); break;
    case NSLayoutAttributeTop:
        if (flipped)
            setEdge(&item->minY, &item->maxY, &item->pinMinY, item->pinMaxY, value);
        else
            setEdge(&item->maxY, &item->minY, &item->pinMaxY, item->pinMinY, value);
        break;
    case NSLayoutAttributeBottom:
        if (flipped)
            setEdge(&item->maxY, &item->minY, &item->pinMaxY, item->pinMinY, value);
        else
            setEdge(&item->minY, &item->maxY, &item->pinMinY, item->pinMaxY, value);
        break;
    case NSLayoutAttributeWidth: setSize(&item->minX, &item->maxX, &item->pinMinX, &item->pinMaxX, value); break;
    case NSLayoutAttributeHeight: setSize(&item->minY, &item->maxY, &item->pinMinY, &item->pinMaxY, value); break;
    case NSLayoutAttributeCenterX: setCenter(&item->minX, &item->maxX, item->pinMinX, item->pinMaxX, value); break;
    case NSLayoutAttributeCenterY: setCenter(&item->minY, &item->maxY, item->pinMinY, item->pinMaxY, value); break;
    default: break;
    }
}

static BOOL isHorizontal(NSLayoutAttribute attribute) {
    return attribute == NSLayoutAttributeLeft || attribute == NSLayoutAttributeRight || attribute == NSLayoutAttributeLeading ||
           attribute == NSLayoutAttributeTrailing || attribute == NSLayoutAttributeWidth || attribute == NSLayoutAttributeCenterX;
}

@implementation NSView (NSConstraintBasedLayout)

static const char constraintsKey, translatesKey;

- (NSMutableArray *)_pdConstraints:(BOOL)create {
    NSMutableArray *list = objc_getAssociatedObject(self, &constraintsKey);

    if (list == nil && create) {
        list = [NSMutableArray array];
        objc_setAssociatedObject(self, &constraintsKey, list, OBJC_ASSOCIATION_RETAIN);
    }
    return list;
}

- (NSArray *)constraints {
    NSArray *list = [self _pdConstraints:NO];

    return list ? [NSArray arrayWithArray:list] : [NSArray array];
}

- (void)addConstraint:(NSLayoutConstraint *)constraint {
    if (constraint == nil)
        return;
    [[self _pdConstraints:YES] addObject:constraint];
    [constraint _setOwner:self];
}

- (void)addConstraints:(NSArray *)constraints {
    for (NSLayoutConstraint *constraint in constraints)
        [self addConstraint:constraint];
}

- (void)removeConstraint:(NSLayoutConstraint *)constraint {
    [constraint _setOwner:nil];
    [[self _pdConstraints:NO] removeObjectIdenticalTo:constraint];
}

- (void)removeConstraints:(NSArray *)constraints {
    for (NSLayoutConstraint *constraint in constraints)
        [self removeConstraint:constraint];
}

- (BOOL)translatesAutoresizingMaskIntoConstraints {
    NSNumber *value = objc_getAssociatedObject(self, &translatesKey);

    return value ? [value boolValue] : YES;
}

- (void)setTranslatesAutoresizingMaskIntoConstraints:(BOOL)flag {
    objc_setAssociatedObject(self, &translatesKey, [NSNumber numberWithBool:flag], OBJC_ASSOCIATION_RETAIN);
}

- (void)layoutSubtreeIfNeeded {
    [self _pdLayoutConstraints];
    for (NSView *subview in [self subviews])
        [subview layoutSubtreeIfNeeded];
}

- (void)setNeedsLayout:(BOOL)flag {
    if (flag)
        [self _pdLayoutConstraints];
}

// solves the equalities this view holds for its direct subviews by relaxation: each pass sets the first
// item's attribute (or the second's when the first is this view), and a few passes settle chains of them
- (void)_pdLayoutConstraints {
    NSArray *constraints = [self _pdConstraints:NO];
    NSArray *subviews = [self subviews];
    NSUInteger count = [subviews count];
    BOOL flipped = [self isFlipped];

    if ([constraints count] == 0 || count == 0)
        return;

    PDLayoutItem *items = calloc(count + 1, sizeof(PDLayoutItem));
    NSRect bounds = [self bounds];

    items[0].view = self;
    items[0].minX = NSMinX(bounds), items[0].maxX = NSMaxX(bounds);
    items[0].minY = NSMinY(bounds), items[0].maxY = NSMaxY(bounds);
    for (NSUInteger i = 0; i < count; i++) {
        NSView *view = [subviews objectAtIndex:i];
        NSRect frame = [view frame];

        items[i + 1].view = view;
        items[i + 1].minX = NSMinX(frame), items[i + 1].maxX = NSMaxX(frame);
        items[i + 1].minY = NSMinY(frame), items[i + 1].maxY = NSMaxY(frame);
    }

    for (int pass = 0; pass < 4; pass++) {
        for (NSUInteger i = 1; i <= count; i++)
            items[i].pinMinX = items[i].pinMaxX = items[i].pinMinY = items[i].pinMaxY = NO;

        // edges first, then sizes and centres, so a size gives way to the edges around it
        for (int phase = 0; phase < 2; phase++) {
            for (NSLayoutConstraint *c in constraints) {
                NSLayoutAttribute a1 = [c firstAttribute], a2 = [c secondAttribute];
                BOOL sizeOrCenter = a1 == NSLayoutAttributeWidth || a1 == NSLayoutAttributeHeight ||
                                    a1 == NSLayoutAttributeCenterX || a1 == NSLayoutAttributeCenterY;

                if ([c relation] != NSLayoutRelationEqual || [c priority] < NSLayoutPriorityRequired || ![c isActive])
                    continue;
                if (sizeOrCenter != (phase == 1))
                    continue;

                PDLayoutItem *first = itemFor(items, count + 1, [c firstItem]);
                PDLayoutItem *second = [c secondItem] ? itemFor(items, count + 1, [c secondItem]) : NULL;
                CGFloat multiplier = [c multiplier] ? [c multiplier] : 1;

                if (first == NULL || ([c secondItem] != nil && second == NULL))
                    continue;
                if (second != NULL && isHorizontal(a1) != isHorizontal(a2) && a1 != NSLayoutAttributeWidth &&
                    a1 != NSLayoutAttributeHeight)
                    continue;

                // this view's own frame is given: solve for the other item instead
                if (first == &items[0]) {
                    if (second == NULL || second == &items[0])
                        continue;
                    setAttribute(second, a2, (attributeValue(first, a1, flipped) - [c constant]) / multiplier, flipped);
                    continue;
                }

                CGFloat value = (second ? attributeValue(second, a2, flipped) * multiplier : 0) + [c constant];

                // two siblings' edges: the larger view moves its edge, so sidebars and bars keep their size
                if (second != NULL && second != &items[0] && second != first && !sizeOrCenter) {
                    CGFloat firstSize = isHorizontal(a1) ? first->maxX - first->minX : first->maxY - first->minY;
                    CGFloat secondSize = isHorizontal(a2) ? second->maxX - second->minX : second->maxY - second->minY;

                    if (secondSize > firstSize) {
                        setAttribute(second, a2, (attributeValue(first, a1, flipped) - [c constant]) / multiplier, flipped);
                        continue;
                    }
                }
                setAttribute(first, a1, value, flipped);
            }
        }
    }

    for (NSUInteger i = 1; i <= count; i++) {
        NSRect frame = NSMakeRect(items[i].minX, items[i].minY, fmax(0, items[i].maxX - items[i].minX),
                                  fmax(0, items[i].maxY - items[i].minY));

        if (!NSEqualRects(frame, [items[i].view frame])) {
            [items[i].view setFrame:frame];
            [items[i].view setNeedsDisplay:YES];
        }
    }
    free(items);
    [self setNeedsDisplay:YES];
}

@end
