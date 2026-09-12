/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePartGradient.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGradient.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSGraphicsContext.h>
#include <math.h>

@implementation NSInterfacePartGradient

- initWithColors:(NSArray *)colors
           angle:(CGFloat)angle
     borderColor:(NSColor *)borderColor
    cornerRadius:(CGFloat)cornerRadius
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    if ([colors count] >= 2) {
        _gradient = [[NSGradient alloc] initWithColors:colors];
    }
    _borderColor = [borderColor retain];
    _angle = angle;
    _cornerRadius = cornerRadius;
    return self;
}

- (void)setTopHighlightColor:(NSColor *)top bottomBorderColor:(NSColor *)bottom
{
    [top retain];
    [_topHighlightColor release];
    _topHighlightColor = top;
    [bottom retain];
    [_bottomBorderColor release];
    _bottomBorderColor = bottom;
}

- (void)dealloc
{
    [_gradient release];
    [_borderColor release];
    [_topHighlightColor release];
    [_bottomBorderColor release];
    [super dealloc];
}

- (void)drawInRect:(NSRect)rect
{
    if (NSIsEmptyRect(rect)) {
        return;
    }

    /* NSGradient's angle is in the current coordinate system, so in a flipped
     * view 270 degrees runs bottom-to-top. A theme describes how a part should
     * look, not which way the view it lands in happens to point, so mirror the
     * angle rather than make every theme author compensate. */
    BOOL flipped = [[NSGraphicsContext currentContext] isFlipped];
    CGFloat angle = flipped ? (CGFloat)fmod(360.0 - _angle, 360.0) : _angle;
    CGFloat topY = flipped ? NSMinY(rect) : NSMaxY(rect) - 1.0;
    CGFloat bottomY = flipped ? NSMaxY(rect) - 1.0 : NSMinY(rect);

    if (_cornerRadius <= 0.0) {
        [_gradient drawInRect:rect angle:angle];
        /* Edge lines are drawn separately: a full-width bar wants a rule along
         * its bottom, not a box around all four sides. */
        if (_bottomBorderColor != nil) {
            [_bottomBorderColor setFill];
            NSRectFill(NSMakeRect(NSMinX(rect), bottomY, NSWidth(rect), 1.0));
        }
        if (_topHighlightColor != nil) {
            [_topHighlightColor setFill];
            NSRectFill(NSMakeRect(NSMinX(rect), topY, NSWidth(rect), 1.0));
        }
        if (_borderColor != nil) {
            [_borderColor set];
            NSFrameRect(rect);
        }
        return;
    }

    /* Half-point inset so the stroke sits on the pixel instead of straddling
     * two and blurring. */
    NSRect bounds = NSInsetRect(rect, 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                         xRadius:_cornerRadius
                                                         yRadius:_cornerRadius];

    [_gradient drawInBezierPath:path angle:_angle];
    if (_borderColor != nil) {
        [_borderColor setStroke];
        [path stroke];
    }
}

@end
