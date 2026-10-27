/*
 * rpc_wayland.c - the window-server RPCs, served over Wayland.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <WindowServer/rpc.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "ext-background-effect-v1-client-protocol.h"
#include <math.h>
#include "linux-dmabuf-v1-client-protocol.h"
#include "wlr-output-management-unstable-v1-client-protocol.h"
#include "pd-window-position-v1-client-protocol.h"
#include "pd-surface-host-v1-client-protocol.h"
#include <CoreGraphics/CGDirectDisplay_puredarwin.h>
#include <CoreGraphics/CGWindowLevel.h>
#include <CoreFoundation/CFBundle.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <pthread.h>
#include <poll.h>
#include <time.h>
#include <stdatomic.h>
#include <mach/mach_time.h>
#include <xkbcommon/xkbcommon.h>

// a finished frame the compositor reads while AppKit draws the next one into the backing store
struct wsFrame {
    struct wl_buffer *buffer;
    void *pixels;
    size_t size;
    int w, h;
    int busy;
};

#define WS_FRAMES 3

struct wsWindow {
    int windowID;
    struct wl_surface *surface;
    struct xdg_surface *xdgSurface;
    struct xdg_toplevel *toplevel;
    // where the compositor placed this window, NULL without pd_window_position
    struct pd_window_position_v1 *position;
    struct zwlr_layer_surface_v1 *layerSurface;
    int level;
    // the backing store AppKit maps by path and draws into, never attached to the surface itself
    void *pixels;
    size_t pixelsSize;
    int pixelsW, pixelsH;
    int shmFd;
    struct wsFrame frames[WS_FRAMES];
    struct wsFrame *front;
    double x, y, w, h;
    int state;
    int configured;
    char title[64];
    /* Last anchor/margins actually sent, so an unchanged layout does not
     * re-issue the requests. */
    uint32_t sentAnchor;
    int32_t sentMarginTop, sentMarginRight, sentMarginBottom, sentMarginLeft;
    int haveSentAnchor;
    // the compositor blurs what is behind the window inside its edges minus blurInset, rounded by blurRadius
    struct ext_background_effect_surface_v1 *blurEffect;
    int blurOn, blurInset, blurRadius, blurDirty;
    int ignoresMouse, inputDirty;
    struct wsWindow *next;
};

static struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wmBase;
    struct zwlr_layer_shell_v1 *layerShell;
    uint32_t layerShellVersion;
    // NULL when the compositor cannot blur, windows that ask for it then just stay translucent
    struct ext_background_effect_manager_v1 *blurManager;
    struct wl_seat *seat;
    struct wl_output *output;
    struct wl_shm *shm;
    // metal's CAMetalLayer presents into subsurfaces of AppKit windows.
    struct wl_subcompositor *subcompositor;
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
    // the surface under the pointer, kept apart from keyboard focus so a new window cannot take pointer events
    int pointerWindowID;
    /* Panels, the dock and the desktop take pointer focus too, so the window
     * reported as active is only ever an xdg toplevel. */
    int activeAppWindowID;
    uint32_t pointerButtonSerial;
    // the newest key, button or focus serial, what wl_data_device.set_selection must quote
    uint32_t inputSerial;
    struct wl_data_device_manager *dataDeviceManager;
    struct wl_data_device *dataDevice;
    struct wsWindow *windows;
    int connected;
    int lastMouseX, lastMouseY;
    int screenW, screenH;
    // modes the compositor lists through wlr-output-management, read from any thread under modesLock
    struct zwlr_output_manager_v1 *outputManager;
    struct pd_window_position_manager_v1 *positionManager;
    struct pd_surface_host_manager_v1 *surfaceHost;
    struct zwlr_output_head_v1 *head;
    uint32_t outputSerial;
    pthread_mutex_t modesLock;
    // window->pixels is swapped on a configure from the dispatch thread while the main thread copies out of it
    pthread_mutex_t pixelsLock;
    struct wsMode *modes;
    struct wsMode *currentMode;
    int32_t nextModeID;
} ws = { .modesLock = PTHREAD_MUTEX_INITIALIZER };

struct wsMode {
    struct zwlr_output_mode_v1 *proxy;
    int32_t width, height, refresh;
    int32_t modeID;
    struct wsMode *next;
};

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

static void relayoutLayerWindows(void);

static void outputMode(void *data, struct wl_output *output, uint32_t flags,
                       int32_t width, int32_t height, int32_t refresh) {
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        int resized = ws.screenW > 0 && (ws.screenW != width || ws.screenH != height);

        /* CoreGraphics cannot discover displays itself; hand it the real
         * geometry now that the compositor has told us. */
        ws.screenW = width;
        ws.screenH = height;
        // panels are anchored to the nearest edges, so margins from the old size would move them
        if (resized)
            relayoutLayerWindows();
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

static struct wsMode *modeForProxy(struct zwlr_output_mode_v1 *proxy) {
    for (struct wsMode *m = ws.modes; m != NULL; m = m->next) {
        if (m->proxy == proxy)
            return m;
    }
    return NULL;
}

static void modeSize(void *data, struct zwlr_output_mode_v1 *proxy, int32_t width, int32_t height) {
    struct wsMode *mode = data;

    pthread_mutex_lock(&ws.modesLock);
    mode->width = width;
    mode->height = height;
    pthread_mutex_unlock(&ws.modesLock);
}

static void modeRefresh(void *data, struct zwlr_output_mode_v1 *proxy, int32_t refresh) {
    struct wsMode *mode = data;

    mode->refresh = refresh;
}

static void modePreferred(void *data, struct zwlr_output_mode_v1 *proxy) {
}

static void modeFinished(void *data, struct zwlr_output_mode_v1 *proxy) {
    struct wsMode *mode = data;

    pthread_mutex_lock(&ws.modesLock);
    for (struct wsMode **link = &ws.modes; *link != NULL; link = &(*link)->next) {
        if (*link != mode)
            continue;
        *link = mode->next;
        break;
    }
    if (ws.currentMode == mode)
        ws.currentMode = NULL;
    pthread_mutex_unlock(&ws.modesLock);
    zwlr_output_mode_v1_destroy(proxy);
    free(mode);
}

static const struct zwlr_output_mode_v1_listener modeListener = {
    .size = modeSize,
    .refresh = modeRefresh,
    .preferred = modePreferred,
    .finished = modeFinished,
};

static void headName(void *data, struct zwlr_output_head_v1 *head, const char *name) {
}

static void headDescription(void *data, struct zwlr_output_head_v1 *head, const char *description) {
}

static void headPhysicalSize(void *data, struct zwlr_output_head_v1 *head, int32_t width, int32_t height) {
}

static void headMode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *proxy) {
    struct wsMode *mode = calloc(1, sizeof(struct wsMode));
    struct wsMode **tail;

    mode->proxy = proxy;
    zwlr_output_mode_v1_add_listener(proxy, &modeListener, mode);
    pthread_mutex_lock(&ws.modesLock);
    mode->modeID = ++ws.nextModeID;
    // kept in the compositor's order, which is how a mode picker lists them
    for (tail = &ws.modes; *tail != NULL; tail = &(*tail)->next) {
    }
    *tail = mode;
    pthread_mutex_unlock(&ws.modesLock);
}

static void headEnabled(void *data, struct zwlr_output_head_v1 *head, int32_t enabled) {
}

static void headCurrentMode(void *data, struct zwlr_output_head_v1 *head, struct zwlr_output_mode_v1 *proxy) {
    pthread_mutex_lock(&ws.modesLock);
    ws.currentMode = modeForProxy(proxy);
    pthread_mutex_unlock(&ws.modesLock);
}

static void headPosition(void *data, struct zwlr_output_head_v1 *head, int32_t x, int32_t y) {
}

static void headTransform(void *data, struct zwlr_output_head_v1 *head, int32_t transform) {
}

static void headScale(void *data, struct zwlr_output_head_v1 *head, wl_fixed_t scale) {
}

static void headFinished(void *data, struct zwlr_output_head_v1 *head) {
    if (ws.head == head)
        ws.head = NULL;
    zwlr_output_head_v1_destroy(head);
}

static void headString(void *data, struct zwlr_output_head_v1 *head, const char *value) {
}

static const struct zwlr_output_head_v1_listener headListener = {
    .name = headName,
    .description = headDescription,
    .physical_size = headPhysicalSize,
    .mode = headMode,
    .enabled = headEnabled,
    .current_mode = headCurrentMode,
    .position = headPosition,
    .transform = headTransform,
    .scale = headScale,
    .finished = headFinished,
    .make = headString,
    .model = headString,
    .serial_number = headString,
};

static void managerHead(void *data, struct zwlr_output_manager_v1 *manager, struct zwlr_output_head_v1 *head) {
    // one display today, the first head is the main one
    if (ws.head == NULL)
        ws.head = head;
    zwlr_output_head_v1_add_listener(head, &headListener, NULL);
}

static void managerDone(void *data, struct zwlr_output_manager_v1 *manager, uint32_t serial) {
    ws.outputSerial = serial;
}

static void managerFinished(void *data, struct zwlr_output_manager_v1 *manager) {
    ws.outputManager = NULL;
    zwlr_output_manager_v1_destroy(manager);
}

static const struct zwlr_output_manager_v1_listener managerListener = {
    .head = managerHead,
    .done = managerDone,
    .finished = managerFinished,
};

static void configurationSucceeded(void *data, struct zwlr_output_configuration_v1 *config) {
    zwlr_output_configuration_v1_destroy(config);
}

static void configurationFailed(void *data, struct zwlr_output_configuration_v1 *config) {
    fprintf(stderr, "WindowServer: compositor refused the display mode\n");
    zwlr_output_configuration_v1_destroy(config);
}

static const struct zwlr_output_configuration_v1_listener configurationListener = {
    .succeeded = configurationSucceeded,
    .failed = configurationFailed,
    .cancelled = configurationFailed,
};

static int copyDisplayModes(struct CGDirectDisplayModeInfo *modes, int max) {
    int count = 0;

    pthread_mutex_lock(&ws.modesLock);
    for (struct wsMode *m = ws.modes; m != NULL && count < max; m = m->next) {
        modes[count].width = m->width;
        modes[count].height = m->height;
        modes[count].refresh = m->refresh / 1000.0;
        modes[count].modeID = m->modeID;
        modes[count].current = (m == ws.currentMode);
        count++;
    }
    pthread_mutex_unlock(&ws.modesLock);
    return count;
}

