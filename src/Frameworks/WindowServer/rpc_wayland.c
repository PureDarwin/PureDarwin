/*
 * rpc_wayland.c - the window-server RPCs, served over Wayland.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <WindowServer/rpc.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "linux-dmabuf-v1-client-protocol.h"
#include <CoreGraphics/CGDirectDisplay_puredarwin.h>
#include <CoreGraphics/CGWindowLevel.h>
#include <CoreFoundation/CFBundle.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <pthread.h>
#include <xkbcommon/xkbcommon.h>

struct wsWindow {
    int windowID;
    struct wl_surface *surface;
    struct xdg_surface *xdgSurface;
    struct xdg_toplevel *toplevel;
    struct zwlr_layer_surface_v1 *layerSurface;
    int level;
    struct wl_shm_pool *pool;
    struct wl_buffer *buffer;
    void *pixels;
    size_t pixelsSize;
    int shmFd;
    double x, y, w, h;
    int state;
    int configured;
    char title[64];
    /* Last anchor/margins actually sent, so an unchanged layout does not
     * re-issue the requests. */
    uint32_t sentAnchor;
    int32_t sentMarginTop, sentMarginRight, sentMarginBottom, sentMarginLeft;
    int haveSentAnchor;
    struct wsWindow *next;
};

static struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wmBase;
    struct zwlr_layer_shell_v1 *layerShell;
    uint32_t layerShellVersion;
    struct wl_seat *seat;
    struct wl_output *output;
    struct wl_shm *shm;
    /* NULL when the compositor does not advertise linux-dmabuf, which is the
     * normal case on a framebuffer-only setup; the shm path is used then. */
    struct zwp_linux_dmabuf_v1 *dmabuf;
    struct wl_keyboard *keyboard;
    struct wl_pointer *pointer;
    struct xkb_context *xkbContext;
    struct xkb_keymap *xkbKeymap;
    struct xkb_state *xkbState;
    struct mach_event queue[64];
    unsigned queueHead;
    unsigned queueTail;
    pthread_mutex_t queueLock;
    pthread_cond_t queueReady;
    pthread_t dispatchThread;
    int dispatchRunning;
    int focusWindowID;
    /* Panels, the dock and the desktop take pointer focus too, so the window
     * reported as active is only ever an xdg toplevel. */
    int activeAppWindowID;
    uint32_t pointerButtonSerial;
    struct wsWindow *windows;
    int connected;
    int lastMouseX, lastMouseY;
    int screenW, screenH;
} ws;

static struct wsWindow *windowForID(int windowID) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->windowID == windowID) {
            return w;
        }
    }
    return NULL;
}

/* xdg_wm_base pings must be answered or the compositor kills the client. */
static void wmBasePing(void *data, struct xdg_wm_base *base, uint32_t serial) {
    xdg_wm_base_pong(base, serial);
}

static const struct xdg_wm_base_listener wmBaseListener = {
    .ping = wmBasePing,
};

static void outputMode(void *data, struct wl_output *output, uint32_t flags,
                       int32_t width, int32_t height, int32_t refresh) {
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        /* CoreGraphics cannot discover displays itself; hand it the real
         * geometry now that the compositor has told us. */
        ws.screenW = width;
        ws.screenH = height;
        CGDirectDisplaySetMainDisplayBounds(CGRectMake(0, 0, width, height));
    }
}

static void outputGeometry(void *data, struct wl_output *output, int32_t x, int32_t y,
                           int32_t physWidth, int32_t physHeight, int32_t subpixel,
                           const char *make, const char *model, int32_t transform) {
}

static void outputDone(void *data, struct wl_output *output) {
}

static void outputScale(void *data, struct wl_output *output, int32_t factor) {
}

static const struct wl_output_listener outputListener = {
    .geometry = outputGeometry,
    .mode = outputMode,
    .done = outputDone,
    .scale = outputScale,
};

/* Defined with the rest of the input handling, below. */
static const struct wl_seat_listener seatListener;
static void *dispatchThreadMain(void *unused);
static int windowCreateBuffer(struct wsWindow *window);
static int windowAttachBuffer(struct wsWindow *window);
static void windowReleaseBuffer(struct wsWindow *window);

static void registryGlobal(void *data, struct wl_registry *registry, uint32_t name,
                           const char *interface, uint32_t version) {
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        ws.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        ws.wmBase = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(ws.wmBase, &wmBaseListener, NULL);
    } else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        /* Version 4 is the first with on-demand keyboard focus, which is what
         * a panel holding a text field needs: at version 1 the only choices
         * are never and exclusive, so a search field could never take focus. */
        uint32_t bind = (version < 4) ? version : 4;

        ws.layerShellVersion = bind;
        ws.layerShell = wl_registry_bind(registry, name,
                                         &zwlr_layer_shell_v1_interface, bind);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        ws.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, zwp_linux_dmabuf_v1_interface.name) == 0) {
        /* Version 3 is the highest that still offers create_params/create_immed
         * without requiring the dmabuf_feedback flow added in 4. */
        uint32_t bind = (version < 3) ? version : 3;

        ws.dmabuf = wl_registry_bind(registry, name, &zwp_linux_dmabuf_v1_interface, bind);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        ws.seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
        /* wl_seat sends capabilities as soon as the bind is processed. Attach
         * before the enclosing roundtrip returns or that initial event is
         * dispatched without a listener and is not sent again. */
        wl_seat_add_listener(ws.seat, &seatListener, NULL);
    } else if (strcmp(interface, wl_output_interface.name) == 0) {
        ws.output = wl_registry_bind(registry, name, &wl_output_interface, 2);
        wl_output_add_listener(ws.output, &outputListener, NULL);
    }
}

