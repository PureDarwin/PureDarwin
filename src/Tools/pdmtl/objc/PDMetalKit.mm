// the CAMetalLayer swapchain and MTKView for PureDarwin over darling-metal: a VkSurfaceKHR on the executor's instance (from a GLFW window, or a Wayland subsurface of an AppKit window)
// and IndiumKit::Layer (indium-pd's pdmtl swapchain) behind it. MRC, like darling-metal
#import <MetalKit/MTKView.h>
#import <QuartzCore/CAMetalLayer.h>

#import <Metal/MTLDeviceInternal.h>
#import <Metal/MTLDrawableInternal.h>
#import <Metal/MTLTextureInternal.h>
#import <WindowServer/rpc.h>

#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <indium-kit/indium-kit.hpp>
#include <indium/pd-layer.hpp>
#include <indium/indium.private.hpp>

#include <dlfcn.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// vulkan_wayland.h without wayland-client.h: the handles only pass through
struct wl_display;
struct wl_surface;
#include <vulkan/vulkan_wayland.h>

@interface CAMetalDrawableInternal : MTLDrawableInternal <CAMetalDrawable>
- (instancetype)initWithKitDrawable:(std::shared_ptr<IndiumKit::Drawable>)drawable layer:(CAMetalLayer *)layer;
- (void)_pdUseTexture:(id<MTLTexture>)texture;
@end

@implementation CAMetalDrawableInternal {
  id<MTLTexture> _texture;
  CAMetalLayer *_layer;
}

- (instancetype)initWithKitDrawable:(std::shared_ptr<IndiumKit::Drawable>)drawable layer:(CAMetalLayer *)layer {
  if (!(self = [super initWithDrawable:drawable]))
    return nil;
  _layer = [layer retain];
  _texture = [[MTLTextureInternal alloc] initWithTexture:drawable->texture()
                                                  device:layer.device
                                         resourceOptions:MTLResourceStorageModePrivate];
  return self;
}

- (void)dealloc {
  [_texture release];
  [_layer release];
  [super dealloc];
}

- (id<MTLTexture>)texture {
  return _texture;
}

// an offscreen drawable hands out the layer's own texture object, which knows its IOSurface
- (void)_pdUseTexture:(id<MTLTexture>)texture {
  [_texture release];
  _texture = [texture retain];
}

- (CAMetalLayer *)layer {
  return _layer;
}

@end

// a Wayland surface on Metal's instance, through the loader or driver Metal itself opened: an app may have
// brought its own libvulkan (MoltenVK apps do), which a dlopen by name would find instead
static VkSurfaceKHR wayland_surface(void *display, void *surface) {
  static PFN_vkCreateWaylandSurfaceKHR create;
  VkWaylandSurfaceCreateInfoKHR ci = { VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR };
  VkSurfaceKHR s = VK_NULL_HANDLE;

  Indium::ensureStarted();
  if (!create && Indium::globalGetInstanceProcAddr)
    create = (PFN_vkCreateWaylandSurfaceKHR)Indium::globalGetInstanceProcAddr(Indium::globalInstance, "vkCreateWaylandSurfaceKHR");
  ci.display = (struct wl_display *)display, ci.surface = (struct wl_surface *)surface;
  if (!create || create(Indium::globalInstance, &ci, NULL, &s) != VK_SUCCESS)
    return VK_NULL_HANDLE;
  return s;
}

// a drawable of a layer in no window: a texture over an IOSurface when it can be, presenting it makes the
// surface the layer's contents so a CARenderer tree (a compositor's layers) shows it
class OffscreenDrawable: public IndiumKit::Drawable {
public:
  OffscreenDrawable(std::shared_ptr<Indium::Texture> texture, CAMetalLayer *layer, id surface)
      : _texture(texture), _layer([layer retain]), _surface([surface retain]) {}
  ~OffscreenDrawable() { [_layer release], [_surface release]; }
  std::shared_ptr<Indium::Texture> texture() override { return _texture; }
  void present() override {
    if (_surface && _layer.contents != _surface) _layer.contents = _surface;
  }

private:
  std::shared_ptr<Indium::Texture> _texture;
  CAMetalLayer *_layer;
  id _surface;
};