// the switch is asynchronous, the new size arrives as a wl_output mode event
static bool setDisplayMode(int32_t modeID) {
    struct zwlr_output_mode_v1 *proxy = NULL;

    pthread_mutex_lock(&ws.modesLock);
    for (struct wsMode *m = ws.modes; m != NULL; m = m->next) {
        if (m->modeID == modeID)
            proxy = m->proxy;
    }
    pthread_mutex_unlock(&ws.modesLock);
    if (proxy == NULL || ws.outputManager == NULL || ws.head == NULL)
        return false;

    struct zwlr_output_configuration_v1 *config =
        zwlr_output_manager_v1_create_configuration(ws.outputManager, ws.outputSerial);
    struct zwlr_output_configuration_head_v1 *configHead =
        zwlr_output_configuration_v1_enable_head(config, ws.head);

    zwlr_output_configuration_v1_add_listener(config, &configurationListener, NULL);
    zwlr_output_configuration_head_v1_set_mode(configHead, proxy);
    zwlr_output_configuration_v1_apply(config);
    wl_display_flush(ws.display);
    return true;
}

static const struct CGDirectDisplayModeProvider modeProvider = {
    .copyModes = copyDisplayModes,
    .setMode = setDisplayMode,
};

/* Defined with the rest of the input handling, below. */
static const struct wl_seat_listener seatListener;
static void *dispatchThreadMain(void *unused);
static int windowCreateBuffer(struct wsWindow *window);
static int windowShowFront(struct wsWindow *window);
static int windowPresent(struct wsWindow *window);
static void windowReleaseBuffer(struct wsWindow *window);
static void windowReleaseFrames(struct wsWindow *window);

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
    } else if (strcmp(interface, ext_background_effect_manager_v1_interface.name) == 0) {
        ws.blurManager = wl_registry_bind(registry, name, &ext_background_effect_manager_v1_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        ws.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, wl_subcompositor_interface.name) == 0) {
        ws.subcompositor = wl_registry_bind(registry, name, &wl_subcompositor_interface, 1);
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
    } else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
        ws.dataDeviceManager = wl_registry_bind(registry, name, &wl_data_device_manager_interface, version < 3 ? version : 3);
    } else if (strcmp(interface, pd_window_position_manager_v1_interface.name) == 0) {
        ws.positionManager = wl_registry_bind(registry, name, &pd_window_position_manager_v1_interface, 1);
    } else if (strcmp(interface, pd_surface_host_manager_v1_interface.name) == 0) {
        ws.surfaceHost = wl_registry_bind(registry, name, &pd_surface_host_manager_v1_interface, 1);
    } else if (strcmp(interface, zwlr_output_manager_v1_interface.name) == 0) {
        ws.outputManager = wl_registry_bind(registry, name, &zwlr_output_manager_v1_interface,
                                            version < 2 ? version : 2);
        zwlr_output_manager_v1_add_listener(ws.outputManager, &managerListener, NULL);
        CGDirectDisplaySetModeProvider(&modeProvider);
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

static void dataDeviceStart(void);

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
    {
        pthread_mutexattr_t recursive;

        pthread_mutexattr_init(&recursive);
        pthread_mutexattr_settype(&recursive, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&ws.pixelsLock, &recursive);
        pthread_mutexattr_destroy(&recursive);
    }
    pthread_cond_init(&ws.queueReady, NULL);
    if (getenv("PD_WS_TRACE") != NULL) {
        fprintf(stderr, "WindowServer: linux-dmabuf %s\n",
                (ws.dmabuf != NULL) ? "available (GPU buffers possible)"
                                    : "absent (shm path)");
    }

    if (ws.dataDeviceManager != NULL && ws.seat != NULL)
        dataDeviceStart();

    ws.connected = 1;
    ws.dispatchRunning = 1;
    pthread_create(&ws.dispatchThread, NULL, dispatchThreadMain, NULL);
    return 1;
}

static void xdgSurfaceConfigure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct wsWindow *window = data;

    xdg_surface_ack_configure(surface, serial);
    // a rebuilt role starts unmapped: put the last finished frame back, or wait for the first flush
    windowShowFront(window);
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
    pthread_mutex_lock(&ws.pixelsLock);
    if (width > 0 && height > 0 &&
        ((double)width != window->w || (double)height != window->h)) {
        window->w = width;
        window->h = height;
        windowReleaseBuffer(window);
        windowCreateBuffer(window);
    }
    // a new layer surface is unmapped until a buffer goes on it, one AppKit ordered out stays unmapped
    if (window->state != MINIMIZED && window->state != HIDDEN && !windowShowFront(window))
        windowPresent(window);
    pthread_mutex_unlock(&ws.pixelsLock);
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

    // a full-width bar on the top or bottom edge reserves its height, so windows stay clear of it
    int32_t zone = 0;

    if (leftGap <= slack && rightGap <= slack) {
        anchor |= ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
        if (anchor & (ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM))
            zone = (int32_t)window->h;
    }
    // the dock is narrower than the screen but still keeps windows above its top edge
    if (window->level >= kCGDockWindowLevelKey && window->level < kCGMainMenuWindowLevelKey &&
        anchor == ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)
        zone = (int32_t)(window->h + bottomGap);

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
    zwlr_layer_surface_v1_set_exclusive_zone(window->layerSurface, zone);
}

static void relayoutLayerWindows(void) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        uint32_t layer;

        // a commit would map a panel AppKit ordered out again
        if (w->layerSurface == NULL || w->front == NULL || w->state == MINIMIZED || w->state == HIDDEN ||
            !layerForWindowLevel(w->level, &layer))
            continue;
        windowApplyLayerGeometry(w, layer);
        wl_surface_commit(w->surface);
    }
    wl_display_flush(ws.display);
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

    size_t size = (size_t)width * 4 * (size_t)height;
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
    window->pixelsW = width;
    window->pixelsH = height;
    return 1;
}

static void windowReleaseBuffer(struct wsWindow *window) {
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

static void frameReleased(void *data, struct wl_buffer *buffer) {
    struct wsFrame *frame = data;

    pthread_mutex_lock(&ws.queueLock);
    frame->busy = 0;
    pthread_cond_broadcast(&ws.queueReady);
    pthread_mutex_unlock(&ws.queueLock);
}

static const struct wl_buffer_listener frameListener = {
    .release = frameReleased,
};

static void frameDestroy(struct wsFrame *frame) {
    if (frame->buffer != NULL)
        wl_buffer_destroy(frame->buffer);
    if (frame->pixels != NULL)
        munmap(frame->pixels, frame->size);
    memset(frame, 0, sizeof(*frame));
}

// anonymous shared memory: the name is gone before the compositor sees the fd
static int frameCreate(struct wsWindow *window, struct wsFrame *frame, int w, int h) {
    static unsigned serial;
    size_t size = (size_t)w * 4 * (size_t)h;
    char name[32];
    int fd;

    snprintf(name, sizeof(name), "/wsf.%u.%u", (unsigned)getpid(), ++serial);
    fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0)
        return 0;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) != 0) {
        close(fd);
        return 0;
    }
    frame->pixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (frame->pixels == MAP_FAILED) {
        frame->pixels = NULL;
        close(fd);
        return 0;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(ws.shm, fd, (int32_t)size);
    frame->buffer = wl_shm_pool_create_buffer(pool, 0, w, h, w * 4, WL_SHM_FORMAT_ARGB8888);
    wl_shm_pool_destroy(pool);
    close(fd);
    wl_buffer_add_listener(frame->buffer, &frameListener, frame);
    frame->size = size;
    frame->w = w;
    frame->h = h;
    return 1;
}

// a frame the compositor has let go of, the one on screen and the one before it (until its release
// arrives) are busy, so a third covers the round trip, and past that this waits briefly
static struct wsFrame *windowFreeFrame(struct wsWindow *window, int w, int h) {
    struct wsFrame *pick = NULL;
    struct timespec deadline;

    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_nsec += 100 * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&ws.queueLock);
    for (;;) {
        for (int i = 0; i < WS_FRAMES && pick == NULL; i++) {
            struct wsFrame *frame = &window->frames[i];
            if (!frame->busy && frame->buffer != NULL && frame->w == w && frame->h == h)
                pick = frame;
        }
        for (int i = 0; i < WS_FRAMES && pick == NULL; i++) {
            if (!window->frames[i].busy)
                pick = &window->frames[i];
        }
        if (pick != NULL || !ws.dispatchRunning)
            break;
        if (pthread_cond_timedwait(&ws.queueReady, &ws.queueLock, &deadline) != 0)
            break;
    }
    if (pick != NULL)
        pick->busy = 1;
    pthread_mutex_unlock(&ws.queueLock);

    if (pick == NULL) {
        if (getenv("PD_WS_TRACE") != NULL)
            fprintf(stderr, "WindowServer: surface %d: no frame released in time\n", window->windowID);
        return NULL;
    }
    if (pick->buffer == NULL || pick->w != w || pick->h != h) {
        frameDestroy(pick);
        if (!frameCreate(window, pick, w, h))
            return NULL;
        pick->busy = 1;
    }
    return pick;
}

// copies what AppKit has finished drawing into a free frame and puts it on the surface, the caller commits
static int windowPresent(struct wsWindow *window) {
    pthread_mutex_lock(&ws.pixelsLock);
    if (window->pixels == NULL || window->pixelsW <= 0 || window->pixelsH <= 0) {
        pthread_mutex_unlock(&ws.pixelsLock);
        return 0;
    }

    struct wsFrame *frame = windowFreeFrame(window, window->pixelsW, window->pixelsH);
    if (frame == NULL) {
        pthread_mutex_unlock(&ws.pixelsLock);
        return 0;
    }

    memcpy(frame->pixels, window->pixels, frame->size);
    pthread_mutex_unlock(&ws.pixelsLock);
    window->front = frame;
    wl_surface_attach(window->surface, frame->buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, frame->w, frame->h);
    return 1;
}

// puts the last presented frame back on the surface, unless the window is meant to be unmapped
static int windowShowFront(struct wsWindow *window) {
    if (window->front == NULL || window->state == MINIMIZED || window->state == HIDDEN)
        return 0;

    pthread_mutex_lock(&ws.queueLock);
    window->front->busy = 1;
    pthread_mutex_unlock(&ws.queueLock);
    wl_surface_attach(window->surface, window->front->buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, window->front->w, window->front->h);
    return 1;
}

static void windowReleaseFrames(struct wsWindow *window) {
    for (int i = 0; i < WS_FRAMES; i++)
        frameDestroy(&window->frames[i]);
    window->front = NULL;
}

/* The NSEventType values NSApplication's translation switches on (Cocoa's numbers). Spelled out
 * here because AppKit's headers are Objective-C and this file is not. */
