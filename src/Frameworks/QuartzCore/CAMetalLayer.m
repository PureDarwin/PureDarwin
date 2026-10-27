#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CAMetalLayer-Private.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <objc/runtime.h>

// MetalKit's swapchain behind a layer (PDMetalLayerBackend)
@interface NSObject (PDMetalLayerBackend)
- (instancetype)initWithGLFWWindow:(void *)window layer:(CAMetalLayer *)layer;
- (instancetype)initWithWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size layer:(CAMetalLayer *)layer;
- (CGSize)surfaceSize;
- (void)setWindowOrigin:(CGPoint)origin;
- (void)configureDevice:(id)device pixelFormat:(NSUInteger)format size:(CGSize)size;
- (id<CAMetalDrawable>)nextDrawable;
- (void *)relinquishWaylandSurface:(void **)subsurface;
+ (id<CAMetalDrawable>)offscreenDrawableForLayer:(CAMetalLayer *)layer;
@end

// the MTLPixelFormatBGRA8Unorm format
#define BGRA8_UNORM 80

static Class backendClass(void) {
    static Class cls;

    if (cls == Nil) {
        cls = objc_getClass("PDMetalLayerBackend");
        if (cls == Nil && dlopen("/System/Library/Frameworks/MetalKit.framework/Versions/A/MetalKit", RTLD_NOW | RTLD_GLOBAL))
            cls = objc_getClass("PDMetalLayerBackend");
        // without MetalKit a layer has no drawables at all: say why once
        static int logged;
        const char *e = cls == Nil ? dlerror() : 0;
        if (cls == Nil && !logged++)
            fprintf(stderr, "CAMetalLayer: no MetalKit backend: %s\n", e ? e : "PDMetalLayerBackend missing");
    }
    return cls;
}

// PD_CAMETAL_TRACE: where an externally presented layer's surface goes
static int traceOn(void) {
    static int on = -1;

    if (on < 0)
        on = getenv("PD_CAMETAL_TRACE") != NULL;
    return on;
}

// WindowServer's subsurface calls, looked up so QuartzCore does not link it
static void *ws(const char *name) {
    return dlsym(RTLD_DEFAULT, name);
}

@implementation CAMetalLayer {
    id _device;
    NSUInteger _pixelFormat;
    CGSize _drawableSize, _viewSize;
    CGColorSpaceRef _colorspace;
    id _backend;
    // a Vulkan surface made on this layer (VK_EXT_metal_surface) presents into its own wl_surface, which the layer
    // keeps across windows: a subsurface of the window while attached
    void *_extSurface, *_extSubsurface;
    NSInteger _extWindow;
}

@synthesize framebufferOnly = _framebufferOnly, maximumDrawableCount = _maximumDrawableCount;
@synthesize displaySyncEnabled = _displaySyncEnabled, presentsWithTransaction = _presentsWithTransaction;
@synthesize allowsNextDrawableTimeout = _allowsNextDrawableTimeout;
@synthesize wantsExtendedDynamicRangeContent = _wantsExtendedDynamicRangeContent;
@synthesize EDRMetadata = _EDRMetadata;

- (instancetype)init {
    if (!(self = [super init]))
        return nil;
    _pixelFormat = BGRA8_UNORM, _framebufferOnly = YES, _maximumDrawableCount = 3;
    _displaySyncEnabled = YES, _allowsNextDrawableTimeout = YES;
    return self;
}

- (instancetype)initWithGLFWWindow:(void *)window {
    if (!(self = [self init]))
        return nil;
    if (!(_backend = [[backendClass() alloc] initWithGLFWWindow:window layer:self])) {
        [self release];
        return nil;
    }
    _drawableSize = [_backend surfaceSize];
    return self;
}

- (instancetype)initWithWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size {
    if (!(self = [self init]))
        return nil;
    _drawableSize = size;
    if (![self _pdAttachToWindowNumber:window origin:origin size:size]) {
        [self release];
        return nil;
    }
    return self;
}

- (void)dealloc {
    void (*destroy)(void *, void *) = ws("_windowServerDestroySubsurface");

    if (_extSurface && destroy)
        destroy(_extSurface, _extSubsurface);
    [_backend release];
    [_device release];
    CGColorSpaceRelease(_colorspace);
    [_EDRMetadata release];
    [super dealloc];
}