static void registryGlobalRemove(void *data, struct wl_registry *registry, uint32_t name) {
}

static const struct wl_registry_listener registryListener = {
    .global = registryGlobal,
    .global_remove = registryGlobalRemove,
};

static int ensureConnected(void) {
    if (ws.connected) {
        return 1;
    }

    /* launchd starts clients and the compositor independently. Match the
     * standalone Wayland probe's startup tolerance so an AppKit process does
     * not fail merely because neuswc has not created its socket yet. */
    for (unsigned attempt = 0; attempt < 1200; ++attempt) {
        ws.display = wl_display_connect(NULL);
        if (ws.display != NULL) {
            break;
        }
        usleep(50000);
    }
    if (ws.display == NULL) {
        fprintf(stderr, "WindowServer: timed out waiting for the Wayland display\n");
        return 0;
    }

    ws.registry = wl_display_get_registry(ws.display);
    wl_registry_add_listener(ws.registry, &registryListener, NULL);

    /* Two rounds: the first delivers the globals, the second the events those
     * globals emit (notably wl_output's mode). */
    wl_display_roundtrip(ws.display);
    wl_display_roundtrip(ws.display);

    if (ws.compositor == NULL || ws.wmBase == NULL || ws.shm == NULL) {
        fprintf(stderr, "WindowServer: compositor lacks wl_compositor, xdg_wm_base or wl_shm\n");
        return 0;
    }

    pthread_mutex_init(&ws.queueLock, NULL);
    pthread_cond_init(&ws.queueReady, NULL);
    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr, "WindowServer: linux-dmabuf %s\n",
                (ws.dmabuf != NULL) ? "available (GPU buffers possible)"
                                    : "absent (shm path)");
    }

    ws.connected = 1;
    ws.dispatchRunning = 1;
    pthread_create(&ws.dispatchThread, NULL, dispatchThreadMain, NULL);
    return 1;
}

static void xdgSurfaceConfigure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct wsWindow *window = data;

    xdg_surface_ack_configure(surface, serial);
    /* The role now exists, so the buffer can go on the surface: either the one
     * created with the window, or a fresh one after a resize. A new role starts
     * unmapped, so this attach is what makes the surface appear at all. */
    windowAttachBuffer(window);
    wl_surface_commit(window->surface);

    pthread_mutex_lock(&ws.queueLock);
    window->configured = 1;
    pthread_cond_broadcast(&ws.queueReady);
    pthread_mutex_unlock(&ws.queueLock);
}

static const struct xdg_surface_listener xdgSurfaceListener = {
    .configure = xdgSurfaceConfigure,
};

static void layerSurfaceConfigure(void *data, struct zwlr_layer_surface_v1 *surface,
                                  uint32_t serial, uint32_t width, uint32_t height) {
    struct wsWindow *window = data;

    zwlr_layer_surface_v1_ack_configure(surface, serial);

    /* The compositor sizes background layers to the output; take what it
     * gives us so the backing store matches the surface. */
    if (width > 0 && height > 0 &&
        ((double)width != window->w || (double)height != window->h)) {
        window->w = width;
        window->h = height;
        windowReleaseBuffer(window);
    }
    /* The role now exists, so the buffer can go on the surface: either the one
     * created with the window, or a fresh one after a resize. A new role starts
     * unmapped, so this attach is what makes the surface appear at all. */
    windowAttachBuffer(window);
    wl_surface_commit(window->surface);

    pthread_mutex_lock(&ws.queueLock);
    window->configured = 1;
    pthread_cond_broadcast(&ws.queueReady);
    pthread_mutex_unlock(&ws.queueLock);
}

static void layerSurfaceClosed(void *data, struct zwlr_layer_surface_v1 *surface) {
}

static const struct zwlr_layer_surface_v1_listener layerSurfaceListener = {
    .configure = layerSurfaceConfigure,
    .closed = layerSurfaceClosed,
};

/* AppKit window levels are CGWindowLevelKey values, so desktop sorts below
 * normal and the dock/menu levels above it. Anything that maps to a layer is
 * given a layer-shell role instead of an xdg_toplevel. */
static int layerForWindowLevel(int level, uint32_t *layerOut) {
    if (ws.layerShell == NULL) {
        return 0;
    }
    if (level <= kCGDesktopWindowLevelKey) {
        *layerOut = ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND;
        return 1;
    }
    /* The dock sits on the TOP layer; the menu bar and anything above it are
     * overlays, which is both what they are semantically and what keeps them
     * clear of the dock's stacking. */
    if (level >= kCGMainMenuWindowLevelKey) {
        *layerOut = ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY;
        return 1;
    }
    if (level >= kCGDockWindowLevelKey) {
        *layerOut = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
        return 1;
    }
    return 0;
}

