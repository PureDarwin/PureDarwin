/* Copyright (c) 2026 PureDarwin. Distributed under the terms of the MIT
   licence that accompanies the rest of this framework. */

#import <AppKit/NSInterfacePartImage.h>
#import <AppKit/NSGraphics.h>
#import <AppKit/NSImage.h>

@implementation NSInterfacePartImage

- initWithImage:(NSImage *)image
      leftInset:(CGFloat)leftInset
     rightInset:(CGFloat)rightInset
       topInset:(CGFloat)topInset
    bottomInset:(CGFloat)bottomInset
{
    if ((self = [super init]) == nil) {
        return nil;
    }
    _image = [image retain];
    _leftInset = MAX(0.0, leftInset);
    _rightInset = MAX(0.0, rightInset);
    _topInset = MAX(0.0, topInset);
    _bottomInset = MAX(0.0, bottomInset);
    return self;
}

- (void)dealloc
{
    [_image release];
    [super dealloc];
}

- (NSSize)size
{
    return (_image != nil) ? [_image size] : NSZeroSize;
}

static void drawPiece(NSImage *image, NSRect source, NSRect destination)
{
    if (NSIsEmptyRect(source) || NSIsEmptyRect(destination)) {
        return;
    }
    [image drawInRect:destination
             fromRect:source
            operation:NSCompositeSourceOver
             fraction:1.0];
}

- (void)drawInRect:(NSRect)rect
{
    if (_image == nil || NSIsEmptyRect(rect)) {
        return;
    }

    NSSize source = [_image size];

    if (source.width <= 0.0 || source.height <= 0.0) {
        return;
    }

    CGFloat left = _leftInset;
    CGFloat right = _rightInset;
    CGFloat top = _topInset;
    CGFloat bottom = _bottomInset;

    /* Corners cannot overlap: with insets wider than the destination there is
     * no middle to stretch, and drawing them anyway would mirror the artwork
     * back on itself. Scale the pair down to fit instead. */
    if (left + right > rect.size.width) {
        CGFloat scale = rect.size.width / (left + right);

        left *= scale;
        right *= scale;
    }
    if (top + bottom > rect.size.height) {
        CGFloat scale = rect.size.height / (top + bottom);

        top *= scale;
        bottom *= scale;
    }

    /* Source insets stay at their natural size; only the destination is
     * clamped, so the corner artwork is never resampled. */
    CGFloat sLeft = MIN(_leftInset, source.width);
    CGFloat sRight = MIN(_rightInset, source.width - sLeft);
    CGFloat sBottom = MIN(_bottomInset, source.height);
    CGFloat sTop = MIN(_topInset, source.height - sBottom);

    CGFloat sMidW = source.width - sLeft - sRight;
    CGFloat sMidH = source.height - sBottom - sTop;
    CGFloat dMidW = rect.size.width - left - right;
    CGFloat dMidH = rect.size.height - bottom - top;

    CGFloat sx = NSMinX(rect);
    CGFloat sy = NSMinY(rect);

    /* Source coordinates are bottom-left origin, matching NSImage. */
    drawPiece(_image, NSMakeRect(0.0, 0.0, sLeft, sBottom),
              NSMakeRect(sx, sy, left, bottom));
    drawPiece(_image, NSMakeRect(source.width - sRight, 0.0, sRight, sBottom),
              NSMakeRect(sx + rect.size.width - right, sy, right, bottom));
    drawPiece(_image, NSMakeRect(0.0, source.height - sTop, sLeft, sTop),
              NSMakeRect(sx, sy + rect.size.height - top, left, top));
    drawPiece(_image, NSMakeRect(source.width - sRight, source.height - sTop, sRight, sTop),
              NSMakeRect(sx + rect.size.width - right, sy + rect.size.height - top, right, top));

    drawPiece(_image, NSMakeRect(sLeft, 0.0, sMidW, sBottom),
              NSMakeRect(sx + left, sy, dMidW, bottom));
    drawPiece(_image, NSMakeRect(sLeft, source.height - sTop, sMidW, sTop),
              NSMakeRect(sx + left, sy + rect.size.height - top, dMidW, top));
    drawPiece(_image, NSMakeRect(0.0, sBottom, sLeft, sMidH),
              NSMakeRect(sx, sy + bottom, left, dMidH));
    drawPiece(_image, NSMakeRect(source.width - sRight, sBottom, sRight, sMidH),
              NSMakeRect(sx + rect.size.width - right, sy + bottom, right, dMidH));

    drawPiece(_image, NSMakeRect(sLeft, sBottom, sMidW, sMidH),
              NSMakeRect(sx + left, sy + bottom, dMidW, dMidH));
}

@end
