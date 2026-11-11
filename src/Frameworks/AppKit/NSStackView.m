// NSStackView.m - lines up its arranged subviews along one axis, without a constraint solver
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSStackView.h>
#import <AppKit/NSControl.h>
#import <AppKit/NSCell.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSValue.h>

// the 8 points macOS puts between stacked views when none is given
static const CGFloat kDefaultSpacing = 8;

@implementation NSStackView

+ (instancetype)stackViewWithViews:(NSArray *)views {
    NSStackView *stack = [[[self alloc] initWithFrame:NSZeroRect] autorelease];
    NSView *view;

    for (view in views)
        [stack addArrangedSubview:view];
    return stack;
}

- (void)_initStack {
    _arrangedSubviews = [[NSMutableArray alloc] init];
    _gravities = [[NSMutableDictionary alloc] init];
    _customSpacing = [[NSMutableDictionary alloc] init];
    _naturalSizes = [[NSMutableDictionary alloc] init];
    _orientation = NSUserInterfaceLayoutOrientationHorizontal;
    _alignment = NSLayoutAttributeCenterY;
    _spacing = kDefaultSpacing;
    _detachesHiddenViews = YES;
}

- (instancetype)initWithFrame:(NSRect)frame {
    if ((self = [super initWithFrame:frame]) != nil)
        [self _initStack];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) == nil)
        return nil;
    [self _initStack];
    if ([coder allowsKeyedCoding]) {
        NSArray *views = [coder decodeObjectForKey:@"NSStackViewArrangedSubviews"];

        _orientation = [coder decodeIntegerForKey:@"NSStackViewOrientation"];
        if ([coder containsValueForKey:@"NSStackViewAlignment"])
            _alignment = [coder decodeIntegerForKey:@"NSStackViewAlignment"];
        else
            _alignment = _orientation == NSUserInterfaceLayoutOrientationVertical ? NSLayoutAttributeCenterX : NSLayoutAttributeCenterY;
        _distribution = [coder decodeIntegerForKey:@"NSStackViewdistribution"];
        if ([coder containsValueForKey:@"NSStackViewSpacing"])
            _spacing = [coder decodeDoubleForKey:@"NSStackViewSpacing"];
        _edgeInsets.top = [coder decodeDoubleForKey:@"NSStackViewEdgeInsets.top"];
        _edgeInsets.left = [coder decodeDoubleForKey:@"NSStackViewEdgeInsets.left"];
        _edgeInsets.bottom = [coder decodeDoubleForKey:@"NSStackViewEdgeInsets.bottom"];
        _edgeInsets.right = [coder decodeDoubleForKey:@"NSStackViewEdgeInsets.right"];
        // macOS detaches hidden views of a decoded stack whatever NSStackViewDetachesHiddenViews says
        if (views != nil)
            [_arrangedSubviews addObjectsFromArray:views];
    }
    return self;
}

- (void)dealloc {
    [_arrangedSubviews release];
    [_gravities release];
    [_customSpacing release];
    [_naturalSizes release];
    [super dealloc];
}

static NSSize wantedSize(NSView *view);

// a control's cell knows the size it wants, any other view the size it had when it joined, not what a fill stretched it to
- (NSSize)_wantedSize:(NSView *)view {
    NSValue *natural = [_naturalSizes objectForKey:[NSValue valueWithNonretainedObject:view]];

    if (natural != nil && ![view isKindOfClass:[NSControl class]] && ![view isKindOfClass:[NSStackView class]])
        return [natural sizeValue];
    return wantedSize(view);
}

static NSSize wantedSize(NSView *view) {
    if ([view isKindOfClass:[NSControl class]] && [(NSControl *)view cell] != nil) {
        NSSize size = [[(NSControl *)view cell] cellSize];

        if (size.width > 0 && size.height > 0)
            return size;
    }
    if ([view isKindOfClass:[NSStackView class]])
        return [(NSStackView *)view fittingSize];
    return [view frame].size;
}