enum {
    WS_LEFT_MOUSE_DOWN  = 1,
    WS_LEFT_MOUSE_UP    = 2,
    WS_RIGHT_MOUSE_DOWN = 3,
    WS_RIGHT_MOUSE_UP   = 4,
    WS_MOUSE_MOVED      = 5,
    WS_LEFT_MOUSE_DRAGGED  = 6,
    WS_RIGHT_MOUSE_DRAGGED = 7,
    WS_MOUSE_EXITED     = 9,
    WS_KEY_DOWN         = 10,
    WS_KEY_UP           = 11,
    WS_SCROLL_WHEEL     = 22,
    WS_OTHER_MOUSE_DOWN = 25,
    WS_OTHER_MOUSE_UP   = 26,
    WS_OTHER_MOUSE_DRAGGED = 27,
};

// when this process last saw user input, in nanoseconds of mach_absolute_time, published for idle time readers
static _Atomic uint64_t lastInputTime;
static int lastInputToken = -1;
static uint64_t lastInputPublished;

// libnotify ships in libSystem without its header in this SDK
extern uint32_t notify_register_check(const char *name, int *out_token);
extern uint32_t notify_set_state(int token, uint64_t state);
extern uint32_t notify_post(const char *name);

#define kLastInputNotifyName "org.puredarwin.hid.lastinput"

static void noteInput(void) {
    static mach_timebase_info_data_t timebase;
    uint64_t now;

    if (timebase.denom == 0)
        mach_timebase_info(&timebase);
    now = mach_absolute_time() * timebase.numer / timebase.denom;

    lastInputTime = now;
    // at most once a second, the reader only needs whole seconds
    if (now - lastInputPublished < 1000000000ULL)
        return;
    lastInputPublished = now;
    if (lastInputToken < 0 && notify_register_check(kLastInputNotifyName, &lastInputToken) != 0)
        return;
    notify_set_state(lastInputToken, now);
}

uint64_t _windowServerLastInputTime(void) {
    return lastInputTime;
}

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
// held buttons, so motion can be reported as a drag the way AppKit expects
static int leftButtonDown, rightButtonDown, otherButtonDown;

// the pointer image over our surfaces: a named shape from the cursor theme, or an image AppKit drew
static struct {
    pthread_mutex_t lock;
    struct wl_cursor_theme *theme;
    struct wl_surface *surface;
    struct wl_buffer *buffer;
    int hotX, hotY;
    int hidden;
    uint32_t enterSerial;
    int inside;
    // an image cursor's own buffer, which the theme does not own
    struct wl_buffer *imageBuffer;
    void *imagePixels;
    size_t imageSize;
} cursor = { .lock = PTHREAD_MUTEX_INITIALIZER };

// xcursor names for each NSCursor, in order of preference, the last ones are in libwayland-cursor's built-in set
static const char *const cursorNames[][5] = {
    { "arrowCursor", "left_ptr", "default", NULL },
    { "IBeamCursor", "xterm", "text", NULL },
    { "pointingHandCursor", "hand2", "pointer", "hand1", NULL },
    { "openHandCursor", "openhand", "grab", "hand1", NULL },
    { "closedHandCursor", "closedhand", "grabbing", NULL },
    { "crosshairCursor", "crosshair", "cross", "left_ptr", NULL },
    { "resizeLeftRightCursor", "sb_h_double_arrow", "ew-resize", "right_side", NULL },
    { "resizeLeftCursor", "left_side", "w-resize", NULL },
    { "resizeRightCursor", "right_side", "e-resize", NULL },
    { "resizeUpDownCursor", "sb_v_double_arrow", "ns-resize", "bottom_side", NULL },
    { "resizeUpCursor", "top_side", "n-resize", NULL },
    { "resizeDownCursor", "bottom_side", "s-resize", NULL },
    { "operationNotAllowedCursor", "not-allowed", "crossed_circle", "left_ptr", NULL },
    { "dragCopyCursor", "copy", "dnd-copy", "left_ptr", NULL },
    { "dragLinkCursor", "alias", "dnd-link", "left_ptr", NULL },
    { "contextualMenuCursor", "context-menu", "left_ptr", NULL },
    { "IBeamCursorForVerticalLayout", "vertical-text", "xterm", NULL },
    { "busyButClickableCursor", "progress", "left_ptr_watch", "left_ptr", NULL },
    { "columnResizeCursor", "col-resize", "sb_h_double_arrow", NULL },
    { "rowResizeCursor", "row-resize", "sb_v_double_arrow", NULL },
    { "zoomInCursor", "zoom-in", "left_ptr", NULL },
    { "zoomOutCursor", "zoom-out", "left_ptr", NULL },
    { "frameResizeTopLeftCursor", "nw-resize", "top_left_corner", NULL },
    { "frameResizeTopRightCursor", "ne-resize", "top_right_corner", NULL },
    { "frameResizeBottomLeftCursor", "sw-resize", "bottom_left_corner", NULL },
    { "frameResizeBottomRightCursor", "se-resize", "bottom_right_corner", NULL },
};

// with the lock held: show the current image if the pointer is over one of our surfaces
static void cursorApplyLocked(void) {
    if (ws.pointer == NULL || !cursor.inside)
        return;
    if (cursor.hidden || cursor.buffer == NULL) {
        wl_pointer_set_cursor(ws.pointer, cursor.enterSerial, NULL, 0, 0);
        return;
    }
    if (cursor.surface == NULL)
        cursor.surface = wl_compositor_create_surface(ws.compositor);
    wl_surface_attach(cursor.surface, cursor.buffer, 0, 0);
    wl_surface_damage(cursor.surface, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_commit(cursor.surface);
    wl_pointer_set_cursor(ws.pointer, cursor.enterSerial, cursor.surface, cursor.hotX, cursor.hotY);
}

static struct wl_cursor *cursorLookup(const char *name) {
    if (cursor.theme == NULL) {
        const char *size = getenv("XCURSOR_SIZE");
        cursor.theme = wl_cursor_theme_load(getenv("XCURSOR_THEME"), (size != NULL && atoi(size) > 0) ? atoi(size) : 24, ws.shm);
        if (cursor.theme == NULL)
            return NULL;
    }
    for (size_t i = 0; i < sizeof(cursorNames) / sizeof(cursorNames[0]); i++) {
        if (strcmp(cursorNames[i][0], name) != 0)
            continue;
        for (int j = 1; j < 5 && cursorNames[i][j] != NULL; j++) {
            struct wl_cursor *found = wl_cursor_theme_get_cursor(cursor.theme, cursorNames[i][j]);
            if (found != NULL)
                return found;
        }
        return NULL;
    }
    // anything else is taken as an xcursor name, which is how AppKit asks for the window-edge shapes
    return wl_cursor_theme_get_cursor(cursor.theme, name);
}

// animated shapes show their first image
static int cursorUseLocked(struct wl_cursor *shape) {
    if (shape == NULL || shape->image_count == 0)
        return 0;

    struct wl_cursor_image *image = shape->images[0];
    cursor.buffer = wl_cursor_image_get_buffer(image);
    cursor.hotX = (int)image->hotspot_x;
    cursor.hotY = (int)image->hotspot_y;
    return 1;
}

void _windowServerSetCursorNamed(const char *name) {
    struct wl_cursor *shape;
    struct wl_buffer *was;

    if (!ensureConnected())
        return;
    pthread_mutex_lock(&cursor.lock);
    shape = cursorLookup(name != NULL ? name : "arrowCursor");
    if (shape == NULL && name != NULL)
        shape = cursorLookup("arrowCursor");
    // AppKit sets the cursor on every move, only a change goes to the compositor
    was = cursor.buffer;
    if (cursorUseLocked(shape) && cursor.buffer != was) {
        cursorApplyLocked();
        wl_display_flush(ws.display);
    }
    pthread_mutex_unlock(&cursor.lock);
}

// argb is premultiplied ARGB8888 in host order, the layout Wayland's shm format names
void _windowServerSetCursorImage(const void *argb, int width, int height, int hotX, int hotY) {
    size_t size = (size_t)width * 4 * (size_t)height;
    char name[32];
    int fd;

    if (!ensureConnected() || argb == NULL || width <= 0 || height <= 0)
        return;
    pthread_mutex_lock(&cursor.lock);
    if (cursor.imageBuffer != NULL) {
        if (cursor.buffer == cursor.imageBuffer)
            cursor.buffer = NULL;
        wl_buffer_destroy(cursor.imageBuffer);
        munmap(cursor.imagePixels, cursor.imageSize);
        cursor.imageBuffer = NULL;
    }

    snprintf(name, sizeof(name), "/wsc.%u", (unsigned)getpid());
    fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd >= 0) {
        shm_unlink(name);
        if (ftruncate(fd, (off_t)size) == 0) {
            cursor.imagePixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (cursor.imagePixels != MAP_FAILED) {
                struct wl_shm_pool *pool = wl_shm_create_pool(ws.shm, fd, (int32_t)size);

                memcpy(cursor.imagePixels, argb, size);
                cursor.imageBuffer = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4,
                                                               WL_SHM_FORMAT_ARGB8888);
                cursor.imageSize = size;
                wl_shm_pool_destroy(pool);
            }
        }
        close(fd);
    }
    if (cursor.imageBuffer != NULL) {
        cursor.buffer = cursor.imageBuffer;
        cursor.hotX = hotX;
        cursor.hotY = hotY;
        cursorApplyLocked();
        wl_display_flush(ws.display);
    }
    pthread_mutex_unlock(&cursor.lock);
}

void _windowServerSetCursorHidden(int hidden) {
    if (!ensureConnected())
        return;
    pthread_mutex_lock(&cursor.lock);
    cursor.hidden = hidden;
    cursorApplyLocked();
    wl_display_flush(ws.display);
    pthread_mutex_unlock(&cursor.lock);
}

static void pointerEnter(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y) {
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface) {
            ws.pointerWindowID = w->windowID;
            if (w->toplevel != NULL) {
                ws.activeAppWindowID = w->windowID;
            }
            break;
        }
    }
    lastPointerX = wl_fixed_to_double(x);
    lastPointerY = wl_fixed_to_double(y);

    // a surface entered with no cursor set shows none, so the current shape goes on each time
    pthread_mutex_lock(&cursor.lock);
    cursor.enterSerial = serial;
    cursor.inside = 1;
    if (cursor.buffer == NULL)
        cursorUseLocked(cursorLookup("arrowCursor"));
    cursorApplyLocked();
    pthread_mutex_unlock(&cursor.lock);
}

static void pointerLeave(void *data, struct wl_pointer *pointer, uint32_t serial,
                         struct wl_surface *surface) {
    struct mach_event event = {0};

    // the last position inside the surface, so AppKit's hit test still finds the view being left
    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface) {
            event.windowID = w->windowID;
            event.code = WS_MOUSE_EXITED;
            /* NSEvent.locationInWindow is window-local. Keep input local all
             * the way to AppKit; compositor placement can change before its
             * asynchronous move notification updates NSWindow's frame. */
            event.x = lastPointerX;
            event.y = w->h - lastPointerY;
            event.mods = currentModifiers;
            queuePush(&event);
            break;
        }
    }
    pthread_mutex_lock(&cursor.lock);
    cursor.inside = 0;
    pthread_mutex_unlock(&cursor.lock);
}

