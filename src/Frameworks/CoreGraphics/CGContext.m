/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted,free of charge,to any person obtaining a copy of this software and associated documentation files (the "Software"),to deal in the Software without restriction,including without limitation the rights to use,copy,modify,merge,publish,distribute,sublicense,and/or sell copies of the Software,and to permit persons to whom the Software is furnished to do so,subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS",WITHOUT WARRANTY OF ANY KIND,EXPRESS OR IMPLIED,INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,DAMAGES OR OTHER LIABILITY,WHETHER IN AN ACTION OF CONTRACT,TORT OR OTHERWISE,ARISING FROM,OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <CoreGraphics/CGContext.h>
#import <Onyx2D/O2Context.h>
#import <Onyx2D/O2MutablePath.h>
#import "CGConversions.h"

CGContextRef CGContextRetain(CGContextRef context) {
  return O2ContextRetain(context);
}

void CGContextRelease(CGContextRef context) {
   O2ContextRelease(context);
}

void CGContextSetAllowsAntialiasing(CGContextRef context,bool yesOrNo) {
   O2ContextSetAllowsAntialiasing(context,yesOrNo);
}

void CGContextBeginTransparencyLayer(CGContextRef context,CFDictionaryRef unused) {
   O2ContextBeginTransparencyLayer(context,(NSDictionary *)unused);
}

void CGContextEndTransparencyLayer(CGContextRef context) {
   O2ContextEndTransparencyLayer(context);
}

bool CGContextIsPathEmpty(CGContextRef context) {
   return O2ContextIsPathEmpty(context);
}

CGPoint CGContextGetPathCurrentPoint(CGContextRef context) {
   return O2ContextGetPathCurrentPoint(context);
}

CGRect CGContextGetPathBoundingBox(CGContextRef context) {
   return O2ContextGetPathBoundingBox(context);
}

bool CGContextSupportsGlobalAlpha(CGContextRef context) {
	return O2ContextSupportsGlobalAlpha(context);
}

bool CGContextIsBitmapContext(CGContextRef context) {
	return O2ContextIsBitmapContext(context);
}

bool CGContextPathContainsPoint(CGContextRef context,CGPoint point,CGPathDrawingMode pathMode) {
	return O2ContextPathContainsPoint(context,point,pathMode);
}

void CGContextBeginPath(CGContextRef context) {
   O2ContextBeginPath(context);
}

void CGContextClosePath(CGContextRef context) {
   O2ContextClosePath(context);
}

void CGContextMoveToPoint(CGContextRef context,CGFloat x,CGFloat y) {
   O2ContextMoveToPoint(context,x,y);
}

void CGContextAddLineToPoint(CGContextRef context,CGFloat x,CGFloat y) {
   O2ContextAddLineToPoint(context,x,y);
}

void CGContextAddCurveToPoint(CGContextRef context,CGFloat cx1,CGFloat cy1,CGFloat cx2,CGFloat cy2,CGFloat x,CGFloat y) {
   O2ContextAddCurveToPoint(context,cx1,cy1,cx2,cy2,x,y);
}

void CGContextAddQuadCurveToPoint(CGContextRef context,CGFloat cx1,CGFloat cy1,CGFloat x,CGFloat y) {
   O2ContextAddQuadCurveToPoint(context,cx1,cy1,x,y);
}

void CGContextAddLines(CGContextRef context,const CGPoint *points,unsigned count) {
   O2ContextAddLines(context,points,count);
}

void CGContextAddRect(CGContextRef context,CGRect rect) {
   O2ContextAddRect(context,rect);
}

void CGContextAddRects(CGContextRef context,const CGRect *rects,unsigned count) {
   O2ContextAddRects(context,rects,count);
}

void CGContextAddArc(CGContextRef context,CGFloat x,CGFloat y,CGFloat radius,CGFloat startRadian,CGFloat endRadian,bool clockwise) {
   O2ContextAddArc(context,x,y,radius,startRadian,endRadian,clockwise);
}