- (NSArray *)_visibleArranged {
    NSMutableArray *result = [NSMutableArray array];
    NSView *view;

    for (view in _arrangedSubviews)
        if (!_detachesHiddenViews || ![view isHidden])
            [result addObject:view];
    return result;
}

- (CGFloat)_spacingAfter:(NSView *)view {
    NSNumber *custom = [_customSpacing objectForKey:[NSValue valueWithNonretainedObject:view]];

    if (custom != nil && [custom doubleValue] != NSStackViewSpacingUseDefault)
        return [custom doubleValue];
    return _spacing;
}

- (NSSize)fittingSize {
    BOOL vertical = (_orientation == NSUserInterfaceLayoutOrientationVertical);
    NSArray *views = [self _visibleArranged];
    CGFloat along = 0, across = 0;
    NSUInteger i;

    for (i = 0; i < [views count]; i++) {
        NSSize size = [self _wantedSize:[views objectAtIndex:i]];

        along += vertical ? size.height : size.width;
        across = MAX(across, vertical ? size.width : size.height);
        if (i + 1 < [views count])
            along += [self _spacingAfter:[views objectAtIndex:i]];
    }
    if (vertical)
        return NSMakeSize(across + _edgeInsets.left + _edgeInsets.right, along + _edgeInsets.top + _edgeInsets.bottom);
    return NSMakeSize(along + _edgeInsets.left + _edgeInsets.right, across + _edgeInsets.top + _edgeInsets.bottom);
}

// the main axis gets each view's wanted length, fill and fill-equally spread the room left over, the cross axis follows the alignment
- (void)_arrange {
    BOOL vertical = (_orientation == NSUserInterfaceLayoutOrientationVertical);
    NSArray *views = [self _visibleArranged];
    NSRect bounds = [self bounds];
    // unflipped as on macOS, so a column runs down from the top edge
    CGFloat start = vertical ? NSMaxY(bounds) - _edgeInsets.top : NSMinX(bounds) + _edgeInsets.left;
    CGFloat length = vertical ? bounds.size.height - _edgeInsets.top - _edgeInsets.bottom : bounds.size.width - _edgeInsets.left - _edgeInsets.right;
    CGFloat crossStart = vertical ? NSMinX(bounds) + _edgeInsets.left : NSMinY(bounds) + _edgeInsets.bottom;
    CGFloat cross = vertical ? bounds.size.width - _edgeInsets.left - _edgeInsets.right : bounds.size.height - _edgeInsets.top - _edgeInsets.bottom;
    NSUInteger i, count = [views count];
    CGFloat used = 0, spacing = 0, extra, gap = 0, at;
    NSView *view;

    if (_arranging || count == 0)
        return;
    _arranging = YES;
    // a stack filling a window's content takes its natural size and the window follows, as Auto Layout sizes it
    if ([self window] != nil && [self superview] == [[self window] contentView] && [[[self superview] subviews] count] == 1) {
        NSSize fit = [self fittingSize];

        if (!NSEqualSizes(fit, bounds.size) && fit.width > 0 && fit.height > 0) {
            [[self window] setContentSize:fit];
            [self setFrame:NSMakeRect(0, 0, fit.width, fit.height)];
            bounds = [self bounds];
            start = vertical ? NSMaxY(bounds) - _edgeInsets.top : NSMinX(bounds) + _edgeInsets.left;
            length = vertical ? bounds.size.height - _edgeInsets.top - _edgeInsets.bottom : bounds.size.width - _edgeInsets.left - _edgeInsets.right;
            crossStart = vertical ? NSMinX(bounds) + _edgeInsets.left : NSMinY(bounds) + _edgeInsets.bottom;
            cross = vertical ? bounds.size.width - _edgeInsets.left - _edgeInsets.right : bounds.size.height - _edgeInsets.top - _edgeInsets.bottom;
        }
    }
    for (i = 0; i < count; i++) {
        NSSize size = [self _wantedSize:[views objectAtIndex:i]];

        used += vertical ? size.height : size.width;
        if (i + 1 < count)
            spacing += [self _spacingAfter:[views objectAtIndex:i]];
    }
    used += spacing;
    extra = MAX(0, length - used);
    if (_distribution == NSStackViewDistributionEqualSpacing && count > 1)
        gap = extra / (count - 1), extra = 0;
    at = start;
    for (i = 0; i < count; i++) {
        NSSize size = [self _wantedSize:[views objectAtIndex:i]];
        CGFloat main = vertical ? size.height : size.width;
        CGFloat side = vertical ? size.width : size.height;
        CGFloat offset;
        NSRect frame;

        view = [views objectAtIndex:i];
        if (_distribution == NSStackViewDistributionFillEqually)
            main = floor(MAX(0, length - spacing) / count);
        else if (_distribution == NSStackViewDistributionFill && i + 1 == count)
            main += extra;
        // leading and top (and their unflipped twins) hug the start, trailing and bottom the end, width and height fill
        switch (_alignment) {
        case NSLayoutAttributeLeft: case NSLayoutAttributeLeading: case NSLayoutAttributeBottom:
            offset = 0;
            break;
        case NSLayoutAttributeRight: case NSLayoutAttributeTrailing: case NSLayoutAttributeTop:
            offset = cross - side;
            break;
        case NSLayoutAttributeWidth: case NSLayoutAttributeHeight:
            offset = 0, side = cross;
            break;
        default:
            offset = floor((cross - side) / 2);
            break;
        }
        frame = vertical ? NSMakeRect(crossStart + offset, at - main, side, main) : NSMakeRect(at, crossStart + offset, main, side);
        [view setFrame:frame];
        if (vertical)
            at -= main + (i + 1 < count ? [self _spacingAfter:view] + gap : 0);
        else
            at += main + (i + 1 < count ? [self _spacingAfter:view] + gap : 0);
    }
    _arranging = NO;
    [self setNeedsDisplay:YES];
}

