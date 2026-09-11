/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePart.h>

@class NSImage;

/* A nine-part (nine-slice) image: the four corners are drawn at their natural
   size, the four edges stretch along one axis and the middle stretches along
   both. This is what lets a theme ship artwork for a control of any size
   rather than only a procedural gradient.

   Insets give the width of the corner pieces, in pixels of the source image. */
@interface NSInterfacePartImage : NSInterfacePart {
    NSImage *_image;
    CGFloat _leftInset;
    CGFloat _rightInset;
    CGFloat _topInset;
    CGFloat _bottomInset;
}

- initWithImage:(NSImage *)image
      leftInset:(CGFloat)leftInset
     rightInset:(CGFloat)rightInset
       topInset:(CGFloat)topInset
    bottomInset:(CGFloat)bottomInset;

@end
