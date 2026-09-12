/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePart.h>

@class NSColor, NSGradient;

/* A linear-gradient background, optionally rounded and bordered. Only linear
 * gradients: NSGradient's radial methods are still NSUnimplementedMethod. */
@interface NSInterfacePartGradient : NSInterfacePart {
    NSGradient *_gradient;
    NSColor *_borderColor;
    NSColor *_topHighlightColor;
    NSColor *_bottomBorderColor;
    CGFloat _angle;
    CGFloat _cornerRadius;
}

- initWithColors:(NSArray *)colors
           angle:(CGFloat)angle
     borderColor:(NSColor *)borderColor
    cornerRadius:(CGFloat)cornerRadius;

/* A bar wants a rule along one edge rather than a box: either edge colour may
 * be nil, and both are independent of borderColor. */
- (void)setTopHighlightColor:(NSColor *)top bottomBorderColor:(NSColor *)bottom;

@end