- (void)resizeSubviewsWithOldSize:(NSSize)oldSize {
    [self _arrange];
}

- (void)layout {
    [self _arrange];
}

- (void)viewWillDraw {
    [self _arrange];
    [super viewWillDraw];
}

- (NSArray *)arrangedSubviews {
    return [[_arrangedSubviews copy] autorelease];
}

- (void)insertArrangedSubview:(NSView *)view atIndex:(NSInteger)index {
    if (view == nil)
        return;
    [_arrangedSubviews removeObject:view];
    if (index < 0 || (NSUInteger)index > [_arrangedSubviews count])
        index = [_arrangedSubviews count];
    [_arrangedSubviews insertObject:view atIndex:index];
    [_naturalSizes setObject:[NSValue valueWithSize:[view frame].size] forKey:[NSValue valueWithNonretainedObject:view]];
    if ([view superview] != self)
        [self addSubview:view];
    [self _arrange];
}

- (void)addArrangedSubview:(NSView *)view {
    [self insertArrangedSubview:view atIndex:[_arrangedSubviews count]];
}

// as on macOS the view stays a subview, only its place in the line goes
- (void)removeArrangedSubview:(NSView *)view {
    [_arrangedSubviews removeObject:view];
    [_gravities removeObjectForKey:[NSValue valueWithNonretainedObject:view]];
    [_naturalSizes removeObjectForKey:[NSValue valueWithNonretainedObject:view]];
    [self _arrange];
}

- (void)willRemoveSubview:(NSView *)subview {
    [_arrangedSubviews removeObject:subview];
    [super willRemoveSubview:subview];
}

// gravity areas run leading, centre, trailing, the arranged order keeps them in that order
- (NSStackViewGravity)_gravityOf:(NSView *)view {
    NSNumber *gravity = [_gravities objectForKey:[NSValue valueWithNonretainedObject:view]];

    return gravity != nil ? [gravity integerValue] : NSStackViewGravityLeading;
}