static void windowApplyLayerGeometry(struct wsWindow *window, uint32_t layer) {
    /* Background surfaces cover the output; nothing to derive. */
    if (layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
        zwlr_layer_surface_v1_set_anchor(window->layerSurface,
            ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
            ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
        zwlr_layer_surface_v1_set_exclusive_zone(window->layerSurface, -1);
        return;
    }

    zwlr_layer_surface_v1_set_size(window->layerSurface,
                                   (uint32_t)window->w, (uint32_t)window->h);

    if (ws.screenW <= 0 || ws.screenH <= 0) {
        if (getenv("PD_WS_TRACE") != NULL) {
            fprintf(stderr,
                    "WindowServer: surface %d NOT ANCHORED - screen size unknown\n",
                    window->windowID);
        }
        return;
    }

    /* AppKit frames are bottom-left origin, which is also the edge sense the
     * anchor bits use, so the gaps map across directly. */
    double leftGap   = window->x;
    double rightGap  = (double)ws.screenW - (window->x + window->w);
    double bottomGap = window->y;
    double topGap    = (double)ws.screenH - (window->y + window->h);

    /* A window is "against" an edge when it is nearer to it than to the
     * opposite one by more than a pixel; otherwise it wants centring. */
    const double slack = 1.0;
    uint32_t anchor = 0;
    int32_t marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;

    if (leftGap + slack < rightGap) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT;
        marginLeft = (int32_t)leftGap;
    } else if (rightGap + slack < leftGap) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        marginRight = (int32_t)rightGap;
    }

    if (bottomGap + slack < topGap) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM;
        marginBottom = (int32_t)bottomGap;
    } else if (topGap + slack < bottomGap) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP;
        marginTop = (int32_t)topGap;
    }

    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr,
                "WindowServer: surface %d anchor=0x%x margins t=%d r=%d b=%d l=%d\n",
                window->windowID, anchor, marginTop, marginRight, marginBottom,
                marginLeft);
    }

    /* Every set_anchor/set_margin pair makes the compositor reconfigure the
     * layer surface, and a reconfigure costs a repaint. This is reached on
     * ordinary state updates, so re-sending values that have not changed made
     * the menu bar redraw continuously - visible as flicker. */
    if (window->haveSentAnchor &&
        window->sentAnchor == anchor &&
        window->sentMarginTop == marginTop &&
        window->sentMarginRight == marginRight &&
        window->sentMarginBottom == marginBottom &&
        window->sentMarginLeft == marginLeft) {
        return;
    }

    window->sentAnchor = anchor;
    window->sentMarginTop = marginTop;
    window->sentMarginRight = marginRight;
    window->sentMarginBottom = marginBottom;
    window->sentMarginLeft = marginLeft;
    window->haveSentAnchor = 1;

    zwlr_layer_surface_v1_set_anchor(window->layerSurface, anchor);
    zwlr_layer_surface_v1_set_margin(window->layerSurface, marginTop, marginRight,
                                     marginBottom, marginLeft);
}

static void windowShmPath(int windowID, char *out, size_t outSize) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFStringRef identifier = (bundle != NULL) ? CFBundleGetIdentifier(bundle) : NULL;
    char bundleID[192];

    if (identifier == NULL ||
        !CFStringGetCString(identifier, bundleID, sizeof(bundleID), kCFStringEncodingUTF8)) {
        snprintf(bundleID, sizeof(bundleID), "unix.%u", (unsigned)getpid());
    }
    wsWindowShmPath(bundleID, (unsigned)getpid(), (unsigned)windowID, out,
                    outSize);
}

/* Backing store sized to the frame, ARGB8888 - which is what AppKit's
 * O2Surface writes (premultiplied-first, host byte order). */
static int windowCreateBuffer(struct wsWindow *window) {
    int width = (int)window->w;
    int height = (int)window->h;

    if (width <= 0 || height <= 0) {
        return 0;
    }

    int stride = width * 4;
    size_t size = (size_t)stride * (size_t)height;
    char path[256];

    windowShmPath(window->windowID, path, sizeof(path));
    shm_unlink(path);

    window->shmFd = shm_open(path, O_CREAT | O_RDWR, 0600);
    if (window->shmFd < 0) {
        fprintf(stderr, "WindowServer: shm_open(%s) failed\n", path);
        return 0;
    }
    if (ftruncate(window->shmFd, (off_t)size) != 0) {
        close(window->shmFd);
        window->shmFd = -1;
        return 0;
    }

    window->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED,
                          window->shmFd, 0);
    if (window->pixels == MAP_FAILED) {
        window->pixels = NULL;
        close(window->shmFd);
        window->shmFd = -1;
        return 0;
    }
    window->pixelsSize = size;

    window->pool = wl_shm_create_pool(ws.shm, window->shmFd, (int32_t)size);
    window->buffer = wl_shm_pool_create_buffer(window->pool, 0, width, height,
                                               stride, WL_SHM_FORMAT_ARGB8888);
    return 1;
}

/* Creates the buffer if needed and puts it on the surface. Only safe once the
 * surface has a role. */
static int windowAttachBuffer(struct wsWindow *window) {
    if (window->buffer == NULL && !windowCreateBuffer(window)) {
        return 0;
    }

    wl_surface_attach(window->surface, window->buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, (int)window->w, (int)window->h);
    return 1;
}