// a BGRA IOSurface the size of the drawable, nil without IOSurface.framework
static id newOffscreenSurface(NSUInteger width, NSUInteger height) {
  static auto create = (id (*)(NSDictionary *))dlsym(RTLD_DEFAULT, "IOSurfaceCreate");
  if (!create) {
    dlopen("/System/Library/Frameworks/IOSurface.framework/IOSurface", RTLD_LAZY | RTLD_GLOBAL);
    create = (id (*)(NSDictionary *))dlsym(RTLD_DEFAULT, "IOSurfaceCreate");
  }
  if (!create) return nil;
  NSDictionary *props = @{ @"IOSurfaceWidth": @(width), @"IOSurfaceHeight": @(height), @"IOSurfaceBytesPerElement": @4,
                           @"IOSurfaceBytesPerRow": @(width * 4), @"IOSurfacePixelFormat": @((unsigned)'BGRA') };
  return create(props);
}

@interface NSObject (PDSurfaceTextures)
- (id<MTLTexture>)newTextureWithDescriptor:(MTLTextureDescriptor *)d iosurface:(id)surface plane:(NSUInteger)plane;
- (id)iosurface;
@end

static char offscreenKey;
static bool glBackend() {
  const char *e=getenv("AIRJITD_OPENGL"); return e && *e && strcmp(e,"0");
}
static int presentAppKitFrame(void *context,const void *pixels,int w,int h,int pitch) {
  static auto present=(int (*)(void *,const void *,int,int,int))dlsym(RTLD_DEFAULT,"_windowServerPresentFrame");
  return present?present(context,pixels,w,h,pitch):-1;
}
static void *acquireAppKitFrame(void *context,int w,int h,int *pitch) {
  static auto acquire=(void *(*)(void *,int,int,int *))dlsym(RTLD_DEFAULT,"_windowServerAcquireFrame");
  return acquire?acquire(context,w,h,pitch):nullptr;
}
static int commitAppKitFrame(void *context,void *pixels,int w,int h) {
  static auto commit=(int (*)(void *,void *,int,int))dlsym(RTLD_DEFAULT,"_windowServerCommitFrame");
  return commit?commit(context,pixels,w,h):-1;
}
static void cancelAppKitFrame(void *context,void *pixels) {
  static auto cancel=(void (*)(void *,void *))dlsym(RTLD_DEFAULT,"_windowServerCancelFrame");
  if (cancel) cancel(context,pixels);
}


// the swapchain behind QuartzCore's CAMetalLayer: QuartzCore loads MetalKit and makes one per attached layer
@interface PDMetalLayerBackend : NSObject
@end

@implementation PDMetalLayerBackend {
  CAMetalLayer *_layer; // owns this backend, not retained
  GLFWwindow *_window;
  VkSurfaceKHR _surface;
  std::shared_ptr<IndiumKit::Layer> _kit;
  id<MTLDevice> _device;
  MTLPixelFormat _pixelFormat;
  CGSize _size;
  // AppKit windows: the subsurface and the window it belongs to
  NSInteger _windowNumber;
  void *_wlSurface, *_wlSubsurface, *_framePresenter;
}

// one texture per layer, remade when the layer's device, format or size changes
+ (id<CAMetalDrawable>)offscreenDrawableForLayer:(CAMetalLayer *)layer {
  id<MTLDevice> device = layer.device;
  CGSize size = layer.drawableSize;
  MTLPixelFormat format = layer.pixelFormat ? layer.pixelFormat : MTLPixelFormatBGRA8Unorm;
  MTLTextureInternal *t = objc_getAssociatedObject(layer, &offscreenKey);

  if (!device)
    return nil;
  if (size.width < 1 || size.height < 1)
    size = CGSizeMake(1, 1);
  if (!t || t.device != device || t.pixelFormat != format || t.width != (NSUInteger)size.width ||
      t.height != (NSUInteger)size.height) {
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:(NSUInteger)size.width
                                                                                 height:(NSUInteger)size.height mipmapped:NO];
    d.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
    t = nil;
    if (format == MTLPixelFormatBGRA8Unorm) {
      id surface = newOffscreenSurface(d.width, d.height);
      if (surface) t = (MTLTextureInternal *)[(id)device newTextureWithDescriptor:d iosurface:surface plane:0];
      [surface release];
    }
    if (!t) {
      d.storageMode = MTLStorageModePrivate;
      t = (MTLTextureInternal *)[device newTextureWithDescriptor:d];
    }
    if (!t)
      return nil;
    objc_setAssociatedObject(layer, &offscreenKey, t, OBJC_ASSOCIATION_RETAIN);
    [t release];
  }
  CAMetalDrawableInternal *d = [[CAMetalDrawableInternal alloc]
      initWithKitDrawable:std::make_shared<OffscreenDrawable>(t.texture, layer, [(id)t iosurface]) layer:layer];
  [d _pdUseTexture:t];
  return [d autorelease];
}