- (NSArray *)viewsInGravity:(NSStackViewGravity)gravity {
    NSMutableArray *result = [NSMutableArray array];
    NSView *view;

    for (view in _arrangedSubviews)
        if ([self _gravityOf:view] == gravity)
            [result addObject:view];
    return result;
}

- (NSArray *)views {
    return [self arrangedSubviews];
}

- (void)insertView:(NSView *)view atIndex:(NSUInteger)index inGravity:(NSStackViewGravity)gravity {
    NSArray *same = [self viewsInGravity:gravity];
    NSUInteger at = 0, i;

    for (i = 0; i < [_arrangedSubviews count]; i++)
        if ([self _gravityOf:[_arrangedSubviews objectAtIndex:i]] <= gravity)
            at = i + 1;
    if (index < [same count])
        at = [_arrangedSubviews indexOfObject:[same objectAtIndex:index]];
    [_gravities setObject:[NSNumber numberWithInteger:gravity] forKey:[NSValue valueWithNonretainedObject:view]];
    [self insertArrangedSubview:view atIndex:at];
}

- (void)addView:(NSView *)view inGravity:(NSStackViewGravity)gravity {
    [self insertView:view atIndex:[[self viewsInGravity:gravity] count] inGravity:gravity];
}

- (void)setViews:(NSArray *)views inGravity:(NSStackViewGravity)gravity {
    NSView *view;

    for (view in [self viewsInGravity:gravity])
        [self removeView:view];
    for (view in views)
        [self addView:view inGravity:gravity];
}

- (void)removeView:(NSView *)view {
    [self removeArrangedSubview:view];
    [view removeFromSuperview];
}

- (NSUserInterfaceLayoutOrientation)orientation { return _orientation; }
- (void)setOrientation:(NSUserInterfaceLayoutOrientation)orientation { _orientation = orientation; [self _arrange]; }
- (NSLayoutAttribute)alignment { return _alignment; }
- (void)setAlignment:(NSLayoutAttribute)alignment { _alignment = alignment; [self _arrange]; }
- (NSStackViewDistribution)distribution { return _distribution; }
- (void)setDistribution:(NSStackViewDistribution)distribution { _distribution = distribution; [self _arrange]; }
- (NSEdgeInsets)edgeInsets { return _edgeInsets; }
- (void)setEdgeInsets:(NSEdgeInsets)insets { _edgeInsets = insets; [self _arrange]; }
- (CGFloat)spacing { return _spacing; }
- (void)setSpacing:(CGFloat)spacing { _spacing = spacing; [self _arrange]; }
- (BOOL)detachesHiddenViews { return _detachesHiddenViews; }
- (void)setDetachesHiddenViews:(BOOL)flag { _detachesHiddenViews = flag; [self _arrange]; }

- (CGFloat)customSpacingAfterView:(NSView *)view {
    NSNumber *custom = [_customSpacing objectForKey:[NSValue valueWithNonretainedObject:view]];

    return custom != nil ? [custom doubleValue] : NSStackViewSpacingUseDefault;
}

- (void)setCustomSpacing:(CGFloat)spacing afterView:(NSView *)view {
    [_customSpacing setObject:[NSNumber numberWithDouble:spacing] forKey:[NSValue valueWithNonretainedObject:view]];
    [self _arrange];
}

// priorities only steer a constraint solver, which this layout does not run
- (NSLayoutPriority)huggingPriorityForOrientation:(NSUserInterfaceLayoutOrientation)orientation { return 250; }
- (void)setHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSUserInterfaceLayoutOrientation)orientation { }
- (NSLayoutPriority)clippingResistancePriorityForOrientation:(NSUserInterfaceLayoutOrientation)orientation { return 1000; }
- (void)setClippingResistancePriority:(NSLayoutPriority)priority forOrientation:(NSUserInterfaceLayoutOrientation)orientation { }

@end
