/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePartGradient.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGradient.h>
#import <AppKit/NSGraphics.h>

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

- (void)dealloc
{
    [_gradient release];
    [_borderColor release];
    [super dealloc];
}

- (void)drawInRect:(NSRect)rect
{
    if (NSIsEmptyRect(rect)) {
        return;
    }
    if (_cornerRadius <= 0.0) {
        [_gradient drawInRect:rect angle:_angle];
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