- (instancetype)initWithGLFWWindow:(void *)window layer:(CAMetalLayer *)layer {
  int w, h;

  if (!(self = [super init]))
    return nil;
  _layer = layer, _pixelFormat = MTLPixelFormatBGRA8Unorm;
  _window = (GLFWwindow *)window;
  Indium::ensureStarted();
  if (!glBackend() && glfwCreateWindowSurface(Indium::globalInstance, _window, NULL, &_surface) != VK_SUCCESS) {
    fprintf(stderr, "CAMetalLayer: no Vulkan surface for the window (did Metal load with surface extensions?)\n");
    [self release];
    return nil;
  }
  glfwGetFramebufferSize(_window, &w, &h);
  _size.width = w, _size.height = h;
  return self;
}

- (instancetype)initWithWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size layer:(CAMetalLayer *)layer {
  void *display = _windowServerWaylandDisplay();

  if (!(self = [super init]))
    return nil;
  _layer = layer, _pixelFormat = MTLPixelFormatBGRA8Unorm;
  _windowNumber = window, _size = size;
  if (!display || !(_wlSurface = _windowServerCreateSubsurface((int)window, (int)origin.x, (int)origin.y, &_wlSubsurface)) ||
      (!glBackend() && !(_surface = wayland_surface(display, _wlSurface)))) {
    fprintf(stderr, "CAMetalLayer: no Wayland subsurface for window %ld\n", (long)window);
    [self release];
    return nil;
  }
  if (glBackend()) {
    auto create=(void *(*)(void *))dlsym(RTLD_DEFAULT,"_windowServerCreateFramePresenter");
    if (!create || !(_framePresenter=create(_wlSurface))) { [self release]; return nil; }
  }
  return self;
}

- (void)dealloc {
  _kit.reset();
  [_device release];
  if (_framePresenter) {
    auto destroy=(void (*)(void *))dlsym(RTLD_DEFAULT,"_windowServerDestroyFramePresenter");
    if (destroy) destroy(_framePresenter);
  }
  if (_wlSurface)
    _windowServerDestroySubsurface(_wlSurface, _wlSubsurface);
  [super dealloc];
}

- (CGSize)surfaceSize {
  return _size;
}

- (void)setWindowOrigin:(CGPoint)origin {
  if (_wlSubsurface)
    _windowServerMoveSubsurface((int)_windowNumber, _wlSubsurface, (int)origin.x, (int)origin.y);
}

// the layer takes the subsurface over for a Vulkan surface of its own, this backend stops presenting and forgets it
- (void *)relinquishWaylandSurface:(void **)subsurface {
  void *surface = _wlSurface;

  _kit.reset();
  if (subsurface)
    *subsurface = _wlSubsurface;
  _wlSurface = _wlSubsurface = NULL;
  return surface;
}

// a new device or size is a new swapchain, a new format only changes the views onto it
- (void)configureDevice:(id)device pixelFormat:(NSUInteger)format size:(CGSize)size {
  BOOL rebuild = device != _device || size.width != _size.width || size.height != _size.height;

  if (size.width > 0 && size.height > 0)
    _size = size;
  if (device != _device) {
    [device retain];
    [_device release];
    _device = device;
  }
  if (rebuild || !_kit) {
    _kit.reset();
    if (_device && glBackend()) {
      auto device=((MTLDeviceInternal *)_device).device;
      if (_window) _kit=PDMakeGLFWLayer(device,_window);
      else if (_framePresenter) _kit=PDMakeFrameLayer(device,_framePresenter,presentAppKitFrame,
        acquireAppKitFrame,commitAppKitFrame,cancelAppKitFrame,(size_t)_size.width,(size_t)_size.height);
    } else if (_device && _surface)
      _kit = IndiumKit::Layer::make(_surface, ((MTLDeviceInternal *)_device).device, (size_t)_size.width,
                                    (size_t)_size.height);
    if (_kit && format != MTLPixelFormatBGRA8Unorm)
      _kit->setPixelFormat((Indium::PixelFormat)format);
  } else if (format != (NSUInteger)_pixelFormat && _kit) {
    _kit->setPixelFormat((Indium::PixelFormat)format);
  }
  _pixelFormat = (MTLPixelFormat)format;
}