static void windowReleaseBuffer(struct wsWindow *window) {
    if (window->buffer != NULL) {
        wl_buffer_destroy(window->buffer);
        window->buffer = NULL;
    }
    if (window->pool != NULL) {
        wl_shm_pool_destroy(window->pool);
        window->pool = NULL;
    }
    if (window->pixels != NULL) {
        munmap(window->pixels, window->pixelsSize);
        window->pixels = NULL;
        window->pixelsSize = 0;
    }
    if (window->shmFd >= 0) {
        close(window->shmFd);
        window->shmFd = -1;
    }
}

/* The NSEventType values NSApplication's translation switches on. Spelled out
 * here because AppKit's headers are Objective-C and this file is not. */
enum {
    WS_LEFT_MOUSE_DOWN  = 1,
    WS_LEFT_MOUSE_UP    = 2,
    WS_RIGHT_MOUSE_DOWN = 3,
    WS_RIGHT_MOUSE_UP   = 4,
    WS_MOUSE_MOVED      = 5,
    WS_KEY_DOWN         = 10,
    WS_KEY_UP           = 11,
};

static void queuePush(const struct mach_event *event) {
    pthread_mutex_lock(&ws.queueLock);

    unsigned next = (ws.queueTail + 1) % (unsigned)(sizeof(ws.queue) / sizeof(ws.queue[0]));
    if (next != ws.queueHead) {
        ws.queue[ws.queueTail] = *event;
        ws.queueTail = next;
    }
    /* A full queue drops the newest event rather than blocking the compositor
     * thread; input is stale by then anyway. */
    pthread_cond_signal(&ws.queueReady);
    pthread_mutex_unlock(&ws.queueLock);
}

static double lastPointerX;
static double lastPointerY;
static unsigned currentModifiers;

static void pointerEnter(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface) {
            ws.focusWindowID = w->windowID;
            if (w->toplevel != NULL) {
                ws.activeAppWindowID = w->windowID;
            }
            break;
        }
    }
    lastPointerX = wl_fixed_to_double(x);
    lastPointerY = wl_fixed_to_double(y);
}

static void pointerLeave(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface) {
}

static void pointerMotion(void *data, struct wl_pointer *pointer, uint32_t time,
                          wl_fixed_t x, wl_fixed_t y) {
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(ws.focusWindowID);
    double newX = wl_fixed_to_double(x);
    double newY = wl_fixed_to_double(y);

    event.windowID = ws.focusWindowID;
    event.code = WS_MOUSE_MOVED;
    /* AppKit's event bridge accepts screen coordinates and converts them back
     * to window coordinates. Wayland pointer coordinates are surface-local. */
    event.x = newX + (window != NULL ? window->x : 0);
    event.y = window != NULL ? window->y + window->h - newY : newY;
    event.dx = newX - lastPointerX;
    event.dy = lastPointerY - newY;
    event.mods = currentModifiers;
    lastPointerX = newX;
    lastPointerY = newY;
    queuePush(&event);
}

static void pointerButton(void *data, struct wl_pointer *pointer, uint32_t serial,
                          uint32_t time, uint32_t button, uint32_t state) {
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(ws.focusWindowID);
    int pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

    if (pressed) {
        ws.pointerButtonSerial = serial;
    }

    /* linux/input-event-codes.h: BTN_LEFT 0x110, BTN_RIGHT 0x111. */
    if (button == 0x110) {
        event.code = pressed ? WS_LEFT_MOUSE_DOWN : WS_LEFT_MOUSE_UP;
    } else if (button == 0x111) {
        event.code = pressed ? WS_RIGHT_MOUSE_DOWN : WS_RIGHT_MOUSE_UP;
    } else {
        return;
    }
    event.windowID = ws.focusWindowID;
    event.x = lastPointerX + (window != NULL ? window->x : 0);
    event.y = window != NULL
        ? window->y + window->h - lastPointerY : lastPointerY;
    event.mods = currentModifiers;
    queuePush(&event);
}

static void pointerAxis(void *data, struct wl_pointer *pointer, uint32_t time,
                        uint32_t axis, wl_fixed_t value) {
}

static void pointerFrame(void *data, struct wl_pointer *pointer) {
}

static void pointerAxisSource(void *data, struct wl_pointer *pointer,
                              uint32_t source) {
}

static void pointerAxisStop(void *data, struct wl_pointer *pointer,
                            uint32_t time, uint32_t axis) {
}

static void pointerAxisDiscrete(void *data, struct wl_pointer *pointer,
                                uint32_t axis, int32_t discrete) {
}

static const struct wl_pointer_listener pointerListener = {
    .enter = pointerEnter,
    .leave = pointerLeave,
    .motion = pointerMotion,
    .button = pointerButton,
    .axis = pointerAxis,
    .frame = pointerFrame,
    .axis_source = pointerAxisSource,
    .axis_stop = pointerAxisStop,
    .axis_discrete = pointerAxisDiscrete,
};

