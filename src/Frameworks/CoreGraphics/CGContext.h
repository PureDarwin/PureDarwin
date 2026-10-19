/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted,free of charge,to any person obtaining a copy of this software and associated documentation files (the "Software"),to deal in the Software without restriction,including without limitation the rights to use,copy,modify,merge,publish,distribute,sublicense,and/or sell copies of the Software,and to permit persons to whom the Software is furnished to do so,subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS",WITHOUT WARRANTY OF ANY KIND,EXPRESS OR IMPLIED,INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,DAMAGES OR OTHER LIABILITY,WHETHER IN AN ACTION OF CONTRACT,TORT OR OTHERWISE,ARISING FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CoreGraphicsExport.h>
#include <CoreFoundation/CFDictionary.h>

typedef void *CGContextRef;

#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGAffineTransform.h>
#import <CoreGraphics/CGFont.h>
#import <CoreGraphics/CGImage.h>
#import <CoreGraphics/CGColor.h>
#import <CoreGraphics/CGPath.h>
#import <CoreGraphics/CGPattern.h>
#import <CoreGraphics/CGShading.h>
#import <CoreGraphics/CGPDFPage.h>
#import <CoreGraphics/CGLayer.h>

typedef enum {
    kCGEncodingFontSpecific,
    kCGEncodingMacRoman,
} CGTextEncoding;

typedef enum {
    kCGLineCapButt,
    kCGLineCapRound,
    kCGLineCapSquare,
} CGLineCap;

typedef enum {
    kCGLineJoinMiter,
    kCGLineJoinRound,
    kCGLineJoinBevel,
} CGLineJoin;

typedef enum {
    kCGPathFill,
    kCGPathEOFill,
    kCGPathStroke,
    kCGPathFillStroke,
    kCGPathEOFillStroke
} CGPathDrawingMode;

typedef enum {
    kCGInterpolationDefault,
    kCGInterpolationNone,
    kCGInterpolationLow,
    kCGInterpolationHigh,
} CGInterpolationQuality;

typedef enum {
    // seperable
    kCGBlendModeNormal,
    kCGBlendModeMultiply,
    kCGBlendModeScreen,
    kCGBlendModeOverlay,
    kCGBlendModeDarken,
    kCGBlendModeLighten,
    kCGBlendModeColorDodge,
    kCGBlendModeColorBurn,
    kCGBlendModeHardLight,
    kCGBlendModeSoftLight,
    kCGBlendModeDifference,
    kCGBlendModeExclusion,
    // nonseperable
    kCGBlendModeHue,
    kCGBlendModeSaturation,
    kCGBlendModeColor,
    kCGBlendModeLuminosity,
    // Porter-Duff
    kCGBlendModeClear,
    kCGBlendModeCopy,
    kCGBlendModeSourceIn,
    kCGBlendModeSourceOut,
    kCGBlendModeSourceAtop,
    kCGBlendModeDestinationOver,
    kCGBlendModeDestinationIn,
    kCGBlendModeDestinationOut,
    kCGBlendModeDestinationAtop,
    kCGBlendModeXOR,
    kCGBlendModePlusDarker,
    kCGBlendModePlusLighter,
} CGBlendMode;

typedef int CGTextDrawingMode;