void CGContextAddArcToPoint(CGContextRef context,CGFloat x1,CGFloat y1,CGFloat x2,CGFloat y2,CGFloat radius) {
   O2ContextAddArcToPoint(context,x1,y1,x2,y2,radius);
}

void CGContextAddEllipseInRect(CGContextRef context,CGRect rect) {
   O2ContextAddEllipseInRect(context,rect);
}

void CGContextAddPath(CGContextRef context,CGPathRef path) {
   O2ContextAddPath(context,path);
}

void CGContextReplacePathWithStrokedPath(CGContextRef context) {
   O2ContextReplacePathWithStrokedPath(context);
}

CGPathRef CGContextCopyPath(CGContextRef context)
{
    return O2ContextCopyPath(context);
}

void CGContextSaveGState(CGContextRef context){
   O2ContextSaveGState(context);
}

void CGContextRestoreGState(CGContextRef context){
   O2ContextRestoreGState(context);
}

CGAffineTransform CGContextGetUserSpaceToDeviceSpaceTransform(CGContextRef context) {
   return CGAffineTransformFromO2(O2ContextGetUserSpaceToDeviceSpaceTransform(context));
}

CGAffineTransform CGContextGetCTM(CGContextRef context) {
   return CGAffineTransformFromO2(O2ContextGetCTM(context));
}

CGRect CGContextGetClipBoundingBox(CGContextRef context) {
   return O2ContextGetClipBoundingBox(context);
}

CGAffineTransform CGContextGetTextMatrix(CGContextRef context) {
   return CGAffineTransformFromO2(O2ContextGetTextMatrix(context));
}

CGInterpolationQuality CGContextGetInterpolationQuality(CGContextRef context) {
   return O2ContextGetInterpolationQuality(context);
}

CGPoint CGContextGetTextPosition(CGContextRef context) {
   return O2ContextGetTextPosition(context);
}

CGPoint CGContextConvertPointToDeviceSpace(CGContextRef context,CGPoint point) {
   return O2ContextConvertPointToDeviceSpace(context,point);
}

CGPoint CGContextConvertPointToUserSpace(CGContextRef context,CGPoint point) {
   return O2ContextConvertPointToUserSpace(context,point);
}

CGSize CGContextConvertSizeToDeviceSpace(CGContextRef context,CGSize size) {
   return O2ContextConvertSizeToDeviceSpace(context,size);
}

CGSize CGContextConvertSizeToUserSpace(CGContextRef context,CGSize size) {
   return O2ContextConvertSizeToUserSpace(context,size);
}

CGRect CGContextConvertRectToDeviceSpace(CGContextRef context,CGRect rect) {
   return O2ContextConvertRectToDeviceSpace(context,rect);
}

CGRect CGContextConvertRectToUserSpace(CGContextRef context,CGRect rect) {
   return O2ContextConvertRectToUserSpace(context,rect);
}

void CGContextSetCTM(CGContextRef context,CGAffineTransform matrix) {
   O2ContextSetCTM(context,O2AffineTransformFromCG(matrix));
}

void CGContextConcatCTM(CGContextRef context,CGAffineTransform matrix) {
   O2ContextConcatCTM(context,O2AffineTransformFromCG(matrix));
}

void CGContextTranslateCTM(CGContextRef context,CGFloat tx,CGFloat ty){
   O2ContextTranslateCTM(context,tx,ty);
}

void CGContextScaleCTM(CGContextRef context,CGFloat scalex,CGFloat scaley){
   O2ContextScaleCTM(context,scalex,scaley);
}

void CGContextRotateCTM(CGContextRef context,CGFloat radians) {
   O2ContextRotateCTM(context,radians);
}

void CGContextClip(CGContextRef context) {
   O2ContextClip(context);
}

void CGContextEOClip(CGContextRef context) {
   O2ContextEOClip(context);
}

void CGContextClipToMask(CGContextRef context,CGRect rect,CGImageRef image) {
   O2ContextClipToMask(context,rect,image);
}