- (id<CAMetalDrawable>)nextDrawable {
  std::shared_ptr<IndiumKit::Drawable> d;

  if (!_kit && _device)
    [self configureDevice:_device pixelFormat:_pixelFormat size:_size];
  if (!_kit || !(d = _kit->nextDrawable())) {
    static int logged;
    if (!logged++)
      fprintf(stderr, "CAMetalLayer: no drawable (device %p, surface %d, swapchain %d, size %gx%g)\n", (void *)_device,
              _surface != VK_NULL_HANDLE, _kit != nullptr, _size.width, _size.height);
    return nil;
  }
  return [[[CAMetalDrawableInternal alloc] initWithKitDrawable:d layer:_layer] autorelease];
}

@end

void *PDMetalLayerCreateForGLFWWindow(void *window) {
  return [[CAMetalLayer alloc] initWithGLFWWindow:window];
}

static double now(void) {
  struct timespec ts;

  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

@implementation MTKView {
  MTLPixelFormat _colorPixelFormat;
  GLFWwindow *_glfw;
  CAMetalLayer *_layer;
  id<MTLDevice> _device;
  id<CAMetalDrawable> _drawable;
  MTLRenderPassDescriptor *_pass;
  NSTimer *_timer;
  CGSize _size;
  id<MTLTexture> _depthTexture, _msaaTexture;
}

@synthesize delegate = _delegate, clearColor = _clearColor;
@synthesize preferredFramesPerSecond = _preferredFramesPerSecond, paused = _paused;
@synthesize enableSetNeedsDisplay = _enableSetNeedsDisplay, framebufferOnly = _framebufferOnly;
@synthesize layer = _layer, title = _title;
@synthesize depthStencilPixelFormat = _depthStencilPixelFormat, clearDepth = _clearDepth, clearStencil = _clearStencil;
@synthesize depthStencilAttachmentTextureUsage = _depthStencilAttachmentTextureUsage;
@synthesize depthStencilStorageMode = _depthStencilStorageMode, sampleCount = _sampleCount;
@synthesize multisampleColorAttachmentTextureUsage = _multisampleColorAttachmentTextureUsage;

- (void)setDefaults {
  _colorPixelFormat = MTLPixelFormatBGRA8Unorm, _clearColor = MTLClearColorMake(0, 0, 0, 1);
  _preferredFramesPerSecond = 60, _framebufferOnly = YES, _clearDepth = 1;
  _sampleCount = 1, _depthStencilAttachmentTextureUsage = MTLTextureUsageRenderTarget;
  _depthStencilStorageMode = MTLStorageModePrivate, _multisampleColorAttachmentTextureUsage = MTLTextureUsageRenderTarget;
}

- (instancetype)initWithFrame:(CGRect)frame device:(id<MTLDevice>)device {
  if (!(self = [super initWithFrame:frame]))
    return nil;
  [self setDefaults];
  _size = frame.size;
  self.device = device;
  return self;
}

// from a nib (a compiled storyboard's view): no device until one is set
- (instancetype)initWithCoder:(NSCoder *)coder {
  if (!(self = [super initWithCoder:coder]))
    return nil;
  [self setDefaults];
  _size = [self frame].size;
  return self;
}

// the NSView initializer, as a storyboard or nib makes the view: no device until one is set
- (instancetype)initWithFrame:(NSRect)frame {
  return [self initWithFrame:frame device:nil];
}

- (MTLPixelFormat)colorPixelFormat {
  return _colorPixelFormat;
}

- (void)setColorPixelFormat:(MTLPixelFormat)format {
  _colorPixelFormat = format;
  _layer.pixelFormat = format;
}

- (void)dealloc {
  [_timer invalidate];
  [self releaseDrawables];
  [_layer release];
  [_device release];
  [_title release];
  [_depthTexture release];
  [_msaaTexture release];
  if (_glfw)
    glfwDestroyWindow(_glfw);
  [super dealloc];
}

- (id<MTLDevice>)device {
  return _device;
}

- (void)setDevice:(id<MTLDevice>)device {
  [device retain];
  [_device release];
  _device = device;
  _layer.device = device;
}

- (void)setTitle:(NSString *)title {
  [_title release];
  _title = [title copy];
  if (_glfw)
    glfwSetWindowTitle(_glfw, [_title UTF8String]);
}

- (CGSize)drawableSize {
  return _layer ? _layer.drawableSize : _size;
}

- (void)setDrawableSize:(CGSize)size {
  _size = size;
  _layer.drawableSize = size;
}

// AppKit: the view's rectangle in Wayland terms, from the window's top left
- (CGRect)rectInWindow {
  NSRect r = [self convertRect:[self bounds] toView:nil];
  CGRect out;

  out.origin.x = r.origin.x;
  out.origin.y = [[self window] frame].size.height - (r.origin.y + r.size.height);
  out.size = r.size;
  return out;
}

- (void)viewDidMoveToWindow {
  [super viewDidMoveToWindow];
  [_timer invalidate];
  _timer = nil;
  [self releaseDrawables];
  if (_glfw)
    return;
  [_layer release];
  _layer = nil;
  if (![self window])
    return;

  CGRect r = [self rectInWindow];
  if (!(_layer = [[CAMetalLayer alloc] initWithWindowNumber:[[self window] windowNumber] origin:r.origin size:r.size]))
    return;
  _size = r.size;
  _layer.pixelFormat = _colorPixelFormat;
  _layer.device = _device;
  _timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / (_preferredFramesPerSecond > 0 ? _preferredFramesPerSecond : 60)
                                            target:self
                                          selector:@selector(_drawFromTimer:)
                                          userInfo:nil
                                           repeats:YES];
}