static void pointerMotion(void *data, struct wl_pointer *pointer, uint32_t time,
                          wl_fixed_t x, wl_fixed_t y) {
    noteInput();
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(ws.pointerWindowID);
    double newX = wl_fixed_to_double(x);
    double newY = wl_fixed_to_double(y);

    event.windowID = ws.pointerWindowID;
    event.code = leftButtonDown ? WS_LEFT_MOUSE_DRAGGED
               : rightButtonDown ? WS_RIGHT_MOUSE_DRAGGED
               : otherButtonDown ? WS_OTHER_MOUSE_DRAGGED : WS_MOUSE_MOVED;
    /* AppKit's locationInWindow is surface-local. Do not add the screen
     * origin here: the compositor's move callback and NSWindow's frame update
     * are asynchronous, so subtracting that frame origin can turn a valid
     * click into a point outside the view. */
    event.x = newX;
    event.y = window != NULL ? window->h - newY : newY;
    event.dx = newX - lastPointerX;
    event.dy = lastPointerY - newY;
    event.mods = currentModifiers;
    lastPointerX = newX;
    lastPointerY = newY;
    // where kCGGetMouseLocation finds the pointer, in the same screen space as the events
    ws.lastMouseX = (int)(newX + (window != NULL ? window->x : 0));
    ws.lastMouseY = (int)(window != NULL ? window->y + window->h - newY : newY);
    queuePush(&event);
}

static void pointerButton(void *data, struct wl_pointer *pointer, uint32_t serial,
                          uint32_t time, uint32_t button, uint32_t state) {
    noteInput();
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(ws.pointerWindowID);
    int pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);

    ws.inputSerial = serial;
    if (pressed) {
        ws.pointerButtonSerial = serial;
    }

    // linux/input-event-codes.h: BTN_LEFT 0x110, BTN_RIGHT 0x111, BTN_MIDDLE 0x112.
    if (button == 0x110) {
        event.code = pressed ? WS_LEFT_MOUSE_DOWN : WS_LEFT_MOUSE_UP;
        leftButtonDown = pressed;
    } else if (button == 0x111) {
        event.code = pressed ? WS_RIGHT_MOUSE_DOWN : WS_RIGHT_MOUSE_UP;
        rightButtonDown = pressed;
    } else if (button == 0x112) {
        event.code = pressed ? WS_OTHER_MOUSE_DOWN : WS_OTHER_MOUSE_UP;
        otherButtonDown = pressed;
    } else {
        return;
    }
    event.windowID = ws.pointerWindowID;
    event.x = lastPointerX;
    event.y = window != NULL ? window->h - lastPointerY : lastPointerY;
    event.mods = currentModifiers;
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: pointer button %s window=%d at %.1f,%.1f serial=%u\n",
                pressed ? "down" : "up", event.windowID, event.x, event.y, serial);
    queuePush(&event);
}

// wheel motion arrives in surface pixels, 10 to a notch on most compositors, AppKit's
// deltaY is in lines and positive when scrolling up, the opposite sign to Wayland
static void pointerAxis(void *data, struct wl_pointer *pointer, uint32_t time,
                        uint32_t axis, wl_fixed_t value) {
    noteInput();
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(ws.pointerWindowID);
    double lines = -wl_fixed_to_double(value) / 10.0;

    event.windowID = ws.pointerWindowID;
    event.code = WS_SCROLL_WHEEL;
    event.x = lastPointerX;
    event.y = window != NULL ? window->h - lastPointerY : lastPointerY;
    if (axis == WL_POINTER_AXIS_VERTICAL_SCROLL)
        event.dy = lines;
    else
        event.dx = lines;
    event.mods = currentModifiers;
    queuePush(&event);
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

// the NSEvent modifier bits for keys that have no text
#define NS_NUMERIC_PAD_KEY_MASK (1u << 21)
#define NS_FUNCTION_KEY_MASK (1u << 23)

// the NSUpArrowFunctionKey values and friends
static uint32_t functionKeyForKeysym(xkb_keysym_t sym) {
    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35)
        return 0xF704 + (sym - XKB_KEY_F1);

    switch (sym) {
    case XKB_KEY_Up: case XKB_KEY_KP_Up: return 0xF700;
    case XKB_KEY_Down: case XKB_KEY_KP_Down: return 0xF701;
    case XKB_KEY_Left: case XKB_KEY_KP_Left: return 0xF702;
    case XKB_KEY_Right: case XKB_KEY_KP_Right: return 0xF703;
    case XKB_KEY_Insert: case XKB_KEY_KP_Insert: return 0xF727;
    case XKB_KEY_Delete: case XKB_KEY_KP_Delete: return 0xF728;
    case XKB_KEY_Home: case XKB_KEY_KP_Home: return 0xF729;
    case XKB_KEY_End: case XKB_KEY_KP_End: return 0xF72B;
    case XKB_KEY_Page_Up: case XKB_KEY_KP_Page_Up: return 0xF72C;
    case XKB_KEY_Page_Down: case XKB_KEY_KP_Page_Down: return 0xF72D;
    case XKB_KEY_Print: return 0xF72E;
    case XKB_KEY_Scroll_Lock: return 0xF72F;
    case XKB_KEY_Pause: return 0xF730;
    case XKB_KEY_Menu: return 0xF735;
    case XKB_KEY_Help: return 0xF746;
    }
    return 0;
}

