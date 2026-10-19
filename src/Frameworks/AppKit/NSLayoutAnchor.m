// NSLayoutAnchor.m - layout anchors and the visual format language, both making NSLayoutConstraints
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSLayoutAnchor.h>
#import <Foundation/Foundation.h>
#import <objc/runtime.h>

@implementation NSLayoutAnchor

- (instancetype)_initWithItem:(id)item attribute:(NSLayoutAttribute)attribute
{
    if ((self = [super init]) == nil)
        return nil;
    _item = item;
    _attribute = attribute;
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (id)item
{
    return _item;
}

- (NSString *)name
{
    static NSString *const names[] = { @"notAnAttribute", @"left", @"right", @"top", @"bottom", @"leading", @"trailing",
                                       @"width", @"height", @"centerX", @"centerY", @"lastBaseline", @"firstBaseline" };

    return (_attribute >= 0 && _attribute <= NSLayoutAttributeFirstBaseline) ? names[_attribute] : @"unknown";
}

- (BOOL)hasAmbiguousLayout
{
    return NO;
}

- (NSArray *)constraintsAffectingLayout
{
    return @[];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"<%@ %p \"%@.%@\">", [self class], self, [_item class], [self name]];
}

- (NSLayoutConstraint *)_constraintTo:(NSLayoutAnchor *)anchor relation:(NSLayoutRelation)relation multiplier:(CGFloat)m constant:(CGFloat)c
{
    return [NSLayoutConstraint constraintWithItem:_item attribute:_attribute relatedBy:relation
                                           toItem:anchor ? anchor->_item : nil
                                        attribute:anchor ? anchor->_attribute : NSLayoutAttributeNotAnAttribute
                                       multiplier:m constant:c];
}

- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor
{
    return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:1 constant:0];
}

- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor
{
    return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:1 constant:0];
}

- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor
{
    return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:1 constant:0];
}

- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:1 constant:c];
}

- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:1 constant:c];
}

- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutAnchor *)anchor constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:1 constant:c];
}

@end

@implementation NSLayoutXAxisAnchor
@end

@implementation NSLayoutYAxisAnchor
@end

@implementation NSLayoutDimension

- (NSLayoutConstraint *)constraintEqualToConstant:(CGFloat)c
{
    return [self _constraintTo:nil relation:NSLayoutRelationEqual multiplier:1 constant:c];
}

- (NSLayoutConstraint *)constraintGreaterThanOrEqualToConstant:(CGFloat)c
{
    return [self _constraintTo:nil relation:NSLayoutRelationGreaterThanOrEqual multiplier:1 constant:c];
}

- (NSLayoutConstraint *)constraintLessThanOrEqualToConstant:(CGFloat)c
{
    return [self _constraintTo:nil relation:NSLayoutRelationLessThanOrEqual multiplier:1 constant:c];
}

- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m
{
    return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:m constant:0];
}

- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m
{
    return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:m constant:0];
}

- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m
{
    return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:m constant:0];
}

- (NSLayoutConstraint *)constraintEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationEqual multiplier:m constant:c];
}

- (NSLayoutConstraint *)constraintGreaterThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationGreaterThanOrEqual multiplier:m constant:c];
}

- (NSLayoutConstraint *)constraintLessThanOrEqualToAnchor:(NSLayoutDimension *)anchor multiplier:(CGFloat)m constant:(CGFloat)c
{
    return [self _constraintTo:anchor relation:NSLayoutRelationLessThanOrEqual multiplier:m constant:c];
}

@end

static char PDAnchorsKey;

@implementation NSView (NSLayoutAnchors)

