#import <Foundation/NSObject.h>
#import <CoreGraphics/CGGeometry.h>
#import <CoreGraphics/CGImage.h>
#import <OpenGL/OpenGL.h>

@class CARenderer, CALayer, CGLPixelSurface, NSTimer, NSMutableArray, NSNumber;

@interface CALayerContext : NSObject {
    CGRect _frame;
    CGLPixelFormatObj _pixelFormat;
    CGLContextObj _glContext;
    CALayer *_layer;
    CARenderer *_renderer;

    NSMutableArray *_deleteTextureIds;

    NSTimer *_timer;
    id _delegate;
    BOOL _renderScheduled;
}

- initWithFrame:(CGRect)rect;

- (void)setFrame:(CGRect)value;
- (void)setLayer:(CALayer *)layer;

- (void)invalidate;

- (void)render;

- (void)startTimerIfNeeded;

- (void)deleteTextureId:(NSNumber *)textureId;

// the delegate hears -layerContextNeedsDisplay: after the layer tree changes
- (void)setDelegate:delegate;
- (void)_layerTreeChanged;
// the last render as an image, rows top down, premultiplied BGRA
- (CGImageRef)createImage;

@end