static void keyboardKeymap(void *data, struct wl_keyboard *keyboard, uint32_t format,
                           int fd, uint32_t size) {
    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    char *text = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (text == MAP_FAILED) {
        close(fd);
        return;
    }
    if (ws.xkbContext == NULL) {
        ws.xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    }
    if (ws.xkbKeymap != NULL) {
        xkb_keymap_unref(ws.xkbKeymap);
    }
    ws.xkbKeymap = xkb_keymap_new_from_string(ws.xkbContext, text,
        XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (ws.xkbState != NULL) {
        xkb_state_unref(ws.xkbState);
    }
    ws.xkbState = (ws.xkbKeymap != NULL) ? xkb_state_new(ws.xkbKeymap) : NULL;

    munmap(text, size);
    close(fd);
}

static void keyboardEnter(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                          struct wl_surface *surface, struct wl_array *keys) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface) {
            struct mach_event event = {0};

            ws.focusWindowID = w->windowID;
            if (w->toplevel != NULL) {
                ws.activeAppWindowID = w->windowID;
            }
            /* Recording the focus is not enough: AppKit has to be told, or no
             * window ever becomes key and keystrokes have nowhere to go. */
            event.windowID = w->windowID;
            event.code = WS_EVENT_FOCUS_GAINED;
            queuePush(&event);
            break;
        }
    }
}

static void keyboardLeave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                          struct wl_surface *surface) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface) {
            struct mach_event event = {0};

            event.windowID = w->windowID;
            event.code = WS_EVENT_FOCUS_LOST;
            queuePush(&event);
            break;
        }
    }
}

static void keyboardKey(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                        uint32_t time, uint32_t key, uint32_t state) {
    struct mach_event event = {0};
    /* Wayland reports evdev codes; xkb keycodes are evdev + 8. */
    xkb_keycode_t keycode = key + 8;

    event.windowID = ws.focusWindowID;
    event.code = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? WS_KEY_DOWN : WS_KEY_UP;
    event.keycode = (unsigned short)key;
    event.mods = currentModifiers;
    event.x = lastPointerX;
    event.y = lastPointerY;

    if (ws.xkbState != NULL) {
        xkb_state_key_get_utf8(ws.xkbState, keycode, event.chars, sizeof(event.chars));
        /* charactersIgnoringModifiers: same text without the active modifiers
         * would need a second state; the unshifted text is close enough for
         * the key-equivalent matching AppKit does with it. */
        memcpy(event.charsIg, event.chars, sizeof(event.charsIg));
    }
    queuePush(&event);
}

static void keyboardModifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                              uint32_t depressed, uint32_t latched, uint32_t locked,
                              uint32_t group) {
    if (ws.xkbState != NULL) {
        xkb_state_update_mask(ws.xkbState, depressed, latched, locked, 0, 0, group);
    }

    /* NSEvent modifier flags. */
    currentModifiers = 0;
    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr, "WindowServer: modifiers dep=0x%x lat=0x%x lock=0x%x "
                        "group=%u xkbState=%p\n",
                depressed, latched, locked, group, (void *)ws.xkbState);
    }
    if (ws.xkbState != NULL) {
        if (xkb_state_mod_name_is_active(ws.xkbState, XKB_MOD_NAME_SHIFT,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            currentModifiers |= (1 << 17);
        }
        if (xkb_state_mod_name_is_active(ws.xkbState, XKB_MOD_NAME_CTRL,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            currentModifiers |= (1 << 18);
        }
        if (xkb_state_mod_name_is_active(ws.xkbState, XKB_MOD_NAME_ALT,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            currentModifiers |= (1 << 19);
        }
        if (xkb_state_mod_name_is_active(ws.xkbState, XKB_MOD_NAME_LOGO,
                                         XKB_STATE_MODS_EFFECTIVE) > 0) {
            currentModifiers |= (1 << 20);
        }
    }
    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr, "WindowServer: modifiers -> NSEvent mods 0x%x\n",
                currentModifiers);
    }
}

static void keyboardRepeatInfo(void *data, struct wl_keyboard *keyboard,
                               int32_t rate, int32_t delay) {
}

static const struct wl_keyboard_listener keyboardListener = {
    .keymap = keyboardKeymap,
    .enter = keyboardEnter,
    .leave = keyboardLeave,
    .key = keyboardKey,
    .modifiers = keyboardModifiers,
    .repeat_info = keyboardRepeatInfo,
};

static void seatCapabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && ws.keyboard == NULL) {
        ws.keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(ws.keyboard, &keyboardListener, NULL);
    }
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && ws.pointer == NULL) {
        ws.pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(ws.pointer, &pointerListener, NULL);
    }
}

static void seatName(void *data, struct wl_seat *seat, const char *name) {
}

static const struct wl_seat_listener seatListener = {
    .capabilities = seatCapabilities,
    .name = seatName,
};

/* Owns the Wayland connection's read side so input arrives without the app
 * having to poll. */
static void *dispatchThreadMain(void *unused) {
    while (ws.dispatchRunning) {
        if (wl_display_dispatch(ws.display) < 0) {
            fprintf(stderr,"WindowServer: Wayland dispatch stopped with error %d\n",
                    wl_display_get_error(ws.display));
            break;
        }
    }
    ws.dispatchRunning = 0;

    /* Unblock a waiter so it can see the connection is gone. */
    pthread_mutex_lock(&ws.queueLock);
    pthread_cond_broadcast(&ws.queueReady);
    pthread_mutex_unlock(&ws.queueLock);
    return NULL;
}

int _windowServerConnect(void) {
    return ensureConnected();
}