void CGContextClipToRect(CGContextRef context,CGRect rect){
   O2ContextClipToRect(context,rect);
}

void CGContextClipToRects(CGContextRef context,const CGRect *rects,unsigned count) {
   O2ContextClipToRects(context,rects,count);
}

void CGContextSetStrokeColorSpace(CGContextRef context,CGColorSpaceRef colorSpace) {
   O2ContextSetStrokeColorSpace(context,colorSpace);
}

void CGContextSetFillColorSpace(CGContextRef context,CGColorSpaceRef colorSpace) {
   O2ContextSetFillColorSpace(context,colorSpace);
}

void CGContextSetStrokeColor(CGContextRef context,const CGFloat *components) {
   O2ContextSetStrokeColor(context,components);
}

void CGContextSetStrokeColorWithColor(CGContextRef context,CGColorRef color) {
   O2ContextSetStrokeColorWithColor(context,color);
}

void CGContextSetGrayStrokeColor(CGContextRef context,CGFloat gray,CGFloat alpha) {
   O2ContextSetGrayStrokeColor(context,gray,alpha);
}

void CGContextSetRGBStrokeColor(CGContextRef context,CGFloat r,CGFloat g,CGFloat b,CGFloat alpha) {
   O2ContextSetRGBStrokeColor(context,r,g,b,alpha);
}

void CGContextSetCMYKStrokeColor(CGContextRef context,CGFloat c,CGFloat m,CGFloat y,CGFloat k,CGFloat alpha) {
   O2ContextSetCMYKStrokeColor(context,c,m,y,k,alpha);
}

void CGContextSetFillColor(CGContextRef context,const CGFloat *components) {
   O2ContextSetFillColor(context,components);
}

void CGContextSetFillColorWithColor(CGContextRef context,CGColorRef color) {
   O2ContextSetFillColorWithColor(context,color);
}

void CGContextSetGrayFillColor(CGContextRef context,CGFloat gray,CGFloat alpha) {
   O2ContextSetGrayFillColor(context,gray,alpha);
}

void CGContextSetRGBFillColor(CGContextRef context,CGFloat r,CGFloat g,CGFloat b,CGFloat alpha) {
   O2ContextSetRGBFillColor(context,r,g,b,alpha);
}

void CGContextSetCMYKFillColor(CGContextRef context,CGFloat c,CGFloat m,CGFloat y,CGFloat k,CGFloat alpha) {
   O2ContextSetCMYKFillColor(context,c,m,y,k,alpha);
}

void CGContextSetAlpha(CGContextRef context,CGFloat alpha) {
   O2ContextSetAlpha(context,alpha);
}

void CGContextSetPatternPhase(CGContextRef context,CGSize phase) {
   O2ContextSetPatternPhase(context,phase);
}

void CGContextSetStrokePattern(CGContextRef context,CGPatternRef pattern,const CGFloat *components) {
   O2ContextSetStrokePattern(context,pattern,components);
}

void CGContextSetFillPattern(CGContextRef context,CGPatternRef pattern,const CGFloat *components) {
   O2ContextSetFillPattern(context,pattern,components);
}

void CGContextSetTextMatrix(CGContextRef context,CGAffineTransform matrix) {
   O2ContextSetTextMatrix(context,O2AffineTransformFromCG(matrix));
}

void CGContextSetTextPosition(CGContextRef context,CGFloat x,CGFloat y) {
   O2ContextSetTextPosition(context,x,y);
}

void CGContextSetCharacterSpacing(CGContextRef context,CGFloat spacing) {
   O2ContextSetCharacterSpacing(context,spacing);
}

void CGContextSetTextDrawingMode(CGContextRef context,CGTextDrawingMode textMode) {
   O2ContextSetTextDrawingMode(context,textMode);
}

void CGContextSetFont(CGContextRef context,CGFontRef font) {
   O2ContextSetFont(context,font);
}

