/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePartFill.h>
#import <AppKit/NSBezierPath.h>
#import <AppKit/NSColor.h>
#import <AppKit/NSGraphics.h>

@implementation NSInterfacePartFill

- initWithFillColor:(NSColor *)fillColor
        borderColor:(NSColor *)borderColor
       cornerRadius:(CGFloat)cornerRadius
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    _fillColor = [fillColor retain];
    _borderColor = [borderColor retain];
    _cornerRadius = cornerRadius;
    return self;
}

- (void)dealloc
{
    [_fillColor release];
    [_borderColor release];
    [super dealloc];
}

- (void)drawInRect:(NSRect)rect
{
    if (NSIsEmptyRect(rect)) {
        return;
    }
    if (_cornerRadius <= 0.0) {
        if (_fillColor != nil) {
            [_fillColor setFill];
            NSRectFill(rect);
        }
        if (_borderColor != nil) {
            [_borderColor set];
            NSFrameRect(rect);
        }
        return;
    }

    /* Inset by half a point so the stroke lands on the pixel rather than
     * straddling two of them and coming out blurred. */
    NSRect bounds = NSInsetRect(rect, 0.5, 0.5);
    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:bounds
                                                         xRadius:_cornerRadius
                                                         yRadius:_cornerRadius];

    if (_fillColor != nil) {
        [_fillColor setFill];
        [path fill];
    }
    if (_borderColor != nil) {
        [_borderColor setStroke];
        [path stroke];
    }
}

@end