int _windowServerReceiveMessage(PortMessage *msg) {
    if (msg == NULL || !ws.connected) {
        return 0;
    }

    pthread_mutex_lock(&ws.queueLock);
    while (ws.queueHead == ws.queueTail && ws.dispatchRunning) {
        pthread_cond_wait(&ws.queueReady, &ws.queueLock);
    }
    if (ws.queueHead == ws.queueTail) {
        pthread_mutex_unlock(&ws.queueLock);
        return 0;
    }

    struct mach_event event = ws.queue[ws.queueHead];
    ws.queueHead = (ws.queueHead + 1) % (unsigned)(sizeof(ws.queue) / sizeof(ws.queue[0]));
    pthread_mutex_unlock(&ws.queueLock);

    memset(msg, 0, sizeof(*msg));
    msg->header.msgh_id = MSG_ID_INLINE;
    msg->code = CODE_INPUT_EVENT;
    msg->len = (int)sizeof(event);
    msg->pid = (int)getpid();
    memcpy(msg->data, &event, sizeof(event));
    return 1;
}

/* Gives the surface its shell role. A window whose level maps to a layer gets
 * a layer-shell surface; everything else gets an xdg_toplevel. The role cannot
 * be changed in place, so a level change tears this down and rebuilds it. */
static void windowCreateRole(struct wsWindow *window, const char *title) {
    uint32_t layer;

    /* PD_WS_TRACE names every window the compositor is asked to show, which is
     * the only way to tell whose surface an unexpected rectangle belongs to. */
    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr,
                "WindowServer: window %d level %d frame {{%g, %g}, {%g, %g}} title \"%s\"\n",
                window->windowID, window->level, window->x, window->y,
                window->w, window->h, title != NULL ? title : "");
    }

    if (layerForWindowLevel(window->level, &layer)) {
        window->layerSurface = zwlr_layer_shell_v1_get_layer_surface(
            ws.layerShell, window->surface, NULL, layer,
            title != NULL && title[0] != '\0' ? title : "puredarwin");
        /* Without this a layer surface never gets keyboard focus, so a panel
         * with a text field resigns key the moment it is shown. */
        if (ws.layerShellVersion >= 4) {
            zwlr_layer_surface_v1_set_keyboard_interactivity(window->layerSurface,
                ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND);
        }
        zwlr_layer_surface_v1_add_listener(window->layerSurface,
                                           &layerSurfaceListener, window);
        zwlr_layer_surface_v1_set_size(window->layerSurface,
                                       (uint32_t)window->w, (uint32_t)window->h);
        windowApplyLayerGeometry(window, layer);
        return;
    }

    window->xdgSurface = xdg_wm_base_get_xdg_surface(ws.wmBase, window->surface);
    xdg_surface_add_listener(window->xdgSurface, &xdgSurfaceListener, window);
    window->toplevel = xdg_surface_get_toplevel(window->xdgSurface);

    if (title != NULL && title[0] != '\0') {
        xdg_toplevel_set_title(window->toplevel, title);
    }
}

static void windowTraceSurface(struct wsWindow *window, const char *what) {
    if (getenv("PD_WS_TRACE") == NULL) {
        return;
    }
    fprintf(stderr,
            "WindowServer: surface %d %s: configured=%d buffer=%p layer=%p "
            "toplevel=%p frame {{%g, %g}, {%g, %g}} screen %dx%d\n",
            window->windowID, what, window->configured,
            (void *)window->buffer, (void *)window->layerSurface,
            (void *)window->toplevel, window->x, window->y, window->w,
            window->h, ws.screenW, ws.screenH);
}

static void windowDestroyRole(struct wsWindow *window) {
    /* A replacement layer surface starts with no anchor or margins, so the
     * record of what was sent must not outlive the surface it described - or
     * the new one would be skipped as "unchanged" and never anchored. */
    window->haveSentAnchor = 0;

    if (window->layerSurface != NULL) {
        zwlr_layer_surface_v1_destroy(window->layerSurface);
        window->layerSurface = NULL;
    }
    if (window->toplevel != NULL) {
        xdg_toplevel_destroy(window->toplevel);
        window->toplevel = NULL;
    }
    if (window->xdgSurface != NULL) {
        xdg_surface_destroy(window->xdgSurface);
        window->xdgSurface = NULL;
    }
}

static kern_return_t windowCreate(struct wsRPCWindow *msg) {
    if (!ensureConnected()) {
        return KERN_FAILURE;
    }
    if (windowForID(msg->windowID) != NULL) {
        return KERN_SUCCESS;
    }

    struct wsWindow *window = calloc(1, sizeof(struct wsWindow));

    window->shmFd = -1;
    window->windowID = msg->windowID;
    window->x = msg->x;
    window->y = msg->y;
    window->w = msg->w;
    window->h = msg->h;

    window->level = msg->level;
    window->surface = wl_compositor_create_surface(ws.compositor);
    strncpy(window->title, msg->title, sizeof(window->title) - 1);

    window->next = ws.windows;
    ws.windows = window;

    windowCreateBuffer(window);

    return KERN_SUCCESS;
}

static kern_return_t windowDestroy(struct wsRPCWindow *msg) {
    struct wsWindow **link = &ws.windows;

    while (*link != NULL) {
        struct wsWindow *window = *link;

        if (window->windowID == msg->windowID) {
            *link = window->next;
            windowReleaseBuffer(window);
            windowDestroyRole(window);
            if (window->surface != NULL)    { wl_surface_destroy(window->surface); }
            free(window);
            if (ws.display != NULL) {
                wl_display_flush(ws.display);
            }
            return KERN_SUCCESS;
        }
        link = &window->next;
    }
    return KERN_SUCCESS;
}

