// CAContext.m - CAContext exports a CAMetalLayer's surface, CALayerHost shows one as a subsurface where its view sits.
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
#import <QuartzCore/CAContext.h>
#import <QuartzCore/CAContext-Private.h>
#import <QuartzCore/CAMetalLayer.h>
#import <QuartzCore/CAMetalLayer-Private.h>
#include <dlfcn.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// PD_CAMETAL_TRACE: which ids are made and where they are hosted
static int traceOn(void) {
    static int on = -1;

    if (on < 0)
        on = getenv("PD_CAMETAL_TRACE") != NULL;
    return on;
}

// a view not yet laid out in its window reports NaN: such a host sits at the window's corner until it is placed
static int position(CGFloat v) {
    return isfinite(v) ? (int)v : 0;
}

// WindowServer's calls, looked up so QuartzCore does not link it
static void *ws(const char *name) {
    return dlsym(RTLD_DEFAULT, name);
}

@implementation CAContext {
    CALayer *_layer;
    CAContextID _contextId;
}

+ (instancetype)remoteContextWithOptions:(NSDictionary *)options {
    return [[[self alloc] init] autorelease];
}

+ (instancetype)remoteContext {
    return [self remoteContextWithOptions:nil];
}

- (void)dealloc {
    [_layer release];
    [super dealloc];
}

- (CALayer *)layer {
    return _layer;
}

// the id names the layer's own surface: a CAMetalLayer's, made now if it has none, so the layer can be drawn before
// any window shows it
- (void)setLayer:(CALayer *)layer {
    uint32_t (*export)(void *) = ws("_windowServerExportSurface");
    void *surface = NULL;

    if (layer == _layer)
        return;
    [layer retain];
    [_layer release];
    _layer = layer;
    _contextId = 0;
    if ([layer isKindOfClass:[CAMetalLayer class]])
        surface = [(CAMetalLayer *)layer _pdWaylandSurface:NULL];
    if (surface && export)
        _contextId = export(surface);
    if (traceOn())
        fprintf(stderr, "CAContext %p: layer %p surface %p -> context %u\n", (void *)self, (void *)layer, surface, _contextId);
}

- (CAContextID)contextId {
    return _contextId;
}

- (void)invalidate {
    [_layer release];
    _layer = nil;
    _contextId = 0;
}

@end

// another process's context shown where this layer sits: a subsurface of the hosting view's window
@implementation CALayerHost {
    CAContextID _contextId;
    void *_subsurface;
    NSInteger _window;
    CGPoint _origin;
}

- (void)dealloc {
    [self _pdDetach];
    [super dealloc];
}

- (CAContextID)contextId {
    return _contextId;
}

// a new id is a new subsurface, at the same place in the same window
- (void)setContextId:(CAContextID)contextId {
    NSInteger window = _window;
    BOOL attached = _subsurface != NULL;

    if (traceOn())
        fprintf(stderr, "CALayerHost %p: context %u (attached %d)\n", (void *)self, contextId, attached);
    if (contextId == _contextId)
        return;
    _contextId = contextId;
    if (attached) {
        [self _pdDetach];
        [self _pdAttachToWindowNumber:window origin:_origin size:CGSizeZero];
    }
}

- (BOOL)_pdAttachToWindowNumber:(NSInteger)window origin:(CGPoint)origin size:(CGSize)size {
    void *(*import)(int, uint32_t, int, int) = ws("_windowServerImportSubsurface");

    _window = window, _origin = origin;
    if (_subsurface || !_contextId || !import)
        return _subsurface != NULL;
    _subsurface = import((int)window, _contextId, position(origin.x), position(origin.y));
    if (traceOn())
        fprintf(stderr, "CALayerHost %p: context %u in window %ld at %g,%g -> %p\n", (void *)self, _contextId, (long)window,
                origin.x, origin.y, _subsurface);
    return _subsurface != NULL;
}

- (void)_pdDetach {
    void (*destroy)(void *, void *) = ws("_windowServerDestroySubsurface");

    if (_subsurface && destroy)
        destroy(NULL, _subsurface);
    _subsurface = NULL;
}

- (BOOL)_pdIsAttached {
    return _subsurface != NULL;
}

- (void)setWindowOrigin:(CGPoint)origin {
    void (*move)(int, void *, int, int) = ws("_windowServerMoveSubsurface");

    _origin = origin;
    if (traceOn())
        fprintf(stderr, "CALayerHost %p: context %u moves to %g,%g\n", (void *)self, _contextId, origin.x, origin.y);
    if (_subsurface && move)
        move((int)_window, _subsurface, position(origin.x), position(origin.y));
}

// the drawing process sizes its surface, the host only places it
- (void)_pdSetViewSize:(CGSize)size {
}

@end
