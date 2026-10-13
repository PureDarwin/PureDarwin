/*
 * main.m - pddecor: title bars for Wayland windows that are not AppKit's, drawn by AppKit.
 *
 * The compositor reports each window that asked for server side decorations over
 * pd-decoration-v1. pddecor draws the top of an NSThemeFrame for it, the same code
 * that frames AppKit's own windows, into a wl_shm buffer and hands it back. The
 * compositor shows it above the window and handles drags and the window buttons.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#import <AppKit/AppKit.h>
#import <Onyx2D/O2Context.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <dispatch/dispatch.h>
#include <wayland-client.h>
#include "pd-decoration-v1-client-protocol.h"

// AppKit's own frame measurements, so the buttons land where the compositor is told they are
extern const float NSWindowTitleHeight;
extern const float NSWindowControlDiameter;
extern const float NSWindowControlSpacing;
extern const float NSWindowControlInset;

@interface NSThemeFrame : NSView
- (void)_pdDrawFrameInContext:(void *)context;
@end

// never shown, it only carries the state the frame draws from
@interface DecorWindow : NSWindow {
@public
    BOOL _decorActive;
}
@end

@implementation DecorWindow
- (BOOL)isKeyWindow
{
    return _decorActive;
}
@end

// one compositor window and the strip last sent for it
@interface Decorated : NSObject {
@public
    uint32_t wid;
    NSString *title;
    uint32_t width;
    BOOL active;
    NSString *drawn;
    DecorWindow *window;
}
@end

@implementation Decorated
- (void)dealloc
{
    [title release];
    [drawn release];
    [window release];
    [super dealloc];
}
@end

static struct wl_display *display;
static struct wl_shm *shm;
static struct pd_decorator_v1 *decorator;
static NSMutableDictionary *windows;

static BOOL dark(void)
{
    CFPropertyListRef style = CFPreferencesCopyAppValue(CFSTR("AppleInterfaceStyle"), kCFPreferencesAnyApplication);

    if (style == NULL)
        return NO;
    CFRelease(style);
    return YES;
}

// the compositor is done reading a strip once it releases it
static void buffer_release(void *data, struct wl_buffer *buffer)
{
    wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = { buffer_release };

// a shared memory file nobody else can open, the compositor gets it through the pool
static int shm_file(size_t size)
{
    char name[64];
    int fd;

    for (int i = 0; i < 100; i++) {
        snprintf(name, sizeof(name), "/pddecor-%d-%d", getpid(), arc4random() & 0xffffff);
        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            if (ftruncate(fd, (off_t)size) == 0)
                return fd;
            close(fd);
            return -1;
        }
    }
    return -1;
}

static void render(Decorated *d)
{
    BOOL isDark = dark();
    NSString *key = [NSString stringWithFormat:@"%@|%u|%d|%d", d->title ?: @"", d->width, d->active, isDark];
    int w = (int)d->width, h = (int)NSWindowTitleHeight;
    size_t stride = (size_t)w * 4, size = stride * h;

    if (w <= 0 || [key isEqualToString:d->drawn])
        return;
    if (d->window == nil) {
        d->window = [[DecorWindow alloc] initWithContentRect:NSMakeRect(0, 0, w, 1)
            styleMask:NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask | NSResizableWindowMask
            backing:NSBackingStoreBuffered defer:YES];
    }
    d->window->_decorActive = d->active;
    [d->window setTitle:d->title ?: @""];
    [d->window setAppearance:[NSAppearance appearanceNamed:isDark ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua]];
    [d->window setContentSize:NSMakeSize(w, 1)];

    NSView *frame = [[d->window contentView] superview];
    int fh = (int)NSHeight([frame bounds]);
    unsigned char *rgba = calloc(size, 1);
    unsigned char *planes[1] = { rgba };
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:planes pixelsWide:w pixelsHigh:h
        bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace
        bytesPerRow:(int)stride bitsPerPixel:32];
    NSGraphicsContext *ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:ctx];
    // only the top of the frame lands in the strip
    O2ContextTranslateCTM((O2ContextRef)[ctx graphicsPort], 0, -(fh - h));
    [(NSThemeFrame *)frame _pdDrawFrameInContext:[ctx graphicsPort]];
    [ctx flushGraphics];
    [NSGraphicsContext restoreGraphicsState];

    int fd = shm_file(size);
    if (fd < 0) {
        [rep release];
        free(rgba);
        return;
    }
    unsigned char *map = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        close(fd);
        [rep release];
        free(rgba);
        return;
    }
    // the bitmap is premultiplied rgba, wl_shm argb8888 is premultiplied bgra in memory
    for (size_t i = 0; i < size; i += 4) {
        map[i] = rgba[i + 2];
        map[i + 1] = rgba[i + 1];
        map[i + 2] = rgba[i];
        map[i + 3] = rgba[i + 3];
    }
    munmap(map, size);
    [rep release];
    free(rgba);

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
    struct wl_buffer *buffer = wl_shm_pool_create_buffer(pool, 0, w, h, (int32_t)stride, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    wl_buffer_add_listener(buffer, &buffer_listener, NULL);

    uint32_t step = (uint32_t)(NSWindowControlDiameter + NSWindowControlSpacing);
    uint32_t inset = (uint32_t)NSWindowControlInset, dia = (uint32_t)NSWindowControlDiameter;
    pd_decorator_v1_set_frame(decorator, d->wid, buffer, (uint32_t)h, (uint32_t)(h / 2 - dia / 2), dia,
                              inset, inset + step, inset + 2 * step);
    wl_display_flush(display);
    [d->drawn release];
    d->drawn = [key retain];
}

static Decorated *find(uint32_t wid)
{
    return windows[@(wid)];
}

static void on_window(void *data, struct pd_decorator_v1 *dec, uint32_t wid)
{
    Decorated *d = [[[Decorated alloc] init] autorelease];

    d->wid = wid;
    windows[@(wid)] = d;
}

static void on_title(void *data, struct pd_decorator_v1 *dec, uint32_t wid, const char *title)
{
    Decorated *d = find(wid);

    if (d == nil)
        return;
    [d->title release];
    d->title = [[NSString alloc] initWithUTF8String:title ?: ""];
}

static void on_width(void *data, struct pd_decorator_v1 *dec, uint32_t wid, uint32_t width)
{
    Decorated *d = find(wid);

    if (d != nil)
        d->width = width;
}

static void on_active(void *data, struct pd_decorator_v1 *dec, uint32_t wid, uint32_t active)
{
    Decorated *d = find(wid);

    if (d != nil)
        d->active = active != 0;
}

static void on_done(void *data, struct pd_decorator_v1 *dec, uint32_t wid)
{
    Decorated *d = find(wid);

    if (d != nil)
        render(d);
}

static void on_removed(void *data, struct pd_decorator_v1 *dec, uint32_t wid)
{
    [windows removeObjectForKey:@(wid)];
}

static const struct pd_decorator_v1_listener decorator_listener = {
    on_window, on_title, on_width, on_active, on_done, on_removed,
};

static void global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if (strcmp(interface, wl_shm_interface.name) == 0)
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    else if (strcmp(interface, pd_decorator_v1_interface.name) == 0)
        decorator = wl_registry_bind(registry, name, &pd_decorator_v1_interface, 1);
}

static void global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = { global, global_remove };

@interface Decor : NSObject <NSApplicationDelegate>
@end

@implementation Decor

// a change of appearance redraws every strip
- (void)themeChanged:(NSNotification *)note
{
    for (Decorated *d in [windows allValues]) {
        [d->drawn release];
        d->drawn = nil;
        render(d);
    }
}

- (void)applicationDidFinishLaunching:(NSNotification *)note
{
    [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(themeChanged:)
                                                            name:@"AppleInterfaceThemeChangedNotification" object:nil];
    dispatch_source_t src = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, (uintptr_t)wl_display_get_fd(display), 0,
                                                   dispatch_get_main_queue());
    dispatch_source_set_event_handler(src, ^{
        if (wl_display_dispatch(display) < 0)
            exit(1);
        wl_display_flush(display);
    });
    dispatch_resume(src);
    wl_display_flush(display);
}

@end

int main(int argc, const char *argv[])
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    struct wl_registry *registry;

    // the session starts this alongside the compositor, so the display may not be listening yet
    for (int i = 0; i < 80 && (display = wl_display_connect(NULL)) == NULL; i++)
        usleep(250000);
    if (display == NULL) {
        fprintf(stderr, "pddecor: no wayland display\n");
        return 1;
    }
    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (decorator == NULL || shm == NULL) {
        fprintf(stderr, "pddecor: the compositor has no pd_decorator_v1\n");
        return 1;
    }
    windows = [[NSMutableDictionary alloc] init];
    pd_decorator_v1_add_listener(decorator, &decorator_listener, NULL);
    wl_display_roundtrip(display);

    NSApplication *app = [NSApplication sharedApplication];
    Decor *delegate = [[Decor alloc] init];
    // a helper with no windows, kept out of the Dock and the menu bar
    [app setActivationPolicy:NSApplicationActivationPolicyAccessory];
    [app setDelegate:delegate];
    [app run];
    [pool release];
    return 0;
}