- (void)_drawFromTimer:(NSTimer *)timer {
  (void)timer;
  if (!_paused && !_enableSetNeedsDisplay)
    [self draw];
}

// AppKit: the subsurface follows the view, and the delegate hears about a new size
- (void)setFrameSize:(NSSize)size {
  [super setFrameSize:size];
  if (_layer && !_glfw && [self window]) {
    CGRect r = [self rectInWindow];

    [_layer setWindowOrigin:r.origin];
    if (r.size.width != _layer.drawableSize.width || r.size.height != _layer.drawableSize.height) {
      [self releaseDrawables];
      _layer.drawableSize = r.size;
      [_delegate mtkView:self drawableSizeWillChange:r.size];
    }
  }
}

- (void)setFrameOrigin:(NSPoint)origin {
  [super setFrameOrigin:origin];
  if (_layer && !_glfw && [self window])
    [_layer setWindowOrigin:[self rectInWindow].origin];
}

- (void)drawRect:(NSRect)rect {
  (void)rect;
  if (_enableSetNeedsDisplay && _layer)
    [self draw];
}

- (id<CAMetalDrawable>)currentDrawable {
  if (!_drawable)
    _drawable = [[_layer nextDrawable] retain];
  return _drawable;
}

- (MTLRenderPassDescriptor *)currentRenderPassDescriptor {
  id<CAMetalDrawable> d;

  if (_pass)
    return _pass;
  if (!(d = self.currentDrawable))
    return nil;
  _pass = [[MTLRenderPassDescriptor alloc] init];
  _pass.colorAttachments[0].texture = d.texture;
  _pass.colorAttachments[0].loadAction = MTLLoadActionClear;
  _pass.colorAttachments[0].storeAction = MTLStoreActionStore;
  _pass.colorAttachments[0].clearColor = _clearColor;
  if (_sampleCount > 1 && self.multisampleColorTexture) {
    _pass.colorAttachments[0].texture = _msaaTexture;
    _pass.colorAttachments[0].resolveTexture = d.texture;
    _pass.colorAttachments[0].storeAction = MTLStoreActionMultisampleResolve;
  }
  if (_depthStencilPixelFormat != MTLPixelFormatInvalid && self.depthStencilTexture) {
    _pass.depthAttachment.texture = _depthTexture;
    _pass.depthAttachment.loadAction = MTLLoadActionClear;
    _pass.depthAttachment.storeAction = MTLStoreActionDontCare;
    _pass.depthAttachment.clearDepth = _clearDepth;
    if (_depthStencilPixelFormat == MTLPixelFormatDepth32Float_Stencil8 || _depthStencilPixelFormat == MTLPixelFormatStencil8) {
      _pass.stencilAttachment.texture = _depthTexture;
      _pass.stencilAttachment.loadAction = MTLLoadActionClear;
      _pass.stencilAttachment.storeAction = MTLStoreActionDontCare;
      _pass.stencilAttachment.clearStencil = _clearStencil;
    }
  }
  return _pass;
}

