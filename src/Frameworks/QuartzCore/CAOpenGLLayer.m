// CAOpenGLLayer.m - a layer whose contents a subclass draws with OpenGL through CGL
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

// the default pixel format and context come from CGL, drawing does not reach the layer's contents yet
#import <QuartzCore/CAOpenGLLayer.h>
#import <QuartzCore/CABase.h>
#import <OpenGL/gl.h>

@implementation CAOpenGLLayer

@synthesize asynchronous = _asynchronous;
@synthesize wantsExtendedDynamicRangeContent;

- (void)dealloc {
    if (_glContext != NULL)
        [self releaseCGLContext:_glContext];
    if (_pixelFormat != NULL)
        [self releaseCGLPixelFormat:_pixelFormat];
    if (_colorspace != NULL)
        CGColorSpaceRelease(_colorspace);
    [super dealloc];
}

- (CGColorSpaceRef)colorspace {
    return _colorspace;
}

- (void)setColorspace:(CGColorSpaceRef)colorspace {
    if (colorspace != NULL)
        CGColorSpaceRetain(colorspace);
    if (_colorspace != NULL)
        CGColorSpaceRelease(_colorspace);
    _colorspace = colorspace;
}

- (BOOL)canDrawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
    return YES;
}

// the base class clears, as Apple's does before a subclass draws anything
- (void)drawInCGLContext:(CGLContextObj)ctx pixelFormat:(CGLPixelFormatObj)pf forLayerTime:(CFTimeInterval)t displayTime:(const CVTimeStamp *)ts {
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
}

- (CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask {
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAColorSize, 24,
        kCGLPFAAlphaSize, 8,
        kCGLPFAAccelerated,
        0
    };
    CGLPixelFormatObj pf = NULL;
    GLint screens = 0;

    CGLChoosePixelFormat(attributes, &pf, &screens);
    return pf;
}

- (void)releaseCGLPixelFormat:(CGLPixelFormatObj)pf {
    CGLReleasePixelFormat(pf);
}

- (CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pf {
    CGLContextObj ctx = NULL;

    CGLCreateContext(pf, NULL, &ctx);
    return ctx;
}

- (void)releaseCGLContext:(CGLContextObj)ctx {
    CGLReleaseContext(ctx);
}

- (void)display {
    CGLContextObj previous = CGLGetCurrentContext();
    CFTimeInterval now = CACurrentMediaTime();

    if (_pixelFormat == NULL)
        _pixelFormat = [self copyCGLPixelFormatForDisplayMask:0];
    if (_glContext == NULL && _pixelFormat != NULL)
        _glContext = [self copyCGLContextForPixelFormat:_pixelFormat];
    if (_glContext == NULL)
        return;
    CGLSetCurrentContext(_glContext);
    if ([self canDrawInCGLContext:_glContext pixelFormat:_pixelFormat forLayerTime:now displayTime:NULL])
        [self drawInCGLContext:_glContext pixelFormat:_pixelFormat forLayerTime:now displayTime:NULL];
    CGLSetCurrentContext(previous);
}

@end
