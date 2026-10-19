// NSGlassEffectView.m - the macOS 26 glass material behind a content view, and glass containers
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSGlassEffectView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSAppearance.h>

@implementation NSGlassEffectView

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _cornerRadius = 0;
    return self;
}

- (void)dealloc
{
    [_contentView release];
    [_tintColor release];
    [super dealloc];
}

- (BOOL)isOpaque
{
    return NO;
}

- (NSView *)contentView
{
    return _contentView;
}

// the content view fills the glass and is the only subview it keeps
- (void)setContentView:(NSView *)view
{
    if (view == _contentView)
        return;
    [_contentView removeFromSuperview];
    [_contentView release];
    _contentView = [view retain];
    if (view != nil) {
        [view setFrame:[self bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:view];
    }
}

- (CGFloat)cornerRadius
{
    return _cornerRadius;
}

- (void)setCornerRadius:(CGFloat)radius
{
    _cornerRadius = radius;
    [self setNeedsDisplay:YES];
}

- (NSColor *)tintColor
{
    return _tintColor;
}

- (void)setTintColor:(NSColor *)color
{
    color = [color copy];
    [_tintColor release];
    _tintColor = color;
    [self setNeedsDisplay:YES];
}

- (NSGlassEffectViewStyle)style
{
    return _style;
}

- (void)setStyle:(NSGlassEffectViewStyle)style
{
    _style = style;
    [self setNeedsDisplay:YES];
}

// no refraction here: a translucent fill in the appearance's tone, the tint on top, and a light rim
- (void)drawRect:(NSRect)rect
{
    BOOL dark = [[[self effectiveAppearance] name] isEqualToString:NSAppearanceNameDarkAqua];
    NSBezierPath *shape = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect([self bounds], 0.5, 0.5)
                                                          xRadius:_cornerRadius yRadius:_cornerRadius];
    CGFloat alpha = _style == NSGlassEffectViewStyleClear ? 0.25 : 0.6;

    [[NSColor colorWithCalibratedWhite:dark ? 0.15 : 0.97 alpha:alpha] set];
    [shape fill];
    if (_tintColor != nil) {
        [[_tintColor colorWithAlphaComponent:[_tintColor alphaComponent] * 0.35] set];
        [shape fill];
    }
    [[NSColor colorWithCalibratedWhite:1.0 alpha:dark ? 0.18 : 0.6] set];
    [shape setLineWidth:1];
    [shape stroke];
}

@end

@implementation NSGlassEffectContainerView

- (void)dealloc
{
    [_contentView release];
    [super dealloc];
}

- (NSView *)contentView
{
    return _contentView;
}

- (void)setContentView:(NSView *)view
{
    if (view == _contentView)
        return;
    [_contentView removeFromSuperview];
    [_contentView release];
    _contentView = [view retain];
    if (view != nil) {
        [view setFrame:[self bounds]];
        [view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
        [self addSubview:view];
    }
}

- (CGFloat)spacing
{
    return _spacing;
}

// glass views closer than the spacing merge on macOS, here each keeps its own shape
- (void)setSpacing:(CGFloat)spacing
{
    _spacing = spacing;
}

@end
