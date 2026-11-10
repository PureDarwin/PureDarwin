// CAOpenGLLayer.h - a layer whose contents a subclass draws with OpenGL through CGL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef PD_QUARTZCORE_CAOPENGLLAYER_H
#define PD_QUARTZCORE_CAOPENGLLAYER_H

#import <QuartzCore/CALayer.h>
#import <CoreVideo/CVBase.h>

@interface CAOpenGLLayer : CALayer {
    BOOL _asynchronous;
    CGLPixelFormatObj _pixelFormat;
    CGLContextObj _glContext;
    CGColorSpaceRef _colorspace;
}

@property(getter=isAsynchronous) BOOL asynchronous;
@property CGColorSpaceRef colorspace;
@property BOOL wantsExtendedDynamicRangeContent;

- (BOOL)canDrawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts;
- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask;
- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pf;
- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf;
- (void)releaseCGLContext:(CGLContextObj)ctx;

@end

#endif