// writes c as NUL-terminated UTF-8 into an 8-byte event field
static void encodeUTF8(uint32_t c, char out[8]) {
    memset(out, 0, 8);
    if (c == 0)
        return;

    if (c < 0x80) {
        out[0] = (char)c;
    } else if (c < 0x800) {
        out[0] = (char)(0xC0 | (c >> 6));
        out[1] = (char)(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        out[0] = (char)(0xE0 | (c >> 12));
        out[1] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[2] = (char)(0x80 | (c & 0x3F));
    } else {
        out[0] = (char)(0xF0 | (c >> 18));
        out[1] = (char)(0x80 | ((c >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((c >> 6) & 0x3F));
        out[3] = (char)(0x80 | (c & 0x3F));
    }
}

// wayland leaves key repeat to the client: rate in keys per second, delay in ms
static struct {
    int32_t rate, delay;
    uint32_t key;
    struct mach_event event;
    struct timespec next;
} keyRepeat = { 25, 600, 0 };

static void timespecAddMs(struct timespec *t, long ms) {
    t->tv_sec += ms / 1000;
    t->tv_nsec += (ms % 1000) * 1000000;
    if (t->tv_nsec >= 1000000000) {
        t->tv_sec++;
        t->tv_nsec -= 1000000000;
    }
}

static void keyRepeatTrack(const struct mach_event *event, uint32_t key, int pressed) {
    if (!pressed) {
        if (key == keyRepeat.key)
            keyRepeat.key = 0;
        return;
    }

    keyRepeat.key = 0;
    if (keyRepeat.rate <= 0 || ws.xkbKeymap == NULL || !xkb_keymap_key_repeats(ws.xkbKeymap, key + 8))
        return;

    keyRepeat.key = key;
    keyRepeat.event = *event;
    keyRepeat.event.repeat = 1;
    clock_gettime(CLOCK_MONOTONIC, &keyRepeat.next);
    timespecAddMs(&keyRepeat.next, keyRepeat.delay);
}

// milliseconds until the held key repeats, -1 when none is held
static int keyRepeatTimeout(void) {
    struct timespec now;

    if (keyRepeat.key == 0)
        return -1;

    clock_gettime(CLOCK_MONOTONIC, &now);
    long ms = (keyRepeat.next.tv_sec - now.tv_sec) * 1000 +
              (keyRepeat.next.tv_nsec - now.tv_nsec) / 1000000;
    return ms < 0 ? 0 : (int)ms;
}

static void keyRepeatFire(void) {
    if (keyRepeat.key == 0 || keyRepeatTimeout() > 0)
        return;

    queuePush(&keyRepeat.event);
    timespecAddMs(&keyRepeat.next, 1000 / keyRepeat.rate);

    // after a stall, carry on from now and skip the backlog
    if (keyRepeatTimeout() == 0) {
        clock_gettime(CLOCK_MONOTONIC, &keyRepeat.next);
        timespecAddMs(&keyRepeat.next, 1000 / keyRepeat.rate);
    }
}

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
    ws.inputSerial = serial;
    if (getenv("PD_WS_TRACE") != NULL) {
        struct wsWindow *w = ws.windows;

        while (w != NULL && w->surface != surface)
            w = w->next;
        fprintf(stderr, "WindowServer: keyboard enter surface %p window %d\n", (void *)surface, w ? w->windowID : 0);
    }
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
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: keyboard leave surface %p\n", (void *)surface);
    keyRepeat.key = 0;
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

// evdev key codes (linux/input-event-codes.h) to the Mac virtual key codes NSEvent keyCode carries.
// 0 means unmapped, since A is the only key whose Mac code is 0
static const uint8_t macKeyCodes[256] = {
    [1] = 0x35,
    [2] = 0x12,
    [3] = 0x13,
    [4] = 0x14,
    [5] = 0x15,
    [6] = 0x17,
    [7] = 0x16,
    [8] = 0x1a,
    [9] = 0x1c,
    [10] = 0x19,
    [11] = 0x1d,
    [12] = 0x1b,
    [13] = 0x18,
    [14] = 0x33,
    [15] = 0x30,
    [16] = 0x0c,
    [17] = 0x0d,
    [18] = 0x0e,
    [19] = 0x0f,
    [20] = 0x11,
    [21] = 0x10,
    [22] = 0x20,
    [23] = 0x22,
    [24] = 0x1f,
    [25] = 0x23,
    [26] = 0x21,
    [27] = 0x1e,
    [28] = 0x24,
    [29] = 0x3b,
    [30] = 0x00,
    [31] = 0x01,
    [32] = 0x02,
    [33] = 0x03,
    [34] = 0x05,
    [35] = 0x04,
    [36] = 0x26,
    [37] = 0x28,
    [38] = 0x25,
    [39] = 0x29,
    [40] = 0x27,
    [41] = 0x32,
    [42] = 0x38,
    [43] = 0x2a,
    [44] = 0x06,
    [45] = 0x07,
    [46] = 0x08,
    [47] = 0x09,
    [48] = 0x0b,
    [49] = 0x2d,
    [50] = 0x2e,
    [51] = 0x2b,
    [52] = 0x2f,
    [53] = 0x2c,
    [54] = 0x3c,
    [55] = 0x43,
    [56] = 0x3a,
    [57] = 0x31,
    [58] = 0x39,
    [59] = 0x7a,
    [60] = 0x78,
    [61] = 0x63,
    [62] = 0x76,
    [63] = 0x60,
    [64] = 0x61,
    [65] = 0x62,
    [66] = 0x64,
    [67] = 0x65,
    [68] = 0x6d,
    [69] = 0x47,
    [71] = 0x59,
    [72] = 0x5b,
    [73] = 0x5c,
    [74] = 0x4e,
    [75] = 0x56,
    [76] = 0x57,
    [77] = 0x58,
    [78] = 0x45,
    [79] = 0x53,
    [80] = 0x54,
    [81] = 0x55,
    [82] = 0x52,
    [83] = 0x41,
    [86] = 0x0a,
    [87] = 0x67,
    [88] = 0x6f,
    [96] = 0x4c,
    [97] = 0x3e,
    [98] = 0x4b,
    [100] = 0x3d,
    [102] = 0x73,
    [103] = 0x7e,
    [104] = 0x74,
    [105] = 0x7b,
    [106] = 0x7c,
    [107] = 0x77,
    [108] = 0x7d,
    [109] = 0x79,
    [110] = 0x72,
    [111] = 0x75,
    [113] = 0x4a,
    [114] = 0x49,
    [115] = 0x48,
    [117] = 0x51,
    [125] = 0x37,
    [126] = 0x36,
    [138] = 0x72,
    [183] = 0x69,
    [184] = 0x6b,
    [185] = 0x71,
    [186] = 0x6a,
    [187] = 0x40,
    [188] = 0x4f,
    [189] = 0x50,
    [190] = 0x5a,
};

static unsigned short macKeyCode(uint32_t key) {
    if (key == 30)
        return 0x00;
    return key < 256 && macKeyCodes[key] ? macKeyCodes[key] : 0xFFFF;
}

static void keyboardKey(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                        uint32_t time, uint32_t key, uint32_t state) {
    ws.inputSerial = serial;
    noteInput();
    struct mach_event event = {0};
    /* Wayland reports evdev codes; xkb keycodes are evdev + 8. */
    xkb_keycode_t keycode = key + 8;

    event.windowID = ws.focusWindowID;
    event.code = (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? WS_KEY_DOWN : WS_KEY_UP;
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: key %u state %u to window %d\n", key, state, ws.focusWindowID);
    event.keycode = macKeyCode(key);
    event.mods = currentModifiers;
    struct wsWindow *window = windowForID(ws.focusWindowID);
    event.x = window != NULL ? ws.lastMouseX - window->x : lastPointerX;
    event.y = window != NULL ? ws.lastMouseY - window->y : lastPointerY;

    if (ws.xkbState != NULL) {
        xkb_keysym_t sym = xkb_state_key_get_one_sym(ws.xkbState, keycode);
        uint32_t function = functionKeyForKeysym(sym);

        // keys with no text get AppKit's private-use characters, as on macOS
        if (function != 0) {
            encodeUTF8(function, event.chars);
            encodeUTF8(function, event.charsIg);
            event.mods |= NS_FUNCTION_KEY_MASK;
            if (function >= 0xF700 && function <= 0xF703)
                event.mods |= NS_NUMERIC_PAD_KEY_MASK;
        } else {
            xkb_state_key_get_utf8(ws.xkbState, keycode, event.chars, sizeof(event.chars));
            // the keysym already has shift applied but not control, which is what this wants
            encodeUTF8(xkb_keysym_to_utf32(sym), event.charsIg);

            // macOS reports backspace as DEL and shift-tab as NSBackTabCharacter
            if (sym == XKB_KEY_BackSpace || sym == XKB_KEY_ISO_Left_Tab) {
                uint32_t c = (sym == XKB_KEY_BackSpace) ? 0x7F : 0x19;
                encodeUTF8(c, event.chars);
                encodeUTF8(c, event.charsIg);
            }
        }
    }
    queuePush(&event);
    keyRepeatTrack(&event, key, state == WL_KEYBOARD_KEY_STATE_PRESSED);
}

static void keyboardModifiers(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                              uint32_t depressed, uint32_t latched, uint32_t locked,
                              uint32_t group) {
    if (ws.xkbState != NULL) {
        xkb_state_update_mask(ws.xkbState, depressed, latched, locked, 0, 0, group);
    }

    /* NSEvent modifier flags. */
    currentModifiers = 0;
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
}

static void keyboardRepeatInfo(void *data, struct wl_keyboard *keyboard,
                               int32_t rate, int32_t delay) {
    keyRepeat.rate = rate;
    keyRepeat.delay = delay;
    if (rate <= 0)
        keyRepeat.key = 0;
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
    struct pollfd pfd = { .fd = wl_display_get_fd(ws.display), .events = POLLIN };

    // wl_display_dispatch by hand, so the wait can end in time for a key repeat
    while (ws.dispatchRunning) {
        while (wl_display_prepare_read(ws.display) != 0)
            wl_display_dispatch_pending(ws.display);
        wl_display_flush(ws.display);

        int ready = poll(&pfd, 1, keyRepeatTimeout());
        if (ready > 0) {
            if (wl_display_read_events(ws.display) < 0)
                break;
        } else {
            wl_display_cancel_read(ws.display);
        }

        if (wl_display_dispatch_pending(ws.display) < 0) {
            fprintf(stderr,"WindowServer: Wayland dispatch stopped with error %d\n",
                    wl_display_get_error(ws.display));
            break;
        }
        keyRepeatFire();
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

// a tiling compositor sizes toplevels itself, zero means the client chooses
static void toplevelConfigure(void *data, struct xdg_toplevel *toplevel,
                              int32_t width, int32_t height, struct wl_array *states) {
    struct wsWindow *window = data;
    struct mach_event event = {0};

    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: toplevel %d configure %dx%d (now %gx%g)\n",
                window->windowID, width, height, window->w, window->h);
    if (width <= 0 || height <= 0 || (width == (int32_t)window->w && height == (int32_t)window->h))
        return;

    event.windowID = window->windowID;
    event.code = WS_EVENT_CONFIGURE;
    event.dx = width;
    event.dy = height;
    queuePush(&event);
}

static void toplevelClose(void *data, struct xdg_toplevel *toplevel) {
    struct wsWindow *window = data;
    struct mach_event event = {0};

    event.windowID = window->windowID;
    event.code = WS_EVENT_CLOSE;
    queuePush(&event);
}

static void windowPositionChanged(void *data, struct pd_window_position_v1 *position, int32_t x, int32_t y) {
    struct wsWindow *window = data;
    struct mach_event event = {0};

    /* Pointer events need the compositor's current placement to turn the
     * surface-local Wayland position into AppKit screen coordinates. The
     * RPC-created frame is only an initial request and can be stale as soon
     * as the compositor places or moves the toplevel. */
    window->x = x;
    if (ws.screenH > 0)
        window->y = ws.screenH - y - (int)window->h;

    event.windowID = window->windowID;
    event.code = WS_EVENT_MOVED;
    event.x = x;
    event.y = y;
    queuePush(&event);
}

static const struct pd_window_position_v1_listener positionListener = {
    .position = windowPositionChanged,
};

static const struct xdg_toplevel_listener toplevelListener = {
    .configure = toplevelConfigure,
    .close = toplevelClose,
};

// AppKit frames count up from the bottom, the compositor's from the top
static void windowSendPosition(struct wsWindow *window) {
    if (window->position == NULL || ws.screenH <= 0)
        return;
    pd_window_position_v1_set_position(window->position, (int32_t)window->x,
                                       (int32_t)(ws.screenH - window->y - window->h));
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
        // the desktop, dock, bars and menus never take the keyboard from the active app
        // a modal panel takes it when shown, anything else when clicked
        if (ws.layerShellVersion >= 4) {
            uint32_t keys = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND;

            // a modal panel and a login or lock screen hold the keyboard as soon as they show
            if (window->level == kCGModalPanelWindowLevelKey || window->level >= kCGScreenSaverWindowLevelKey)
                keys = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE;
            else if (window->level <= kCGPopUpMenuWindowLevelKey)
                keys = ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE;
            zwlr_layer_surface_v1_set_keyboard_interactivity(window->layerSurface, keys);
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
    xdg_toplevel_add_listener(window->toplevel, &toplevelListener, window);
    if (ws.positionManager != NULL) {
        window->position = pd_window_position_manager_v1_get_window_position(ws.positionManager, window->surface);
        pd_window_position_v1_add_listener(window->position, &positionListener, window);
        windowSendPosition(window);
    }

    // compositors match window rules and desktop entries on the app id
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFStringRef identifier = bundle != NULL ? CFBundleGetIdentifier(bundle) : NULL;
    char appID[256];
    if (identifier != NULL && CFStringGetCString(identifier, appID, sizeof(appID), kCFStringEncodingUTF8))
        xdg_toplevel_set_app_id(window->toplevel, appID);

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
            (void *)window->front, (void *)window->layerSurface,
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
    if (window->position != NULL) {
        pd_window_position_v1_destroy(window->position);
        window->position = NULL;
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
            if (window->blurEffect != NULL) { ext_background_effect_surface_v1_destroy(window->blurEffect); }
            if (window->surface != NULL)    { wl_surface_destroy(window->surface); }
            windowReleaseFrames(window);
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

        // the frame on screen stays up until AppKit flushes one drawn at the new size
        pthread_mutex_lock(&ws.pixelsLock);
        windowReleaseBuffer(window);
        windowCreateBuffer(window);
        pthread_mutex_unlock(&ws.pixelsLock);
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
                if (window->toplevel != NULL && window->state == FULLSCREEN)
                    xdg_toplevel_unset_fullscreen(window->toplevel);
                if (window->toplevel != NULL) { xdg_toplevel_set_maximized(window->toplevel); }
                break;
            case FULLSCREEN:
                if (window->toplevel != NULL)
                    xdg_toplevel_set_fullscreen(window->toplevel, NULL);
                break;
            case NORMAL:
                if (window->toplevel != NULL && window->state == FULLSCREEN)
                    xdg_toplevel_unset_fullscreen(window->toplevel);
                else if (window->toplevel != NULL) { xdg_toplevel_unset_maximized(window->toplevel); }
                /* window->state is still the *previous* state here - it is
                 * assigned after this switch - so this is the test for "coming
                 * back from an unmapped state". Both MINIMIZED and HIDDEN
                 * detach the buffer, and without re-attaching it the window
                 * never reappears however often NORMAL is sent. */
                if (window->state == MINIMIZED || window->state == HIDDEN) {
                    // a surface without its role yet is mapped by the first flush instead
                    window->state = NORMAL;
                    if ((window->toplevel != NULL || window->layerSurface != NULL) &&
                        (windowShowFront(window) || windowPresent(window))) {
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
    } else if (moved && window->toplevel != NULL) {
        // the app moved its own window, the compositor's moves come back as WS_EVENT_MOVED and never get here
        windowSendPosition(window);
    }

    wl_display_flush(ws.display);
    return KERN_SUCCESS;
}

// sends the window's blur region, applied by the compositor on the window's next commit
static void windowApplyBlur(struct wsWindow *window) {
    struct wl_region *region;
    int w = (int)window->w - 2 * window->blurInset, h = (int)window->h - 2 * window->blurInset;
    int r = window->blurRadius, x0 = window->blurInset, y0 = window->blurInset;

    window->blurDirty = 0;
    if (ws.blurManager == NULL || window->surface == NULL) {
        return;
    }
    if (!window->blurOn || w <= 0 || h <= 0) {
        if (window->blurEffect != NULL) {
            ext_background_effect_surface_v1_destroy(window->blurEffect);
            window->blurEffect = NULL;
        }
        return;
    }
    if (window->blurEffect == NULL) {
        window->blurEffect = ext_background_effect_manager_v1_get_background_effect(ws.blurManager, window->surface);
    }
    region = wl_compositor_create_region(ws.compositor);
    r = r * 2 > w ? w / 2 : r * 2 > h ? h / 2 : r;
    // the middle band, then the rounded rows top and bottom stepped in to follow the corner
    wl_region_add(region, x0, y0 + r, w, h - 2 * r);
    for (int i = 0; i < r; i++) {
        double dy = r - i - 0.5;
        int in = r - (int)(sqrt((double)r * r - dy * dy) + 0.5);

        wl_region_add(region, x0 + in, y0 + i, w - 2 * in, 1);
        wl_region_add(region, x0 + in, y0 + h - 1 - i, w - 2 * in, 1);
    }
    ext_background_effect_surface_v1_set_blur_region(window->blurEffect, region);
    wl_region_destroy(region);
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
            if (window->pixels == NULL) {
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
            if (window->blurDirty) {
                windowApplyBlur(window);
            }
            // an empty input region keeps the compositor from ever giving this surface the pointer
            if (window->inputDirty) {
                struct wl_region *empty = window->ignoresMouse ? wl_compositor_create_region(ws.compositor) : NULL;

                wl_surface_set_input_region(window->surface, empty);
                if (empty != NULL)
                    wl_region_destroy(empty);
                window->inputDirty = 0;
            }
            // hand the compositor a copy of the backing store, so it never reads a frame mid-draw
            if (window->state != MINIMIZED && window->state != HIDDEN && windowPresent(window)) {
                wl_surface_commit(window->surface);
                wl_display_flush(ws.display);
            }
            return KERN_SUCCESS;
        }

        case kWSWindowSetBlur: {
            struct wsRPCSimple *msg = (struct wsRPCSimple *)data;
            struct wsWindow *window = windowForID(msg->val1);

            if (window == NULL) {
                return KERN_FAILURE;
            }
            window->blurOn = msg->val2, window->blurInset = msg->val3, window->blurRadius = msg->val4;
            window->blurDirty = 1;
            return KERN_SUCCESS;
        }

        case kWSWindowSetIgnoresMouse: {
            struct wsRPCSimple *msg = (struct wsRPCSimple *)data;
            struct wsWindow *window = windowForID(msg->val1);

            if (window == NULL) {
                return KERN_FAILURE;
            }
            window->ignoresMouse = msg->val2;
            window->inputDirty = 1;
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
            if (getenv("PD_WS_TRACE") != NULL)
                fprintf(stderr, "WindowServer: toplevel %d resize edges %d at %gx%g in %gx%g\n",
                        window->windowID, msg->val2, lastPointerX, lastPointerY, window->w, window->h);
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

// metal: a CAMetalLayer presents with Vulkan into its own child surface of a window. The child is desynchronised,
// so its frames show without AppKit committing the window, and position changes apply on the window's next commit, made here.
void *_windowServerWaylandDisplay(void) {
    return ensureConnected() ? ws.display : NULL;
}

// a child surface not yet placed in any window, for a Vulkan surface made before its view's window is on screen
void *_windowServerCreateSurface(void) {
    struct wl_surface *surface;
    struct wl_region *none;

    if (!ensureConnected()) {
        return NULL;
    }
    surface = wl_compositor_create_surface(ws.compositor);
    // the window takes the input, a child left with the default region would swallow every click over it
    none = wl_compositor_create_region(ws.compositor);
    wl_surface_set_input_region(surface, none);
    wl_region_destroy(none);
    wl_display_flush(ws.display);
    return surface;
}

void *_windowServerAttachSubsurface(int windowID, void *surface, int x, int y) {
    struct wsWindow *window;
    struct wl_subsurface *subsurface;

    if (surface == NULL || !ensureConnected() || ws.subcompositor == NULL || (window = windowForID(windowID)) == NULL) {
        return NULL;
    }
    subsurface = wl_subcompositor_get_subsurface(ws.subcompositor, surface, window->surface);
    wl_subsurface_set_position(subsurface, x, y);
    wl_subsurface_set_desync(subsurface);
    wl_surface_commit(window->surface);
    wl_display_flush(ws.display);
    return subsurface;
}

static void exportHandle(void *data, struct pd_surface_export_v1 *export, uint32_t handle) {
    *(uint32_t *)data = handle;
}

static const struct pd_surface_export_v1_listener exportListener = {
    .handle = exportHandle,
};

// CAContext: a handle another process imports to show this surface in its window, 0 without the compositor's support.
// the export lives as long as the surface
uint32_t _windowServerExportSurface(void *surface) {
    struct wl_event_queue *queue;
    struct pd_surface_host_manager_v1 *host;
    struct pd_surface_export_v1 *export;
    uint32_t handle = 0;

    if (surface == NULL || !ensureConnected() || ws.surfaceHost == NULL) {
        return 0;
    }
    // the reply is read here on a private queue, the dispatch thread keeps the default one
    queue = wl_display_create_queue(ws.display);
    host = wl_proxy_create_wrapper(ws.surfaceHost);
    wl_proxy_set_queue((struct wl_proxy *)host, queue);
    export = pd_surface_host_manager_v1_export_surface(host, surface);
    pd_surface_export_v1_add_listener(export, &exportListener, &handle);
    while (handle == 0 && wl_display_roundtrip_queue(ws.display, queue) >= 0) {
    }
    wl_proxy_set_queue((struct wl_proxy *)export, NULL);
    wl_proxy_wrapper_destroy(host);
    wl_event_queue_destroy(queue);
    return handle;
}

// CALayerHost: an exported surface of another process as a subsurface of this window at (x, y)
void *_windowServerImportSubsurface(int windowID, uint32_t handle, int x, int y) {
    struct wsWindow *window;
    struct wl_subsurface *subsurface;

    if (handle == 0 || !ensureConnected() || ws.surfaceHost == NULL || (window = windowForID(windowID)) == NULL) {
        return NULL;
    }
    subsurface = pd_surface_host_manager_v1_import_subsurface(ws.surfaceHost, handle, window->surface);
    wl_subsurface_set_position(subsurface, x, y);
    wl_subsurface_set_desync(subsurface);
    wl_surface_commit(window->surface);
    wl_display_flush(ws.display);
    return subsurface;
}

void *_windowServerCreateSubsurface(int windowID, int x, int y, void **subsurfaceOut) {
    struct wl_surface *surface;

    if (!ensureConnected() || ws.subcompositor == NULL || windowForID(windowID) == NULL) {
        return NULL;
    }
    surface = _windowServerCreateSurface();
    *subsurfaceOut = _windowServerAttachSubsurface(windowID, surface, x, y);
    return surface;
}

void _windowServerMoveSubsurface(int windowID, void *subsurface, int x, int y) {
    struct wsWindow *window = windowForID(windowID);

    if (window == NULL || subsurface == NULL) {
        return;
    }
    wl_subsurface_set_position(subsurface, x, y);
    wl_surface_commit(window->surface);
    wl_display_flush(ws.display);
}

void _windowServerDestroySubsurface(void *surface, void *subsurface) {
    if (subsurface != NULL) {
        wl_subsurface_destroy(subsurface);
    }
    if (surface != NULL) {
        wl_surface_destroy(surface);
    }
    if (ws.display != NULL) {
        wl_display_flush(ws.display);
    }
}


void *_windowServerCreateFramePresenter(void *surface) {
    if (!surface || !ensureConnected()) return NULL;
    struct wsWindow *presenter = calloc(1, sizeof(*presenter));
    if (presenter) presenter->surface = surface;
    return presenter;
}

static struct wsFrame *presenterFrame(void *handle, void *pixels, int width, int height) {
    struct wsWindow *presenter = handle;
    if (!presenter || !pixels) return NULL;
    for (int i = 0; i < WS_FRAMES; i++) {
        struct wsFrame *frame = &presenter->frames[i];
        if (frame->pixels == pixels && frame->w == width && frame->h == height && frame->busy)
            return frame;
    }
    return NULL;
}

void *_windowServerAcquireFrame(void *handle, int width, int height, int *pitch) {
    struct wsWindow *presenter = handle;
    if (!presenter || width <= 0 || height <= 0 || width > 16384 || height > 16384 ||
        !ws.display || !ws.dispatchRunning)
        return NULL;
    struct wsFrame *frame = windowFreeFrame(presenter, width, height);
    if (!frame) return NULL;
    if (pitch) *pitch = width * 4;
    return frame->pixels;
}

int _windowServerCommitFrame(void *handle, void *pixels, int width, int height) {
    struct wsWindow *presenter = handle;
    struct wsFrame *frame = presenterFrame(handle, pixels, width, height);
    if (!frame || !ws.display || !ws.dispatchRunning) return -1;
    wl_surface_attach(presenter->surface, frame->buffer, 0, 0);
    wl_surface_damage(presenter->surface, 0, 0, width, height);
    wl_surface_commit(presenter->surface);
    return wl_display_flush(ws.display) < 0 && errno != EAGAIN ? -1 : 0;
}

void _windowServerCancelFrame(void *handle, void *pixels) {
    struct wsWindow *presenter = handle;
    if (!presenter || !pixels) return;
    for (int i = 0; i < WS_FRAMES; i++) {
        struct wsFrame *frame = &presenter->frames[i];
        if (frame->pixels == pixels) {
            pthread_mutex_lock(&ws.queueLock);
            frame->busy = 0;
            pthread_cond_broadcast(&ws.queueReady);
            pthread_mutex_unlock(&ws.queueLock);
            return;
        }
    }
}

int _windowServerPresentFrame(void *handle, const void *pixels, int width, int height, int pitch) {
    if (!pixels || pitch < (int64_t)width * 4) return -1;
    int dst_pitch;
    void *destination = _windowServerAcquireFrame(handle, width, height, &dst_pitch);
    if (!destination) return -1;
    for (int y = 0; y < height; y++)
        memcpy((char *)destination + (size_t)y * dst_pitch, (const char *)pixels + (size_t)y * pitch, (size_t)width * 4);
    int rc = _windowServerCommitFrame(handle, destination, width, height);
    if (rc) _windowServerCancelFrame(handle, destination);
    return rc;
}

void _windowServerDestroyFramePresenter(void *handle) {
    struct wsWindow *presenter = handle;
    if (!presenter) return;
    for (int i = 0; i < WS_FRAMES; i++) frameDestroy(&presenter->frames[i]);
    free(presenter);
}

// the selection: what this process offers through wl_data_source, and the newest offer from another client
struct wsOffer {
    struct wl_data_offer *offer;
    char **mimeTypes;
    int count;
};

static struct {
    pthread_mutex_t lock;
    struct wsOffer *pending;
    struct wsOffer *current;
    unsigned generation;
    struct wl_data_source *source;
    WSSelectionSendHandler handler;
} selection = { .lock = PTHREAD_MUTEX_INITIALIZER };

// a drag from any client over our surfaces: the offer, where it is, and what the source allows
static struct {
    pthread_mutex_t lock;
    struct wsOffer *offer;
    struct wl_surface *surface;
    int windowID;
    uint32_t serial;
    uint32_t sourceActions;
    uint32_t action;
} dragIn = { .lock = PTHREAD_MUTEX_INITIALIZER };

static void offerFree(struct wsOffer *offer) {
    if (offer == NULL)
        return;
    if (offer->offer != NULL)
        wl_data_offer_destroy(offer->offer);
    for (int i = 0; i < offer->count; i++)
        free(offer->mimeTypes[i]);
    free(offer->mimeTypes);
    free(offer);
}

static void offerMimeType(void *data, struct wl_data_offer *proxy, const char *mimeType) {
    struct wsOffer *offer = data;
    char **grown = realloc(offer->mimeTypes, sizeof(char *) * (size_t)(offer->count + 1));

    if (grown == NULL)
        return;
    offer->mimeTypes = grown;
    offer->mimeTypes[offer->count++] = strdup(mimeType);
}

static void offerSourceActions(void *data, struct wl_data_offer *proxy, uint32_t actions) {
    pthread_mutex_lock(&dragIn.lock);
    dragIn.sourceActions = actions;
    pthread_mutex_unlock(&dragIn.lock);
}

static void offerAction(void *data, struct wl_data_offer *proxy, uint32_t action) {
    pthread_mutex_lock(&dragIn.lock);
    dragIn.action = action;
    pthread_mutex_unlock(&dragIn.lock);
}

static const struct wl_data_offer_listener offerListener = {
    .offer = offerMimeType,
    .source_actions = offerSourceActions,
    .action = offerAction,
};

// a new offer announces its types before the selection or enter event that uses it
static void deviceDataOffer(void *data, struct wl_data_device *device, struct wl_data_offer *proxy) {
    struct wsOffer *offer = calloc(1, sizeof(*offer));

    if (offer == NULL) {
        wl_data_offer_destroy(proxy);
        return;
    }
    offer->offer = proxy;
    wl_data_offer_add_listener(proxy, &offerListener, offer);
    pthread_mutex_lock(&selection.lock);
    offerFree(selection.pending);
    selection.pending = offer;
    pthread_mutex_unlock(&selection.lock);
}

static void dragInPush(int code, double x, double y) {
    struct mach_event event = {0};
    struct wsWindow *window = windowForID(dragIn.windowID);

    event.windowID = dragIn.windowID;
    event.code = code;
    event.x = x;
    event.y = window != NULL ? window->h - y : y;
    event.mods = currentModifiers;
    queuePush(&event);
}

static void deviceEnter(void *data, struct wl_data_device *device, uint32_t serial, struct wl_surface *surface,
                        wl_fixed_t x, wl_fixed_t y, struct wl_data_offer *proxy) {
    int windowID = 0;

    for (struct wsWindow *w = ws.windows; w != NULL; w = w->next) {
        if (w->surface == surface)
            windowID = w->windowID;
    }
    pthread_mutex_lock(&selection.lock);
    pthread_mutex_lock(&dragIn.lock);
    offerFree(dragIn.offer);
    dragIn.offer = NULL;
    if (proxy != NULL && selection.pending != NULL && selection.pending->offer == proxy) {
        dragIn.offer = selection.pending;
        selection.pending = NULL;
    }
    dragIn.surface = surface;
    dragIn.windowID = windowID;
    dragIn.serial = serial;
    dragIn.action = 0;
    pthread_mutex_unlock(&dragIn.lock);
    pthread_mutex_unlock(&selection.lock);
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: drag enter window=%d offer %p\n", windowID, (void *)proxy);
    if (windowID != 0)
        dragInPush(WS_EVENT_DRAG_ENTER, wl_fixed_to_double(x), wl_fixed_to_double(y));
}

static void deviceLeave(void *data, struct wl_data_device *device) {
    int windowID;

    pthread_mutex_lock(&dragIn.lock);
    windowID = dragIn.windowID;
    pthread_mutex_unlock(&dragIn.lock);
    if (windowID != 0)
        dragInPush(WS_EVENT_DRAG_LEAVE, 0, 0);
    pthread_mutex_lock(&dragIn.lock);
    offerFree(dragIn.offer);
    dragIn.offer = NULL;
    dragIn.surface = NULL;
    dragIn.windowID = 0;
    pthread_mutex_unlock(&dragIn.lock);
}

static void deviceMotion(void *data, struct wl_data_device *device, uint32_t time, wl_fixed_t x, wl_fixed_t y) {
    if (dragIn.windowID != 0)
        dragInPush(WS_EVENT_DRAG_MOTION, wl_fixed_to_double(x), wl_fixed_to_double(y));
}

// the offer stays until AppKit has read the data and called _windowServerDragFinish
static void deviceDrop(void *data, struct wl_data_device *device) {
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: drag drop window=%d\n", dragIn.windowID);
    if (dragIn.windowID != 0)
        dragInPush(WS_EVENT_DRAG_DROP, 0, 0);
}

static void deviceSelection(void *data, struct wl_data_device *device, struct wl_data_offer *proxy) {
    pthread_mutex_lock(&selection.lock);
    offerFree(selection.current);
    selection.current = NULL;
    if (proxy != NULL && selection.pending != NULL && selection.pending->offer == proxy) {
        selection.current = selection.pending;
        selection.pending = NULL;
    }
    selection.generation++;
    pthread_mutex_unlock(&selection.lock);
}

static const struct wl_data_device_listener deviceListener = {
    .data_offer = deviceDataOffer,
    .enter = deviceEnter,
    .leave = deviceLeave,
    .motion = deviceMotion,
    .drop = deviceDrop,
    .selection = deviceSelection,
};

static void dataDeviceStart(void) {
    ws.dataDevice = wl_data_device_manager_get_data_device(ws.dataDeviceManager, ws.seat);
    wl_data_device_add_listener(ws.dataDevice, &deviceListener, NULL);
}

static void sourceTarget(void *data, struct wl_data_source *source, const char *mimeType) {
}

// another client asked for our selection: AppKit writes the bytes and closes the fd
static void sourceSend(void *data, struct wl_data_source *source, const char *mimeType, int fd) {
    WSSelectionSendHandler handler;

    pthread_mutex_lock(&selection.lock);
    handler = source == selection.source ? selection.handler : NULL;
    pthread_mutex_unlock(&selection.lock);
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: selection send %s fd %d handler %p\n", mimeType, fd, (void *)handler);
    if (handler != NULL)
        handler(mimeType, fd);
    else
        close(fd);
}

// someone else owns the selection now
static void sourceCancelled(void *data, struct wl_data_source *source) {
    pthread_mutex_lock(&selection.lock);
    if (source == selection.source) {
        selection.source = NULL;
        selection.handler = NULL;
    }
    pthread_mutex_unlock(&selection.lock);
    wl_data_source_destroy(source);
}

static const struct wl_data_source_listener sourceListener = {
    .target = sourceTarget,
    .send = sourceSend,
    .cancelled = sourceCancelled,
};

int _windowServerSetSelection(const char *const *mimeTypes, int count, WSSelectionSendHandler handler) {
    struct wl_data_source *source;

    // the clipboard only follows a process that already has windows, it never connects on its own
    if (!ws.connected || ws.dataDevice == NULL)
        return 0;
    source = wl_data_device_manager_create_data_source(ws.dataDeviceManager);
    wl_data_source_add_listener(source, &sourceListener, NULL);
    for (int i = 0; i < count; i++)
        wl_data_source_offer(source, mimeTypes[i]);
    pthread_mutex_lock(&selection.lock);
    selection.source = source;
    selection.handler = handler;
    pthread_mutex_unlock(&selection.lock);
    wl_data_device_set_selection(ws.dataDevice, count > 0 ? source : NULL, ws.inputSerial);
    wl_display_flush(ws.display);
    return 1;
}

int _windowServerSelectionIsOurs(void) {
    int ours;

    pthread_mutex_lock(&selection.lock);
    ours = selection.source != NULL;
    pthread_mutex_unlock(&selection.lock);
    return ours;
}

unsigned _windowServerSelectionGeneration(void) {
    unsigned generation;

    if (!ws.connected)
        return 0;
    pthread_mutex_lock(&selection.lock);
    generation = selection.generation;
    pthread_mutex_unlock(&selection.lock);
    return generation;
}

int _windowServerCopySelectionMimeTypes(char ***mimeTypesOut) {
    int count = 0;

    *mimeTypesOut = NULL;
    pthread_mutex_lock(&selection.lock);
    if (selection.current != NULL && selection.current->count > 0) {
        *mimeTypesOut = calloc((size_t)selection.current->count, sizeof(char *));
        for (int i = 0; *mimeTypesOut != NULL && i < selection.current->count; i++)
            (*mimeTypesOut)[count++] = strdup(selection.current->mimeTypes[i]);
    }
    pthread_mutex_unlock(&selection.lock);
    return count;
}

static void *readPipe(int fd, const char *what, const char *mimeType, size_t *lengthOut);

void *_windowServerReadSelection(const char *mimeType, size_t *lengthOut) {
    int fds[2];

    *lengthOut = 0;
    if (!ws.connected || pipe(fds) != 0)
        return NULL;
    pthread_mutex_lock(&selection.lock);
    if (selection.current == NULL) {
        pthread_mutex_unlock(&selection.lock);
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    wl_data_offer_receive(selection.current->offer, mimeType, fds[1]);
    pthread_mutex_unlock(&selection.lock);
    close(fds[1]);
    wl_display_flush(ws.display);
    return readPipe(fds[0], "selection", mimeType, lengthOut);
}

// the bytes another client writes into a pipe, giving up after two quiet seconds
static void *readPipe(int fd, const char *what, const char *mimeType, size_t *lengthOut) {
    char *buffer = NULL;
    size_t length = 0, capacity = 0;

    for (;;) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN };
        ssize_t got;

        if (poll(&pfd, 1, 2000) <= 0)
            break;
        if (length == capacity) {
            char *grown = realloc(buffer, capacity ? capacity * 2 : 4096);
            if (grown == NULL)
                break;
            buffer = grown;
            capacity = capacity ? capacity * 2 : 4096;
        }
        got = read(fd, buffer + length, capacity - length);
        if (got <= 0)
            break;
        length += (size_t)got;
    }
    close(fd);
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: %s read %s gave %zu bytes\n", what, mimeType, length);
    *lengthOut = length;
    if (buffer == NULL)
        buffer = malloc(1);
    return buffer;
}

// the drag this process started, from start_drag until the compositor says how it ended
static struct {
    pthread_mutex_t lock;
    struct wl_data_source *source;
    struct wl_surface *icon;
    struct wl_buffer *iconBuffer;
    void *iconPixels;
    size_t iconSize;
    WSSelectionSendHandler handler;
    int state;
    uint32_t action;
} dragOut = { .lock = PTHREAD_MUTEX_INITIALIZER };

static void dragOutSetState(struct wl_data_source *source, int state) {
    pthread_mutex_lock(&dragOut.lock);
    if (source == dragOut.source && dragOut.state != WS_DRAG_FINISHED && dragOut.state != WS_DRAG_CANCELLED)
        dragOut.state = state;
    pthread_mutex_unlock(&dragOut.lock);
}

static void dragSourceTarget(void *data, struct wl_data_source *source, const char *mimeType) {
}

static void dragSourceSend(void *data, struct wl_data_source *source, const char *mimeType, int fd) {
    WSSelectionSendHandler handler;

    pthread_mutex_lock(&dragOut.lock);
    handler = source == dragOut.source ? dragOut.handler : NULL;
    pthread_mutex_unlock(&dragOut.lock);
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: drag send %s fd %d\n", mimeType, fd);
    if (handler != NULL)
        handler(mimeType, fd);
    else
        close(fd);
}

static void dragSourceCancelled(void *data, struct wl_data_source *source) {
    dragOutSetState(source, WS_DRAG_CANCELLED);
}

static void dragSourceDropPerformed(void *data, struct wl_data_source *source) {
    dragOutSetState(source, WS_DRAG_DROPPED);
}

static void dragSourceFinished(void *data, struct wl_data_source *source) {
    dragOutSetState(source, WS_DRAG_FINISHED);
}

static void dragSourceAction(void *data, struct wl_data_source *source, uint32_t action) {
    pthread_mutex_lock(&dragOut.lock);
    if (source == dragOut.source)
        dragOut.action = action;
    pthread_mutex_unlock(&dragOut.lock);
}

static const struct wl_data_source_listener dragSourceListener = {
    .target = dragSourceTarget,
    .send = dragSourceSend,
    .cancelled = dragSourceCancelled,
    .dnd_drop_performed = dragSourceDropPerformed,
    .dnd_finished = dragSourceFinished,
    .action = dragSourceAction,
};

// a surface showing the drag image under the pointer, the hot spot is where the pointer holds it
static struct wl_surface *dragIcon(const void *argb, int width, int height, int hotX, int hotY) {
    size_t size = (size_t)width * 4 * (size_t)height;
    struct wl_surface *surface;
    char name[32];
    int fd;

    if (argb == NULL || width <= 0 || height <= 0)
        return NULL;
    snprintf(name, sizeof(name), "/wsd.%u", (unsigned)getpid());
    fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0)
        return NULL;
    shm_unlink(name);
    if (ftruncate(fd, (off_t)size) != 0 ||
        (dragOut.iconPixels = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
        dragOut.iconPixels = NULL;
        close(fd);
        return NULL;
    }
    struct wl_shm_pool *pool = wl_shm_create_pool(ws.shm, fd, (int32_t)size);
    memcpy(dragOut.iconPixels, argb, size);
    dragOut.iconBuffer = wl_shm_pool_create_buffer(pool, 0, width, height, width * 4, WL_SHM_FORMAT_ARGB8888);
    dragOut.iconSize = size;
    wl_shm_pool_destroy(pool);
    close(fd);

    surface = wl_compositor_create_surface(ws.compositor);
    wl_surface_attach(surface, dragOut.iconBuffer, -hotX, -hotY);
    wl_surface_damage(surface, 0, 0, width, height);
    wl_surface_commit(surface);
    return surface;
}

static void dragOutClear(void) {
    if (dragOut.source != NULL)
        wl_data_source_destroy(dragOut.source);
    if (dragOut.icon != NULL)
        wl_surface_destroy(dragOut.icon);
    if (dragOut.iconBuffer != NULL)
        wl_buffer_destroy(dragOut.iconBuffer);
    if (dragOut.iconPixels != NULL)
        munmap(dragOut.iconPixels, dragOut.iconSize);
    dragOut.source = NULL;
    dragOut.icon = NULL;
    dragOut.iconBuffer = NULL;
    dragOut.iconPixels = NULL;
    dragOut.handler = NULL;
    dragOut.state = WS_DRAG_NONE;
    dragOut.action = 0;
}

int _windowServerDragStart(int windowID, const char *const *mimeTypes, int count, uint32_t actions,
                           const void *argb, int width, int height, int hotX, int hotY,
                           WSSelectionSendHandler handler) {
    struct wsWindow *window = windowForID(windowID);

    if (!ws.connected || ws.dataDevice == NULL || window == NULL || window->surface == NULL)
        return 0;
    pthread_mutex_lock(&dragOut.lock);
    dragOutClear();
    dragOut.source = wl_data_device_manager_create_data_source(ws.dataDeviceManager);
    wl_data_source_add_listener(dragOut.source, &dragSourceListener, NULL);
    for (int i = 0; i < count; i++)
        wl_data_source_offer(dragOut.source, mimeTypes[i]);
    if (wl_proxy_get_version((struct wl_proxy *)dragOut.source) >= 3)
        wl_data_source_set_actions(dragOut.source, actions);
    dragOut.handler = handler;
    dragOut.state = WS_DRAG_ACTIVE;
    dragOut.icon = dragIcon(argb, width, height, hotX, hotY);
    pthread_mutex_unlock(&dragOut.lock);
    if (getenv("PD_WS_TRACE") != NULL)
        fprintf(stderr, "WindowServer: drag start window=%d types %d serial %u\n", windowID, count, ws.pointerButtonSerial);
    wl_data_device_start_drag(ws.dataDevice, dragOut.source, window->surface, dragOut.icon, ws.pointerButtonSerial);
    wl_display_flush(ws.display);
    return 1;
}

int _windowServerDragState(uint32_t *actionOut) {
    int state;

    pthread_mutex_lock(&dragOut.lock);
    state = dragOut.state;
    if (actionOut != NULL)
        *actionOut = dragOut.action;
    pthread_mutex_unlock(&dragOut.lock);
    return state;
}

void _windowServerDragEnd(void) {
    pthread_mutex_lock(&dragOut.lock);
    dragOutClear();
    pthread_mutex_unlock(&dragOut.lock);
    if (ws.connected)
        wl_display_flush(ws.display);
}

int _windowServerCopyDragMimeTypes(char ***mimeTypesOut, uint32_t *sourceActionsOut) {
    int count = 0;

    *mimeTypesOut = NULL;
    pthread_mutex_lock(&dragIn.lock);
    if (dragIn.offer != NULL && dragIn.offer->count > 0) {
        *mimeTypesOut = calloc((size_t)dragIn.offer->count, sizeof(char *));
        for (int i = 0; *mimeTypesOut != NULL && i < dragIn.offer->count; i++)
            (*mimeTypesOut)[count++] = strdup(dragIn.offer->mimeTypes[i]);
    }
    if (sourceActionsOut != NULL)
        *sourceActionsOut = dragIn.sourceActions;
    pthread_mutex_unlock(&dragIn.lock);
    return count;
}

// the destination's answer: a type it would take, or NULL, and the actions it allows
void _windowServerDragAccept(const char *mimeType, uint32_t actions, uint32_t preferred) {
    pthread_mutex_lock(&dragIn.lock);
    if (dragIn.offer != NULL) {
        wl_data_offer_accept(dragIn.offer->offer, dragIn.serial, mimeType);
        if (wl_proxy_get_version((struct wl_proxy *)dragIn.offer->offer) >= 3)
            wl_data_offer_set_actions(dragIn.offer->offer, actions, preferred);
    }
    pthread_mutex_unlock(&dragIn.lock);
    if (ws.connected)
        wl_display_flush(ws.display);
}

uint32_t _windowServerDragAction(void) {
    uint32_t action;

    pthread_mutex_lock(&dragIn.lock);
    action = dragIn.action;
    pthread_mutex_unlock(&dragIn.lock);
    return action;
}

void *_windowServerReadDrag(const char *mimeType, size_t *lengthOut) {
    int fds[2];

    *lengthOut = 0;
    if (!ws.connected || pipe(fds) != 0)
        return NULL;
    pthread_mutex_lock(&dragIn.lock);
    if (dragIn.offer == NULL) {
        pthread_mutex_unlock(&dragIn.lock);
        close(fds[0]);
        close(fds[1]);
        return NULL;
    }
    wl_data_offer_receive(dragIn.offer->offer, mimeType, fds[1]);
    pthread_mutex_unlock(&dragIn.lock);
    close(fds[1]);
    wl_display_flush(ws.display);
    return readPipe(fds[0], "drag", mimeType, lengthOut);
}

// after a drop the destination tells the source it is done, and the offer goes
void _windowServerDragFinish(void) {
    pthread_mutex_lock(&dragIn.lock);
    if (dragIn.offer != NULL) {
        if (wl_proxy_get_version((struct wl_proxy *)dragIn.offer->offer) >= 3)
            wl_data_offer_finish(dragIn.offer->offer);
        offerFree(dragIn.offer);
        dragIn.offer = NULL;
    }
    dragIn.surface = NULL;
    dragIn.windowID = 0;
    pthread_mutex_unlock(&dragIn.lock);
    if (ws.connected)
        wl_display_flush(ws.display);
}