// made again whenever the drawable changes size
- (id<MTLTexture>)depthStencilTexture {
  id<MTLTexture> color = self.currentDrawable.texture;
  MTLTextureDescriptor *desc;

  if (_depthStencilPixelFormat == MTLPixelFormatInvalid || !color)
    return nil;
  if (_depthTexture && _depthTexture.width == color.width && _depthTexture.height == color.height &&
      _depthTexture.pixelFormat == _depthStencilPixelFormat && _depthTexture.sampleCount == MAX(_sampleCount, 1u))
    return _depthTexture;
  [_depthTexture release];
  desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:_depthStencilPixelFormat
                                                            width:color.width
                                                           height:color.height
                                                        mipmapped:NO];
  desc.usage = _depthStencilAttachmentTextureUsage, desc.storageMode = _depthStencilStorageMode;
  if (_sampleCount > 1)
    desc.textureType = MTLTextureType2DMultisample, desc.sampleCount = _sampleCount;
  _depthTexture = [_device newTextureWithDescriptor:desc];
  return _depthTexture;
}

// the drawable's size and format at sampleCount samples, made again when any of them changes
- (id<MTLTexture>)multisampleColorTexture {
  id<MTLTexture> color = self.currentDrawable.texture;
  MTLTextureDescriptor *desc;

  if (_sampleCount <= 1 || !color)
    return nil;
  if (_msaaTexture && _msaaTexture.width == color.width && _msaaTexture.height == color.height &&
      _msaaTexture.pixelFormat == color.pixelFormat && _msaaTexture.sampleCount == _sampleCount)
    return _msaaTexture;
  [_msaaTexture release];
  desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:color.pixelFormat width:color.width
                                                           height:color.height mipmapped:NO];
  desc.textureType = MTLTextureType2DMultisample, desc.sampleCount = _sampleCount;
  desc.usage = _multisampleColorAttachmentTextureUsage, desc.storageMode = MTLStorageModePrivate;
  _msaaTexture = [_device newTextureWithDescriptor:desc];
  return _msaaTexture;
}

- (void)releaseDrawables {
  [_pass release];
  [_drawable release];
  _pass = nil, _drawable = nil;
}

- (void)draw {
  NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

  [_delegate drawInMTKView:self];
  [self releaseDrawables];
  [pool release];
}

- (void)run {
  const char *fps = getenv("PD_MTK_FPS"), *limit = getenv("PD_MTK_FRAMES");
  long frames = 0, max = limit ? atol(limit) : 0;
  double t0 = now();
  long n0 = 0;

  if (!_glfw) {
    if (!glfwInit()) {
      fprintf(stderr, "MTKView: GLFW could not open the display\n");
      return;
    }
    glfwWindowHint(GLFW_CLIENT_API, glBackend()?GLFW_OPENGL_API:GLFW_NO_API);
    if (glBackend()) {
      glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,2);
      glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    }
    _glfw = glfwCreateWindow((int)_size.width, (int)_size.height, _title ? [_title UTF8String] : "Metal", NULL, NULL);
    [_layer release];
    if (!_glfw || !(_layer = [[CAMetalLayer alloc] initWithGLFWWindow:_glfw]))
      return;
    _layer.pixelFormat = _colorPixelFormat;
    _layer.device = _device;
  }

  while (!glfwWindowShouldClose(_glfw) && (!max || frames < max)) {
    glfwPollEvents();
    if (_paused) {
      glfwWaitEventsTimeout(0.1);
      continue;
    }
    // a tiling compositor or the user resized the window: the delegate hears it before the next frame
    {
      int w, h;

      glfwGetFramebufferSize(_glfw, &w, &h);
      if (w > 0 && h > 0 && (w != (int)_layer.drawableSize.width || h != (int)_layer.drawableSize.height)) {
        CGSize size;

        size.width = w, size.height = h;
        _layer.drawableSize = size;
        [_delegate mtkView:self drawableSizeWillChange:size];
      }
    }
    [self draw];
    frames++;
    if (fps && now() - t0 >= 1) {
      printf("MTKView: %.1f fps\n", (frames - n0) / (now() - t0));
      fflush(stdout);
      t0 = now(), n0 = frames;
    }
  }
}

@end