void CGContextSetFontSize(CGContextRef context,CGFloat size) {
   O2ContextSetFontSize(context,size);
}

void CGContextSelectFont(CGContextRef context,const char *name,CGFloat size,CGTextEncoding encoding) {
   O2ContextSelectFont(context,name,size,encoding);
}

void CGContextSetShouldSmoothFonts(CGContextRef context,bool yesOrNo) {
   O2ContextSetShouldSmoothFonts(context,yesOrNo);
}

// glyph positions are not quantized by Onyx2D's text path either way, so there is nothing to switch
void CGContextSetShouldSubpixelQuantizeFonts(CGContextRef context,bool yesOrNo) {
}

void CGContextSetAllowsFontSubpixelQuantization(CGContextRef context,bool yesOrNo) {
}

// Onyx2D places glyphs at their exact positions, so subpixel positioning is always on
void CGContextSetShouldSubpixelPositionFonts(CGContextRef context,bool yesOrNo) {
}

void CGContextSetAllowsFontSubpixelPositioning(CGContextRef context,bool yesOrNo) {
}

void CGContextSetBaseCTM(CGContextRef context,CGAffineTransform matrix) {
   O2ContextSetBaseCTM(context,O2AffineTransformFromCG(matrix));
}

// the layer covers the whole clip, so bounding it by the rect only clips drawing to it
void CGContextBeginTransparencyLayerWithRect(CGContextRef context,CGRect rect,CFDictionaryRef auxInfo) {
   O2ContextBeginTransparencyLayer(context,(NSDictionary *)auxInfo);
   O2ContextClipToRect(context,rect);
}

void CGContextSetLineWidth(CGContextRef context,CGFloat width) {
   O2ContextSetLineWidth(context,width);
}

void CGContextSetLineCap(CGContextRef context,CGLineCap lineCap) {
   O2ContextSetLineCap(context,lineCap);
}

void CGContextSetLineJoin(CGContextRef context,CGLineJoin lineJoin) {
   O2ContextSetLineJoin(context,lineJoin);
}

void CGContextSetMiterLimit(CGContextRef context,CGFloat miterLimit) {
   O2ContextSetMiterLimit(context,miterLimit);
}

// Onyx2D keeps single precision, the public API takes CGFloat like every macOS caller
void CGContextSetLineDash(CGContextRef context,CGFloat phase,const CGFloat *lengths,size_t count) {
   O2Float narrow[count>0 ? count : 1];

   for(size_t i=0;i<count;i++)
    narrow[i]=lengths[i];
   O2ContextSetLineDash(context,phase,lengths ? narrow : NULL,(unsigned)count);
}

void CGContextSetRenderingIntent(CGContextRef context,CGColorRenderingIntent renderingIntent) {
   O2ContextSetRenderingIntent(context,renderingIntent);
}

void CGContextSetBlendMode(CGContextRef context,CGBlendMode blendMode) {
   O2ContextSetBlendMode(context,blendMode);
}

void CGContextSetFlatness(CGContextRef context,CGFloat flatness) {
   O2ContextSetFlatness(context,flatness);
}

void CGContextSetInterpolationQuality(CGContextRef context,CGInterpolationQuality quality) {
   O2ContextSetInterpolationQuality(context,quality);
} 

void CGContextSetShadowWithColor(CGContextRef context,CGSize offset,CGFloat blur,CGColorRef color) {
   O2ContextSetShadowWithColor(context,offset,blur,color);
}

void CGContextSetShadow(CGContextRef context,CGSize offset,CGFloat blur) {
   O2ContextSetShadow(context,offset,blur);
}

void CGContextSetShouldAntialias(CGContextRef context,bool yesOrNo) {
   O2ContextSetShouldAntialias(context,yesOrNo);
}

void CGContextStrokeLineSegments(CGContextRef context,const CGPoint *points,unsigned count) {
   O2ContextStrokeLineSegments(context,points,count);
}