// until the app sets drawableSize the swapchain takes the view's size, the property stays as the app left it,
// since apps compare it with their view to know when to remake their own targets
- (BOOL)_pdAttachToWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size {
    _viewSize = size;
    _extWindow = window;
    if (_extSurface) {
        void *(*attach)(int, void *, int, int) = ws("_windowServerAttachSubsurface");

        if (!_extSubsurface && attach)
            _extSubsurface = attach((int)window, _extSurface, (int)origin.x, (int)origin.y);
        if (traceOn())
            fprintf(stderr, "CAMetalLayer %p: attach to window %ld at %g,%g %gx%g -> %p\n", (void *)self, (long)window,
                    origin.x, origin.y, size.width, size.height, _extSubsurface);
        return _extSubsurface != NULL;
    }
    [_backend release];
    _backend = nil;
    if (!(_backend = [[backendClass() alloc] initWithWindowNumber:window origin:origin size:[self _swapchainSize] layer:self]))
        return NO;
    [self _configure];
    return YES;
}

- (CGSize)_swapchainSize {
    return _drawableSize.width > 0 && _drawableSize.height > 0 ? _drawableSize : _viewSize;
}

// the hosting view's new size: the swapchain follows it while the app leaves drawableSize unset
- (void)_pdSetViewSize:(CGSize)size {
    if (size.width == _viewSize.width && size.height == _viewSize.height)
        return;
    _viewSize = size;
    if (_drawableSize.width <= 0 || _drawableSize.height <= 0)
        [self _configure];
}

- (void)_pdDetach {
    if (_extSubsurface) {
        void (*destroy)(void *, void *) = ws("_windowServerDestroySubsurface");

        if (destroy)
            destroy(NULL, _extSubsurface);
        _extSubsurface = NULL;
    }
    [_backend release];
    _backend = nil;
}

- (BOOL)_pdIsAttached {
    return _extSurface ? _extSubsurface != NULL : _backend != nil;
}

// the wl_surface for a Vulkan surface made on this layer directly (VK_EXT_metal_surface). From then on the layer
// hands out no drawables of its own, and the surface outlives windows: a view not yet in one is placed on attach
- (void *)_pdWaylandSurface:(void **)display {
    void *(*wlDisplay)(void) = ws("_windowServerWaylandDisplay");
    void *(*create)(void) = ws("_windowServerCreateSurface");

    if (!_extSurface) {
        if (_backend) {
            _extSurface = [_backend relinquishWaylandSurface:&_extSubsurface];
            [_backend release];
            _backend = nil;
        }
        if (!_extSurface && create)
            _extSurface = create();
    }
    if (display)
        *display = wlDisplay ? wlDisplay() : NULL;
    if (traceOn())
        fprintf(stderr, "CAMetalLayer %p: wayland surface %p (subsurface %p, window %ld)\n", (void *)self, _extSurface,
                _extSubsurface, (long)_extWindow);
    return _extSurface;
}

- (void)_configure {
    [_backend configureDevice:_device pixelFormat:_pixelFormat size:[self _swapchainSize]];
}

- (void)setWindowOrigin:(CGPoint)origin {
    void (*move)(int, void *, int, int) = ws("_windowServerMoveSubsurface");

    if (_extSubsurface && move)
        move((int)_extWindow, _extSubsurface, (int)origin.x, (int)origin.y);
    [_backend setWindowOrigin:origin];
}

- (id)device {
    return _device;
}

// the swapchain lives on the device, so it is made when the device is set
- (void)setDevice:(id)device {
    if (device == _device)
        return;
    [device retain];
    [_device release];
    _device = device;
    [self _configure];
}

- (id)preferredDevice {
    id (*create)(void) = (id (*)(void))dlsym(RTLD_DEFAULT, "MTLCreateSystemDefaultDevice");

    return create ? [create() autorelease] : nil;
}

- (NSUInteger)pixelFormat {
    return _pixelFormat;
}

- (void)setPixelFormat:(NSUInteger)format {
    if (format == _pixelFormat)
        return;
    _pixelFormat = format;
    [self _configure];
}

- (CGSize)drawableSize {
    return _drawableSize;
}

// a new size is a new swapchain, since Wayland surfaces take their size from it
- (void)setDrawableSize:(CGSize)size {
    if (size.width == _drawableSize.width && size.height == _drawableSize.height)
        return;
    _drawableSize = size;
    [self _configure];
}

- (CGColorSpaceRef)colorspace {
    return _colorspace;
}

- (void)setColorspace:(CGColorSpaceRef)space {
    CGColorSpaceRetain(space);
    CGColorSpaceRelease(_colorspace);
    _colorspace = space;
}

// a layer in no window has no swapchain: like macOS it still hands out drawables, ones nobody sees
- (id<CAMetalDrawable>)nextDrawable {
    if (_backend == nil)
        return [backendClass() offscreenDrawableForLayer:self];
    return [_backend nextDrawable];
}

@end