COREGRAPHICS_EXPORT CGContextRef CGContextRetain(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextRelease(CGContextRef context);

// context state
COREGRAPHICS_EXPORT void CGContextSetAllowsAntialiasing(CGContextRef context, bool yesOrNo);

// layers
COREGRAPHICS_EXPORT void CGContextBeginTransparencyLayer(CGContextRef context, CFDictionaryRef unused);
COREGRAPHICS_EXPORT void CGContextEndTransparencyLayer(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextBeginTransparencyLayerWithRect(CGContextRef context, CGRect rect, CFDictionaryRef auxInfo);

// path
COREGRAPHICS_EXPORT bool CGContextIsPathEmpty(CGContextRef context);
COREGRAPHICS_EXPORT CGPoint CGContextGetPathCurrentPoint(CGContextRef context);
COREGRAPHICS_EXPORT CGRect CGContextGetPathBoundingBox(CGContextRef context);
COREGRAPHICS_EXPORT bool CGContextPathContainsPoint(CGContextRef context, CGPoint point, CGPathDrawingMode pathMode);

COREGRAPHICS_EXPORT void CGContextBeginPath(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextClosePath(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextMoveToPoint(CGContextRef context, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGContextAddLineToPoint(CGContextRef context, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGContextAddCurveToPoint(CGContextRef context, CGFloat cx1, CGFloat cy1, CGFloat cx2, CGFloat cy2, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGContextAddQuadCurveToPoint(CGContextRef context, CGFloat cx1, CGFloat cy1, CGFloat x, CGFloat y);

COREGRAPHICS_EXPORT void CGContextAddLines(CGContextRef context, const CGPoint *points, unsigned count);
COREGRAPHICS_EXPORT void CGContextAddRect(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT void CGContextAddRects(CGContextRef context, const CGRect *rects, unsigned count);

COREGRAPHICS_EXPORT void CGContextAddArc(CGContextRef context, CGFloat x, CGFloat y, CGFloat radius, CGFloat startRadian, CGFloat endRadian, bool clockwise);
COREGRAPHICS_EXPORT void CGContextAddArcToPoint(CGContextRef context, CGFloat x1, CGFloat y1, CGFloat x2, CGFloat y2, CGFloat radius);
COREGRAPHICS_EXPORT void CGContextAddEllipseInRect(CGContextRef context, CGRect rect);

COREGRAPHICS_EXPORT void CGContextAddPath(CGContextRef context, CGPathRef path);

COREGRAPHICS_EXPORT void CGContextReplacePathWithStrokedPath(CGContextRef context);
COREGRAPHICS_EXPORT CGPathRef CGContextCopyPath(CGContextRef context);

// gstate

COREGRAPHICS_EXPORT void CGContextSaveGState(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextRestoreGState(CGContextRef context);

COREGRAPHICS_EXPORT CGAffineTransform CGContextGetUserSpaceToDeviceSpaceTransform(CGContextRef context);
COREGRAPHICS_EXPORT CGAffineTransform CGContextGetCTM(CGContextRef context);
COREGRAPHICS_EXPORT CGRect CGContextGetClipBoundingBox(CGContextRef context);
COREGRAPHICS_EXPORT CGAffineTransform CGContextGetTextMatrix(CGContextRef context);
COREGRAPHICS_EXPORT CGInterpolationQuality CGContextGetInterpolationQuality(CGContextRef context);
COREGRAPHICS_EXPORT CGPoint CGContextGetTextPosition(CGContextRef context);

COREGRAPHICS_EXPORT CGPoint CGContextConvertPointToDeviceSpace(CGContextRef context, CGPoint point);
COREGRAPHICS_EXPORT CGPoint CGContextConvertPointToUserSpace(CGContextRef context, CGPoint point);
COREGRAPHICS_EXPORT CGSize CGContextConvertSizeToDeviceSpace(CGContextRef context, CGSize size);
COREGRAPHICS_EXPORT CGSize CGContextConvertSizeToUserSpace(CGContextRef context, CGSize size);
COREGRAPHICS_EXPORT CGRect CGContextConvertRectToDeviceSpace(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT CGRect CGContextConvertRectToUserSpace(CGContextRef context, CGRect rect);

COREGRAPHICS_EXPORT void CGContextConcatCTM(CGContextRef context, CGAffineTransform matrix);
COREGRAPHICS_EXPORT void CGContextTranslateCTM(CGContextRef context, CGFloat translatex, CGFloat translatey);
COREGRAPHICS_EXPORT void CGContextScaleCTM(CGContextRef context, CGFloat scalex, CGFloat scaley);
COREGRAPHICS_EXPORT void CGContextRotateCTM(CGContextRef context, CGFloat radians);

COREGRAPHICS_EXPORT void CGContextClip(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextEOClip(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextClipToMask(CGContextRef context, CGRect rect, CGImageRef image);
COREGRAPHICS_EXPORT void CGContextClipToRect(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT void CGContextClipToRects(CGContextRef context, const CGRect *rects, unsigned count);

COREGRAPHICS_EXPORT void CGContextSetStrokeColorSpace(CGContextRef context, CGColorSpaceRef colorSpace);
COREGRAPHICS_EXPORT void CGContextSetFillColorSpace(CGContextRef context, CGColorSpaceRef colorSpace);

COREGRAPHICS_EXPORT void CGContextSetStrokeColor(CGContextRef context, const CGFloat *components);
COREGRAPHICS_EXPORT void CGContextSetStrokeColorWithColor(CGContextRef context, CGColorRef color);
COREGRAPHICS_EXPORT void CGContextSetGrayStrokeColor(CGContextRef context, CGFloat gray, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetRGBStrokeColor(CGContextRef context, CGFloat r, CGFloat g, CGFloat b, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCMYKStrokeColor(CGContextRef context, CGFloat c, CGFloat m, CGFloat y, CGFloat k, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCalibratedRGBStrokeColor(CGContextRef context, CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCalibratedGrayStrokeColor(CGContextRef context, CGFloat gray, CGFloat alpha);

COREGRAPHICS_EXPORT void CGContextSetFillColor(CGContextRef context, const CGFloat *components);
COREGRAPHICS_EXPORT void CGContextSetFillColorWithColor(CGContextRef context, CGColorRef color);
COREGRAPHICS_EXPORT void CGContextSetGrayFillColor(CGContextRef context, CGFloat gray, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetRGBFillColor(CGContextRef context, CGFloat r, CGFloat g, CGFloat b, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCMYKFillColor(CGContextRef context, CGFloat c, CGFloat m, CGFloat y, CGFloat k, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCalibratedGrayFillColor(CGContextRef context, CGFloat gray, CGFloat alpha);
COREGRAPHICS_EXPORT void CGContextSetCalibratedRGBFillColor(CGContextRef context, CGFloat red, CGFloat green, CGFloat blue, CGFloat alpha);

COREGRAPHICS_EXPORT void CGContextSetAlpha(CGContextRef context, CGFloat alpha);

COREGRAPHICS_EXPORT void CGContextSetPatternPhase(CGContextRef context, CGSize phase);
COREGRAPHICS_EXPORT void CGContextSetStrokePattern(CGContextRef context, CGPatternRef pattern, const CGFloat *components);
COREGRAPHICS_EXPORT void CGContextSetFillPattern(CGContextRef context, CGPatternRef pattern, const CGFloat *components);

COREGRAPHICS_EXPORT void CGContextSetTextMatrix(CGContextRef context, CGAffineTransform matrix);

COREGRAPHICS_EXPORT void CGContextSetTextPosition(CGContextRef context, CGFloat x, CGFloat y);
COREGRAPHICS_EXPORT void CGContextSetCharacterSpacing(CGContextRef context, CGFloat spacing);
COREGRAPHICS_EXPORT void CGContextSetTextDrawingMode(CGContextRef context, CGTextDrawingMode textMode);

COREGRAPHICS_EXPORT void CGContextSetFont(CGContextRef context, CGFontRef font);
COREGRAPHICS_EXPORT void CGContextSetFontSize(CGContextRef context, CGFloat size);
COREGRAPHICS_EXPORT void CGContextSelectFont(CGContextRef context, const char *name, CGFloat size, CGTextEncoding encoding);
COREGRAPHICS_EXPORT void CGContextSetShouldSmoothFonts(CGContextRef context, bool yesOrNo);
COREGRAPHICS_EXPORT void CGContextSetShouldSubpixelQuantizeFonts(CGContextRef context, bool yesOrNo);
COREGRAPHICS_EXPORT void CGContextSetAllowsFontSubpixelQuantization(CGContextRef context, bool yesOrNo);
COREGRAPHICS_EXPORT void CGContextSetShouldSubpixelPositionFonts(CGContextRef context, bool yesOrNo);
COREGRAPHICS_EXPORT void CGContextSetAllowsFontSubpixelPositioning(CGContextRef context, bool yesOrNo);
COREGRAPHICS_EXPORT void CGContextSetBaseCTM(CGContextRef context, CGAffineTransform matrix);

COREGRAPHICS_EXPORT void CGContextSetLineWidth(CGContextRef context, CGFloat width);
COREGRAPHICS_EXPORT void CGContextSetLineCap(CGContextRef context, CGLineCap lineCap);
COREGRAPHICS_EXPORT void CGContextSetLineJoin(CGContextRef context, CGLineJoin lineJoin);
COREGRAPHICS_EXPORT void CGContextSetMiterLimit(CGContextRef context, CGFloat miterLimit);
COREGRAPHICS_EXPORT void CGContextSetLineDash(CGContextRef context, CGFloat phase, const CGFloat *lengths, size_t count);

COREGRAPHICS_EXPORT void CGContextSetRenderingIntent(CGContextRef context, CGColorRenderingIntent renderingIntent);
COREGRAPHICS_EXPORT void CGContextSetBlendMode(CGContextRef context, CGBlendMode blendMode);

COREGRAPHICS_EXPORT void CGContextSetFlatness(CGContextRef context, CGFloat flatness);

COREGRAPHICS_EXPORT void CGContextSetInterpolationQuality(CGContextRef context, CGInterpolationQuality quality);

COREGRAPHICS_EXPORT void CGContextSetShadowWithColor(CGContextRef context, CGSize offset, CGFloat blur, CGColorRef color);
COREGRAPHICS_EXPORT void CGContextSetShadow(CGContextRef context, CGSize offset, CGFloat blur);

COREGRAPHICS_EXPORT void CGContextSetShouldAntialias(CGContextRef context, bool yesOrNo);

// drawing
COREGRAPHICS_EXPORT void CGContextStrokeLineSegments(CGContextRef context, const CGPoint *points, unsigned count);

COREGRAPHICS_EXPORT void CGContextStrokeRect(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT void CGContextStrokeRectWithWidth(CGContextRef context, CGRect rect, CGFloat width);
COREGRAPHICS_EXPORT void CGContextStrokeEllipseInRect(CGContextRef context, CGRect rect);

COREGRAPHICS_EXPORT void CGContextFillRect(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT void CGContextFillRects(CGContextRef context, const CGRect *rects, unsigned count);
COREGRAPHICS_EXPORT void CGContextFillEllipseInRect(CGContextRef context, CGRect rect);

COREGRAPHICS_EXPORT void CGContextDrawPath(CGContextRef context, CGPathDrawingMode pathMode);
COREGRAPHICS_EXPORT void CGContextStrokePath(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextFillPath(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextEOFillPath(CGContextRef context);

COREGRAPHICS_EXPORT void CGContextClearRect(CGContextRef context, CGRect rect);

COREGRAPHICS_EXPORT void CGContextShowGlyphs(CGContextRef context, const CGGlyph *glyphs, unsigned count);
COREGRAPHICS_EXPORT void CGContextShowGlyphsAtPoint(CGContextRef context, CGFloat x, CGFloat y, const CGGlyph *glyphs, unsigned count);
COREGRAPHICS_EXPORT void CGContextShowGlyphsWithAdvances(CGContextRef context, const CGGlyph *glyphs, const CGSize *advances, unsigned count);

COREGRAPHICS_EXPORT void CGContextShowText(CGContextRef context, const char *text, unsigned count);
COREGRAPHICS_EXPORT void CGContextShowTextAtPoint(CGContextRef context, CGFloat x, CGFloat y, const char *text, unsigned count);

COREGRAPHICS_EXPORT void CGContextDrawShading(CGContextRef context, CGShadingRef shading);
COREGRAPHICS_EXPORT void CGContextDrawImage(CGContextRef context, CGRect rect, CGImageRef image);
COREGRAPHICS_EXPORT void CGContextDrawLayerAtPoint(CGContextRef context, CGPoint point, CGLayerRef layer);
COREGRAPHICS_EXPORT void CGContextDrawLayerInRect(CGContextRef context, CGRect rect, CGLayerRef layer);
COREGRAPHICS_EXPORT void CGContextDrawPDFPage(CGContextRef context, CGPDFPageRef page);

COREGRAPHICS_EXPORT void CGContextFlush(CGContextRef context);
COREGRAPHICS_EXPORT void CGContextSynchronize(CGContextRef context);

// pagination

COREGRAPHICS_EXPORT void CGContextBeginPage(CGContextRef context, const CGRect *mediaBox);
COREGRAPHICS_EXPORT void CGContextEndPage(CGContextRef context);

// **PRIVATE** These are private in Apple's implementation as well as ours.

COREGRAPHICS_EXPORT void CGContextSetCTM(CGContextRef context, CGAffineTransform matrix);
COREGRAPHICS_EXPORT void CGContextResetClip(CGContextRef context);

// Temporary hacks

COREGRAPHICS_EXPORT CFDataRef CGContextCaptureBitmap(CGContextRef context, CGRect rect);
COREGRAPHICS_EXPORT void CGContextCopyBits(CGContextRef context, CGRect rect, CGPoint point, int gState);
COREGRAPHICS_EXPORT bool CGContextSupportsGlobalAlpha(CGContextRef context);
COREGRAPHICS_EXPORT bool CGContextIsBitmapContext(CGContextRef context);