void CGContextStrokeRect(CGContextRef context,CGRect rect) {
   O2ContextStrokeRect(context,rect);
}

void CGContextStrokeRectWithWidth(CGContextRef context,CGRect rect,CGFloat width) {
   O2ContextStrokeRectWithWidth(context,rect,width);
}

void CGContextStrokeEllipseInRect(CGContextRef context,CGRect rect) {
   O2ContextStrokeEllipseInRect(context,rect);
}

void CGContextFillRect(CGContextRef context,CGRect rect) {
   O2ContextFillRect(context,rect);
}

void CGContextFillRects(CGContextRef context,const CGRect *rects,unsigned count) {
   O2ContextFillRects(context,rects,count);
}

void CGContextFillEllipseInRect(CGContextRef context,CGRect rect) {
   O2ContextFillEllipseInRect(context,rect);
}

void CGContextDrawPath(CGContextRef context,CGPathDrawingMode pathMode) {
   O2ContextDrawPath(context,pathMode);
}

void CGContextStrokePath(CGContextRef context) {
   O2ContextStrokePath(context);
}

void CGContextFillPath(CGContextRef context) {
   O2ContextFillPath(context);
}

void CGContextEOFillPath(CGContextRef context) {
   O2ContextEOFillPath(context);
}

void CGContextClearRect(CGContextRef context,CGRect rect) {
   O2ContextClearRect(context,rect);
}

void CGContextShowGlyphs(CGContextRef context,const CGGlyph *glyphs,unsigned count) {
   O2ContextShowGlyphs(context,glyphs,count);
}

void CGContextShowGlyphsAtPoint(CGContextRef context,CGFloat x,CGFloat y,const CGGlyph *glyphs,unsigned count) {
   O2ContextShowGlyphsAtPoint(context,x,y,glyphs,count);
}

void CGContextShowGlyphsWithAdvances(CGContextRef context,const CGGlyph *glyphs,const CGSize *advances,unsigned count) {
   O2ContextShowGlyphsWithAdvances(context,glyphs,advances,count);
}

void CGContextShowText(CGContextRef context,const char *text,unsigned count) {
   O2ContextShowText(context,text,count);
}

void CGContextShowTextAtPoint(CGContextRef context,CGFloat x,CGFloat y,const char *text,unsigned count) {
   O2ContextShowTextAtPoint(context,x,y,text,count);
}

void CGContextDrawShading(CGContextRef context,CGShadingRef shading) {
   O2ContextDrawShading(context,shading);
}  

void CGContextDrawImage(CGContextRef context,CGRect rect,CGImageRef image) {
   O2ContextDrawImage(context,rect,image);
}

void CGContextDrawLayerAtPoint(CGContextRef context,CGPoint point,CGLayerRef layer) {
   O2ContextDrawLayerAtPoint(context,point,layer);
}

void CGContextDrawLayerInRect(CGContextRef context,CGRect rect,CGLayerRef layer) {
   O2ContextDrawLayerInRect(context,rect,layer);
}

void CGContextDrawPDFPage(CGContextRef context,CGPDFPageRef page) {
   O2ContextDrawPDFPage(context,page);
}

void CGContextFlush(CGContextRef context) {
   O2ContextFlush(context);
}

void CGContextSynchronize(CGContextRef context) {
   O2ContextSynchronize(context);
}

void CGContextBeginPage(CGContextRef context,const CGRect *mediaBox) {
   O2ContextBeginPage(context,mediaBox);
}

void CGContextEndPage(CGContextRef context) {
   O2ContextEndPage(context);
}

/// temporary hacks

void CGContextResetClip(CGContextRef context) {
   O2ContextResetClip(context);
}

void CGContextCopyBits(CGContextRef context,CGRect rect,CGPoint point,int gState) {
   O2ContextCopyBits(context,rect,point,gState);
}

CFDataRef CGContextCaptureBitmap(CGContextRef context,CGRect rect) {
   return (CFDataRef)O2ContextCaptureBitmap(context,rect);
}