static int windowEnsureRole(struct wsWindow *window) {
    if (window->layerSurface != NULL || window->toplevel != NULL) {
        return 1;
    }

    windowCreateRole(window, window->title);
    wl_surface_commit(window->surface);
    wl_display_flush(ws.display);

    pthread_mutex_lock(&ws.queueLock);
    while (!window->configured && ws.dispatchRunning) {
        pthread_cond_wait(&ws.queueReady, &ws.queueLock);
    }
    pthread_mutex_unlock(&ws.queueLock);

    return window->configured;
}

static kern_return_t windowModifyState(struct wsRPCWindow *msg) {
    struct wsWindow *window = windowForID(msg->windowID);

    /* AppKit sends the modify before the create for a window it has just
     * built, so treat an unknown id as a create. */
    if (window == NULL) {
        kern_return_t created = windowCreate(msg);

        if (created != KERN_SUCCESS) {
            return created;
        }
        window = windowForID(msg->windowID);
        if (window == NULL) {
            return KERN_FAILURE;
        }
    }

    if (msg->w > 0 && msg->h > 0 &&
        (msg->w != window->w || msg->h != window->h)) {
        window->w = msg->w;
        window->h = msg->h;

        windowReleaseBuffer(window);
        if (windowAttachBuffer(window)) {
            wl_surface_commit(window->surface);
            wl_display_flush(ws.display);
        }
    }

    if (msg->title[0] != '\0') {
        strncpy(window->title, msg->title, sizeof(window->title) - 1);
    }

    if (window->layerSurface == NULL && window->toplevel == NULL) {
        window->level = msg->level;
    }

    /* A level change can move the window between a layer and a toplevel, and
     * the role is fixed once assigned - rebuild it. */
    if (msg->level != window->level) {
        uint32_t wasLayer, nowLayer;
        int wasLayered = layerForWindowLevel(window->level, &wasLayer);
        int nowLayered = layerForWindowLevel(msg->level, &nowLayer);

        window->level = msg->level;

        if (wasLayered != nowLayered || (wasLayered && wasLayer != nowLayer)) {
            /* The client mapped this shm by path and caches the mapping, so
             * the buffer must survive the rebuild or it would go on drawing
             * into an orphaned object. It cannot stay attached though: giving
             * a wl_surface a new role while a buffer is attached is a protocol
             * error, so detach first and re-attach once reconfigured. */
            wl_surface_attach(window->surface, NULL, 0, 0);
            wl_surface_commit(window->surface);
            windowDestroyRole(window);
            window->configured = 0;
            windowCreateRole(window, msg->title);
            wl_surface_commit(window->surface);
            wl_display_flush(ws.display);

            pthread_mutex_lock(&ws.queueLock);
            while (!window->configured && ws.dispatchRunning) {
                pthread_cond_wait(&ws.queueReady, &ws.queueLock);
            }
            pthread_mutex_unlock(&ws.queueLock);
            return window->configured ? KERN_SUCCESS : KERN_FAILURE;
        }
    }

    if (msg->title[0] != '\0' && window->toplevel != NULL) {
        xdg_toplevel_set_title(window->toplevel, msg->title);
    }

    if (msg->state != window->state) {
        switch (msg->state) {
            case MINIMIZED:
                /* Deliberately not xdg_toplevel_set_minimized(): that request
                 * is a one-way hint. xdg-shell has no unset_minimized, so a
                 * client cannot un-minimize itself - only the compositor can,
                 * through a taskbar protocol - and a window minimized that way
                 * can never be restored. Unmapping looks the same and is
                 * reversible, which is what -deminiaturize: needs. */
                wl_surface_attach(window->surface, NULL, 0, 0);
                wl_surface_commit(window->surface);
                break;
            case MAXIMIZED:
                if (window->toplevel != NULL) { xdg_toplevel_set_maximized(window->toplevel); }
                break;
            case NORMAL:
                if (window->toplevel != NULL) { xdg_toplevel_unset_maximized(window->toplevel); }
                /* window->state is still the *previous* state here - it is
                 * assigned after this switch - so this is the test for "coming
                 * back from an unmapped state". Both MINIMIZED and HIDDEN
                 * detach the buffer, and without re-attaching it the window
                 * never reappears however often NORMAL is sent. */
                if (window->state == MINIMIZED || window->state == HIDDEN) {
                    if (windowAttachBuffer(window)) {
                        wl_surface_commit(window->surface);
                    }
                }
                break;
            case HIDDEN:
                /* Wayland has no hide; the surface is unmapped by attaching a
                 * null buffer. */
                wl_surface_attach(window->surface, NULL, 0, 0);
                wl_surface_commit(window->surface);
                break;
            default:
                break;
        }
        window->state = msg->state;
    }

    int moved = (msg->x != window->x || msg->y != window->y ||
                 msg->w != window->w || msg->h != window->h);

    window->x = msg->x;
    window->y = msg->y;
    window->w = msg->w;
    window->h = msg->h;

    if (moved && window->layerSurface != NULL) {
        uint32_t layer;
        if (layerForWindowLevel(window->level, &layer)) {
            windowApplyLayerGeometry(window, layer);
            wl_surface_commit(window->surface);
        }
    }

    wl_display_flush(ws.display);
    return KERN_SUCCESS;
}

