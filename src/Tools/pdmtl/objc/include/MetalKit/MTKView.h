// MTKView on PureDarwin. In an AppKit window it presents through a CAMetalLayer on a Wayland subsurface of
// the window and draws from a timer, as on macOS. Without AppKit, -run (PD) opens a GLFW window (Wayland or
// X11) and draws until it closes. Apple's MTKViewDelegate renderers work unchanged either way
#ifndef PD_METALKIT_MTKVIEW_H
#define PD_METALKIT_MTKVIEW_H

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

@class MTKView;

@protocol MTKViewDelegate <NSObject>
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size;
- (void)drawInMTKView:(MTKView *)view;
@end

@interface MTKView : NSView

@property(nonatomic, assign) id<MTKViewDelegate> delegate;
@property(nonatomic, retain) id<MTLDevice> device;
@property(nonatomic) MTLPixelFormat colorPixelFormat;
@property(nonatomic) MTLClearColor clearColor;
// a depth (or depth+stencil) texture the size of the drawable, attached to currentRenderPassDescriptor
@property(nonatomic) MTLPixelFormat depthStencilPixelFormat;
@property(nonatomic) double clearDepth;
@property(nonatomic) uint32_t clearStencil;
@property(nonatomic, readonly) id<MTLTexture> depthStencilTexture;
@property(nonatomic) MTLTextureUsage depthStencilAttachmentTextureUsage;
@property(nonatomic) MTLStorageMode depthStencilStorageMode;
// sampleCount > 1: currentRenderPassDescriptor draws into multisampleColorTexture and resolves into the drawable
@property(nonatomic) NSUInteger sampleCount;
@property(nonatomic, readonly) id<MTLTexture> multisampleColorTexture;
@property(nonatomic) MTLTextureUsage multisampleColorAttachmentTextureUsage;
@property(nonatomic) CGSize drawableSize;
@property(nonatomic) NSInteger preferredFramesPerSecond;
@property(nonatomic, getter=isPaused) BOOL paused;
@property(nonatomic) BOOL enableSetNeedsDisplay;
@property(nonatomic) BOOL framebufferOnly;
@property(nonatomic, readonly) CAMetalLayer *layer;
@property(nonatomic, readonly) MTLRenderPassDescriptor *currentRenderPassDescriptor;
@property(nonatomic, readonly) id<CAMetalDrawable> currentDrawable;
// PD: the window title
@property(nonatomic, copy) NSString *title;

- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device;
- (void)draw;
- (void)releaseDrawables;
// PD, without AppKit: a GLFW window drawn until it closes (PD_MTK_FPS=1 prints frames per second,
// PD_MTK_FRAMES=n stops after n)
- (void)run;

@end

#endif
