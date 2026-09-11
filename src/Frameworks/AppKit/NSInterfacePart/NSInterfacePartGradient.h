/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePart.h>

@class NSColor, NSGradient;

/* A linear-gradient background, optionally rounded and bordered. Only linear
 * gradients: NSGradient's radial methods are still NSUnimplementedMethod. */
@interface NSInterfacePartGradient : NSInterfacePart {
    NSGradient *_gradient;
    NSColor *_borderColor;
    CGFloat _angle;
    CGFloat _cornerRadius;
}

- initWithColors:(NSArray *)colors
           angle:(CGFloat)angle
     borderColor:(NSColor *)borderColor
    cornerRadius:(CGFloat)cornerRadius;

@end