kern_return_t _windowServerRPC(void *data, size_t len, void *reply, int *replyLen) {
    if (data == NULL || len < sizeof(struct wsRPCBase)) {
        return KERN_INVALID_ARGUMENT;
    }

    struct wsRPCBase *base = (struct wsRPCBase *)data;

    switch (base->code) {
        case kWSWindowCreate:
            return windowCreate((struct wsRPCWindow *)data);

        case kWSWindowDestroy:
            return windowDestroy((struct wsRPCWindow *)data);

        case kWSWindowModifyState:
            return windowModifyState((struct wsRPCWindow *)data);

        case kWSWindowFlush: {
            struct wsRPCWindow *msg = (struct wsRPCWindow *)data;
            struct wsWindow *window = windowForID(msg->windowID);

            if (window == NULL || !windowEnsureRole(window)) {
                return KERN_SUCCESS;
            }
            windowTraceSurface(window, "flush");
            if (window->buffer == NULL) {
                return KERN_SUCCESS;
            }

            if (getenv("PD_WS_TRACE") != NULL && window->pixels != NULL &&
                window->level >= kCGMainMenuWindowLevelKey) {
                const uint32_t *px = window->pixels;
                size_t count = window->pixelsSize / 4;
                size_t nonzero = 0;
                size_t i;

                for (i = 0; i < count; i++) {
                    if (px[i] != 0) {
                        nonzero++;
                    }
                }
                fprintf(stderr,
                        "WindowServer: surface %d pixels: %zu/%zu non-zero, "
                        "first=0x%08x mid=0x%08x\n",
                        window->windowID, nonzero, count, px[0],
                        px[count / 2]);
            }
            /* The client has drawn into the shared pixels; tell the compositor
             * the whole surface changed and hand the frame over. */
            wl_surface_attach(window->surface, window->buffer, 0, 0);
            wl_surface_damage(window->surface, 0, 0, (int)window->w, (int)window->h);
            wl_surface_commit(window->surface);
            wl_display_flush(ws.display);
            return KERN_SUCCESS;
        }

        case kWSWindowMove: {
            struct wsRPCSimple *msg = (struct wsRPCSimple *)data;
            struct wsWindow *window = windowForID(msg->val1);

            if (window == NULL || window->toplevel == NULL || ws.seat == NULL ||
                ws.pointerButtonSerial == 0) {
                return KERN_FAILURE;
            }
            xdg_toplevel_move(window->toplevel, ws.seat,
                              ws.pointerButtonSerial);
            wl_display_flush(ws.display);
            return KERN_SUCCESS;
        }

        case kWSWindowResize: {
            struct wsRPCSimple *msg = (struct wsRPCSimple *)data;
            struct wsWindow *window = windowForID(msg->val1);

            if (window == NULL || window->toplevel == NULL || ws.seat == NULL ||
                ws.pointerButtonSerial == 0) {
                return KERN_FAILURE;
            }
            xdg_toplevel_resize(window->toplevel, ws.seat,
                                ws.pointerButtonSerial, msg->val2);
            wl_display_flush(ws.display);
            return KERN_SUCCESS;
        }

        case kCGGetLastMouseDelta:
        case kWSGetWindowInfo: {
            if (reply == NULL || replyLen == NULL ||
                (size_t)*replyLen < sizeof(struct wsRPCWindow)) {
                return KERN_INVALID_ARGUMENT;
            }
            if (len < sizeof(struct wsRPCWindow)) {
                return KERN_INVALID_ARGUMENT;
            }
            struct wsRPCWindow *in = (struct wsRPCWindow *)data;
            struct wsRPCWindow *out = (struct wsRPCWindow *)reply;
            int wanted = in->windowID != 0 ? in->windowID : ws.activeAppWindowID;
            struct wsWindow *window = wanted != 0 ? windowForID(wanted) : NULL;

            memset(out, 0, sizeof(*out));
            out->base.code = kWSGetWindowInfo;
            out->base.len = sizeof(*out) - sizeof(struct wsRPCBase);
            if (window != NULL) {
                out->windowID = window->windowID;
                out->x = window->x;
                out->y = window->y;
                out->w = window->w;
                out->h = window->h;
                out->state = window->state;
                out->level = window->level;
                strncpy(out->title, window->title, sizeof(out->title) - 1);
            }
            *replyLen = sizeof(struct wsRPCWindow);
            return KERN_SUCCESS;
        }

        case kCGGetMouseLocation: {
            if (reply == NULL || replyLen == NULL ||
                (size_t)*replyLen < sizeof(struct wsRPCSimple)) {
                return KERN_INVALID_ARGUMENT;
            }
            struct wsRPCSimple *out = (struct wsRPCSimple *)reply;

            out->val1 = ws.lastMouseX;
            out->val2 = ws.lastMouseY;
            *replyLen = sizeof(struct wsRPCSimple);
            return KERN_SUCCESS;
        }

        default:
            fprintf(stderr, "WindowServer: unhandled RPC code %u\n", base->code);
            return KERN_INVALID_ARGUMENT;
    }
}
