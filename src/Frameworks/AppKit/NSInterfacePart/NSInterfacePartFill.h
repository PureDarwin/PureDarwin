/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePart.h>

@class NSColor;

/* A flat background: the look NSGraphicsStyle drew directly before it went
 * through the theme, plus an optional corner radius. */
@interface NSInterfacePartFill : NSInterfacePart {
    NSColor *_fillColor;
    NSColor *_borderColor;
    CGFloat _cornerRadius;
}

- initWithFillColor:(NSColor *)fillColor
        borderColor:(NSColor *)borderColor
       cornerRadius:(CGFloat)cornerRadius;

@end
