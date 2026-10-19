// NSVisualEffectView.m - a view drawn as a translucent material behind or within a window
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#import <AppKit/NSVisualEffectView.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSImage.h>
#import <AppKit/NSAppearance.h>
#import <AppKit/NSWindow.h>

@implementation NSVisualEffectView

@synthesize material = _material, blendingMode = _blendingMode, state = _state, emphasized = _emphasized;

- (instancetype)initWithFrame:(NSRect)frame
{
    if ((self = [super initWithFrame:frame]) == nil)
        return nil;
    _material = NSVisualEffectMaterialAppearanceBased;
    return self;
}

- (void)dealloc
{
    [_maskImage release];
    [super dealloc];
}

- (NSImage *)maskImage
{
    return _maskImage;
}

- (void)setMaskImage:(NSImage *)image
{
    [image retain];
    [_maskImage release];
    _maskImage = image;
    [self setNeedsDisplay:YES];
}

- (void)setMaterial:(NSVisualEffectMaterial)material
{
    _material = material;
    [self setNeedsDisplay:YES];
}

- (void)setState:(NSVisualEffectState)state
{
    _state = state;
    [self setNeedsDisplay:YES];
}

- (void)setEmphasized:(BOOL)emphasized
{
    _emphasized = emphasized;
    [self setNeedsDisplay:YES];
}

- (NSInteger)interiorBackgroundStyle
{
    return _emphasized ? 1 : 0;
}

- (BOOL)_isDark
{
    NSAppearance *appearance = [self effectiveAppearance];

    if (_material == NSVisualEffectMaterialDark || _material == NSVisualEffectMaterialUltraDark || _material == NSVisualEffectMaterialHUDWindow)
        return YES;
    if (_material == NSVisualEffectMaterialLight || _material == NSVisualEffectMaterialMediumLight)
        return NO;
    return [[appearance name] rangeOfString:@"Dark"].location != NSNotFound;
}

// the compositor has no blur for a window's own pixels, so the material is its tint, opaque enough to read
- (NSColor *)_materialColor
{
    BOOL dark = [self _isDark];
    BOOL inactive = _state == NSVisualEffectStateInactive ||
                    (_state == NSVisualEffectStateFollowsWindowActiveState && ![[self window] isKeyWindow]);
    CGFloat alpha = inactive ? 0.96 : 0.9;

    switch (_material) {
    case NSVisualEffectMaterialSelection:
        return [[NSColor keyboardFocusIndicatorColor] colorWithAlphaComponent:_emphasized ? 1.0 : 0.35];
    case NSVisualEffectMaterialUltraDark:
    case NSVisualEffectMaterialHUDWindow:
        return [NSColor colorWithCalibratedWhite:0.12 alpha:0.92];
    case NSVisualEffectMaterialSidebar:
    case NSVisualEffectMaterialUnderWindowBackground:
        return dark ? [NSColor colorWithCalibratedWhite:0.17 alpha:alpha] : [NSColor colorWithCalibratedWhite:0.93 alpha:alpha];
    case NSVisualEffectMaterialMenu:
    case NSVisualEffectMaterialPopover:
    case NSVisualEffectMaterialToolTip:
        return dark ? [NSColor colorWithCalibratedWhite:0.2 alpha:alpha] : [NSColor colorWithCalibratedWhite:0.97 alpha:alpha];
    default:
        return dark ? [NSColor colorWithCalibratedWhite:0.15 alpha:alpha] : [NSColor colorWithCalibratedWhite:0.95 alpha:alpha];
    }
}

- (void)drawRect:(NSRect)dirty
{
    [[self _materialColor] set];
    if (_maskImage != nil) {
        [_maskImage drawInRect:[self bounds] fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1.0];
        NSRectFillUsingOperation(dirty, NSCompositeSourceIn);
    } else {
        NSRectFillUsingOperation(dirty, NSCompositeSourceOver);
    }
}

- (BOOL)isOpaque
{
    return NO;
}

@end
