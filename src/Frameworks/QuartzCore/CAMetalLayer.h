// the CAMetalLayer swapchain on Metal's Vulkan device, for a Wayland subsurface of an AppKit window (a view hosting the layer) or a GLFW window.
// MetalKit supplies it on first attach, and frames come back as CAMetalDrawables that MTLCommandBuffer can present
#ifndef PD_QUARTZCORE_CAMETALLAYER_H
#define PD_QUARTZCORE_CAMETALLAYER_H

#import <QuartzCore/CALayer.h>
#import <QuartzCore/CAEDRMetadata.h>

#if __has_include(<Metal/Metal.h>)
#import <Metal/Metal.h>
#define CA_METAL_DEVICE id<MTLDevice>
#define CA_METAL_PIXEL_FORMAT MTLPixelFormat
#define CA_METAL_TEXTURE id<MTLTexture>
#define CA_METAL_DRAWABLE_BASE MTLDrawable
#else
#define CA_METAL_DEVICE id
#define CA_METAL_PIXEL_FORMAT NSUInteger
#define CA_METAL_TEXTURE id
#define CA_METAL_DRAWABLE_BASE NSObject
#endif

@class CAMetalLayer;

@protocol CAMetalDrawable <CA_METAL_DRAWABLE_BASE>
@property(readonly) CA_METAL_TEXTURE texture;
@property(readonly) CAMetalLayer *layer;
@end

@interface CAMetalLayer : CALayer

@property(nonatomic, retain) CA_METAL_DEVICE device;
@property(nonatomic, readonly) CA_METAL_DEVICE preferredDevice;
// pixel formats BGRA8Unorm and BGRA8Unorm_sRGB
@property(nonatomic) CA_METAL_PIXEL_FORMAT pixelFormat;
@property(nonatomic) BOOL framebufferOnly;
@property(nonatomic) CGSize drawableSize;
@property(nonatomic) NSUInteger maximumDrawableCount;
@property(nonatomic) BOOL displaySyncEnabled;
@property(nonatomic) BOOL presentsWithTransaction;
@property(nonatomic) BOOL allowsNextDrawableTimeout;
@property(nonatomic) BOOL wantsExtendedDynamicRangeContent;
// stored only: the swapchain is BGRA8 in sRGB or linear by pixelFormat
@property(nonatomic) CGColorSpaceRef colorspace;
// stored only: PureDarwin composites in standard range
@property(nonatomic, retain) CAEDRMetadata *EDRMetadata;

// PD: the layer of a GLFW window (GLFWwindow *)
- (instancetype)initWithGLFWWindow:(void *)window;
// PD: a subsurface of an AppKit window (windowNumber) at origin from its top left
- (instancetype)initWithWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size;
// PD: moves the subsurface (AppKit layers only)
- (void)setWindowOrigin:(CGPoint)origin;
// nil until the layer is attached to a window, and when the window is gone
- (id<CAMetalDrawable>)nextDrawable;

@end

#ifdef __cplusplus
extern "C" {
#endif
// PD, for C and C++ callers (metal-cpp): a retained CAMetalLayer for a GLFW window, or NULL. In MetalKit
void *PDMetalLayerCreateForGLFWWindow(void *window);
#ifdef __cplusplus
}
#endif

#endif