// each view keeps one anchor per attribute, as macOS hands out the same object every time
- (id)_pdAnchor:(Class)cls attribute:(NSLayoutAttribute)attribute
{
    NSMutableDictionary *anchors = objc_getAssociatedObject(self, &PDAnchorsKey);
    NSNumber *key = [NSNumber numberWithInteger:attribute];
    NSLayoutAnchor *anchor;

    if (anchors == nil) {
        anchors = [NSMutableDictionary dictionary];
        objc_setAssociatedObject(self, &PDAnchorsKey, anchors, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }
    anchor = [anchors objectForKey:key];
    if (anchor == nil) {
        anchor = [[[cls alloc] _initWithItem:self attribute:attribute] autorelease];
        [anchors setObject:anchor forKey:key];
    }
    return anchor;
}

- (NSLayoutXAxisAnchor *)leadingAnchor
{
    return [self _pdAnchor:[NSLayoutXAxisAnchor class] attribute:NSLayoutAttributeLeading];
}

- (NSLayoutXAxisAnchor *)trailingAnchor
{
    return [self _pdAnchor:[NSLayoutXAxisAnchor class] attribute:NSLayoutAttributeTrailing];
}

- (NSLayoutXAxisAnchor *)leftAnchor
{
    return [self _pdAnchor:[NSLayoutXAxisAnchor class] attribute:NSLayoutAttributeLeft];
}

- (NSLayoutXAxisAnchor *)rightAnchor
{
    return [self _pdAnchor:[NSLayoutXAxisAnchor class] attribute:NSLayoutAttributeRight];
}

- (NSLayoutYAxisAnchor *)topAnchor
{
    return [self _pdAnchor:[NSLayoutYAxisAnchor class] attribute:NSLayoutAttributeTop];
}

- (NSLayoutYAxisAnchor *)bottomAnchor
{
    return [self _pdAnchor:[NSLayoutYAxisAnchor class] attribute:NSLayoutAttributeBottom];
}

- (NSLayoutDimension *)widthAnchor
{
    return [self _pdAnchor:[NSLayoutDimension class] attribute:NSLayoutAttributeWidth];
}

- (NSLayoutDimension *)heightAnchor
{
    return [self _pdAnchor:[NSLayoutDimension class] attribute:NSLayoutAttributeHeight];
}

- (NSLayoutXAxisAnchor *)centerXAnchor
{
    return [self _pdAnchor:[NSLayoutXAxisAnchor class] attribute:NSLayoutAttributeCenterX];
}

- (NSLayoutYAxisAnchor *)centerYAnchor
{
    return [self _pdAnchor:[NSLayoutYAxisAnchor class] attribute:NSLayoutAttributeCenterY];
}

- (NSLayoutYAxisAnchor *)firstBaselineAnchor
{
    return [self _pdAnchor:[NSLayoutYAxisAnchor class] attribute:NSLayoutAttributeFirstBaseline];
}

- (NSLayoutYAxisAnchor *)lastBaselineAnchor
{
    return [self _pdAnchor:[NSLayoutYAxisAnchor class] attribute:NSLayoutAttributeLastBaseline];
}

@end

// the standard space between sibling views, and between a view and its superview's edge
static const CGFloat PDStandardSpacing = 8;
static const CGFloat PDStandardEdgeSpacing = 20;

struct predicate {
    NSLayoutRelation relation;
    CGFloat constant;
    id view;
    NSLayoutPriority priority;
};

// a number, or a name from the metrics, or for a view's size another view's name
static BOOL PDScanValue(NSScanner *scanner, NSDictionary *metrics, NSDictionary *views, CGFloat *value, id *view)
{
    double number;
    NSString *name;
    NSCharacterSet *nameChars = [NSCharacterSet characterSetWithCharactersInString:
                                                    @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"];

    *view = nil;
    if ([scanner scanDouble:&number]) {
        *value = number;
        return YES;
    }
    if (![scanner scanCharactersFromSet:nameChars intoString:&name])
        return NO;
    if ([metrics objectForKey:name] != nil) {
        *value = [[metrics objectForKey:name] doubleValue];
        return YES;
    }
    if ([views objectForKey:name] != nil) {
        *view = [views objectForKey:name];
        *value = 0;
        return YES;
    }
    [NSException raise:NSInvalidArgumentException format:@"Unknown metric or view %@ in visual format", name];
    return NO;
}

// "(>=20@750,<=40)" or a bare "-12-" spacing: relation, value and priority, comma separated
static NSArray *PDScanPredicates(NSScanner *scanner, NSDictionary *metrics, NSDictionary *views)
{
    NSMutableArray *predicates = [NSMutableArray array];

    do {
        struct predicate p = { NSLayoutRelationEqual, 0, nil, NSLayoutPriorityRequired };
        double priority;

        if ([scanner scanString:@">=" intoString:NULL])
            p.relation = NSLayoutRelationGreaterThanOrEqual;
        else if ([scanner scanString:@"<=" intoString:NULL])
            p.relation = NSLayoutRelationLessThanOrEqual;
        else
            [scanner scanString:@"==" intoString:NULL];
        if (!PDScanValue(scanner, metrics, views, &p.constant, &p.view))
            break;
        if ([scanner scanString:@"@" intoString:NULL] && [scanner scanDouble:&priority])
            p.priority = (NSLayoutPriority)priority;
        [predicates addObject:[NSValue valueWithBytes:&p objCType:@encode(struct predicate)]];
    } while ([scanner scanString:@"," intoString:NULL]);
    return predicates;
}

@implementation NSLayoutConstraint (NSVisualFormat)

// H: or V:, then |, views in brackets with optional size predicates, and spacing between them
+ (NSArray *)constraintsWithVisualFormat:(NSString *)format options:(NSLayoutFormatOptions)opts
                                 metrics:(NSDictionary *)metrics views:(NSDictionary *)views
{
    NSScanner *scanner = [NSScanner scannerWithString:format];
    NSMutableArray *constraints = [NSMutableArray array];
    NSMutableArray *chain = [NSMutableArray array];
    BOOL vertical = NO;
    BOOL leadingToTrailing = (opts & NSLayoutFormatDirectionMask) == NSLayoutFormatDirectionLeadingToTrailing;
    NSLayoutAttribute start, end, size;
    id previous = nil;
    BOOL previousIsSuperview = NO;
    NSArray *pendingSpacing = nil;
    BOOL pendingStandard = NO;

    [scanner setCharactersToBeSkipped:[NSCharacterSet whitespaceCharacterSet]];
    if ([scanner scanString:@"V:" intoString:NULL])
        vertical = YES;
    else
        [scanner scanString:@"H:" intoString:NULL];
    start = vertical ? NSLayoutAttributeTop : leadingToTrailing ? NSLayoutAttributeLeading : NSLayoutAttributeLeft;
    end = vertical ? NSLayoutAttributeBottom : leadingToTrailing ? NSLayoutAttributeTrailing : NSLayoutAttributeRight;
    size = vertical ? NSLayoutAttributeHeight : NSLayoutAttributeWidth;

    while (![scanner isAtEnd]) {
        if ([scanner scanString:@"|" intoString:NULL]) {
            id superview = [previous respondsToSelector:@selector(superview)] ? [previous superview] : nil;

            if (previous != nil && (pendingSpacing != nil || pendingStandard)) {
                NSArray *spacing = pendingSpacing ?: @[];
                if (pendingStandard) {
                    struct predicate p = { NSLayoutRelationEqual, PDStandardEdgeSpacing, nil, NSLayoutPriorityRequired };
                    spacing = @[ [NSValue valueWithBytes:&p objCType:@encode(struct predicate)] ];
                }
                for (NSValue *value in spacing) {
                    struct predicate p;
                    [value getValue:&p];
                    [constraints addObject:[self constraintWithItem:superview attribute:end relatedBy:p.relation
                                                             toItem:previous attribute:end multiplier:1 constant:p.constant]];
                    [[constraints lastObject] setPriority:p.priority];
                }
            } else if (previous != nil) {
                [constraints addObject:[self constraintWithItem:superview attribute:end relatedBy:NSLayoutRelationEqual
                                                         toItem:previous attribute:end multiplier:1 constant:0]];
            }
            previousIsSuperview = previous == nil;
            pendingSpacing = nil;
            pendingStandard = NO;
            continue;
        }
        if ([scanner scanString:@"-" intoString:NULL]) {
            if ([scanner scanString:@"(" intoString:NULL]) {
                pendingSpacing = PDScanPredicates(scanner, metrics, views);
                [scanner scanString:@")" intoString:NULL];
                [scanner scanString:@"-" intoString:NULL];
            } else {
                NSArray *predicates = PDScanPredicates(scanner, metrics, views);
                if ([predicates count] > 0) {
                    pendingSpacing = predicates;
                    [scanner scanString:@"-" intoString:NULL];
                } else {
                    pendingStandard = YES;
                }
            }
            continue;
        }
        if ([scanner scanString:@"[" intoString:NULL]) {
            NSString *name;
            id view;

            [scanner scanCharactersFromSet:[NSCharacterSet characterSetWithCharactersInString:
                                                               @"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"]
                                intoString:&name];
            view = [views objectForKey:name];
            if (view == nil)
                [NSException raise:NSInvalidArgumentException format:@"Unknown view %@ in visual format %@", name, format];
            if ([scanner scanString:@"(" intoString:NULL]) {
                for (NSValue *value in PDScanPredicates(scanner, metrics, views)) {
                    struct predicate p;
                    [value getValue:&p];
                    [constraints addObject:[self constraintWithItem:view attribute:size relatedBy:p.relation
                                                             toItem:p.view attribute:p.view ? size : NSLayoutAttributeNotAnAttribute
                                                         multiplier:1 constant:p.constant]];
                    [[constraints lastObject] setPriority:p.priority];
                }
                [scanner scanString:@")" intoString:NULL];
            }
            [scanner scanString:@"]" intoString:NULL];

            // the gap before this view, from the superview's edge or the previous view
            if (previous != nil || previousIsSuperview) {
                id from = previousIsSuperview ? [view superview] : previous;
                NSLayoutAttribute fromAttribute = previousIsSuperview ? start : end;
                NSArray *spacing = pendingSpacing;

                if (spacing == nil) {
                    struct predicate p = { NSLayoutRelationEqual, pendingStandard ? (previousIsSuperview ? PDStandardEdgeSpacing : PDStandardSpacing) : 0,
                                           nil, NSLayoutPriorityRequired };
                    spacing = @[ [NSValue valueWithBytes:&p objCType:@encode(struct predicate)] ];
                }
                for (NSValue *value in spacing) {
                    struct predicate p;
                    [value getValue:&p];
                    [constraints addObject:[self constraintWithItem:view attribute:start relatedBy:p.relation
                                                             toItem:from attribute:fromAttribute multiplier:1 constant:p.constant]];
                    [[constraints lastObject] setPriority:p.priority];
                }
            }
            [chain addObject:view];
            previous = view;
            previousIsSuperview = NO;
            pendingSpacing = nil;
            pendingStandard = NO;
            continue;
        }
        [NSException raise:NSInvalidArgumentException format:@"Unable to parse visual format %@", format];
    }

    // the views in the line are aligned on every attribute the options name
    for (NSLayoutAttribute attribute = NSLayoutAttributeLeft; attribute <= NSLayoutAttributeFirstBaseline; attribute++) {
        if (!(opts & (1 << attribute)) || [chain count] < 2)
            continue;
        for (NSUInteger i = 1; i < [chain count]; i++)
            [constraints addObject:[self constraintWithItem:[chain objectAtIndex:i] attribute:attribute relatedBy:NSLayoutRelationEqual
                                                     toItem:[chain objectAtIndex:0] attribute:attribute multiplier:1 constant:0]];
    }
    return constraints;
}

@end
