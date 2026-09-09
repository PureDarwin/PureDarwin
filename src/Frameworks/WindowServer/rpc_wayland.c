/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <WindowServer/rpc.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"
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
    struct wsWindow *next;
};

static struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *wmBase;
    struct zwlr_layer_shell_v1 *layerShell;
    struct wl_seat *seat;
    struct wl_output *output;
    struct wl_shm *shm;
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
    uint32_t pointerButtonSerial;
    struct wsWindow *windows;
    int connected;
    int lastMouseX, lastMouseY;
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
        ws.layerShell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        ws.shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
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
    ws.connected = 1;
    ws.dispatchRunning = 1;
    pthread_create(&ws.dispatchThread, NULL, dispatchThreadMain, NULL);
    return 1;
}

static void xdgSurfaceConfigure(void *data, struct xdg_surface *surface, uint32_t serial) {
    struct wsWindow *window = data;

    xdg_surface_ack_configure(surface, serial);
    if (window->buffer == NULL) {
        windowAttachBuffer(window);
    } else {
        /* Surviving buffer from a role rebuild: the new role starts unmapped,
         * so attach it again or the surface never shows anything. */
        wl_surface_attach(window->surface, window->buffer, 0, 0);
        wl_surface_damage(window->surface, 0, 0, (int)window->w, (int)window->h);
    }
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
    if (window->buffer == NULL) {
        windowAttachBuffer(window);
    } else {
        /* Surviving buffer from a role rebuild: the new role starts unmapped,
         * so attach it again or the surface never shows anything. */
        wl_surface_attach(window->surface, window->buffer, 0, 0);
        wl_surface_damage(window->surface, 0, 0, (int)window->w, (int)window->h);
    }
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
    if (level == kCGDockWindowLevelKey || level == kCGMainMenuWindowLevelKey) {
        *layerOut = ZWLR_LAYER_SHELL_V1_LAYER_TOP;
        return 1;
    }
    return 0;
}

static void windowShmPath(int windowID, char *out, size_t outSize) {
    CFBundleRef bundle = CFBundleGetMainBundle();
    CFStringRef identifier = (bundle != NULL) ? CFBundleGetIdentifier(bundle) : NULL;
    char bundleID[192];

    if (identifier == NULL ||
        !CFStringGetCString(identifier, bundleID, sizeof(bundleID), kCFStringEncodingUTF8)) {
        snprintf(bundleID, sizeof(bundleID), "unix.%u", (unsigned)getpid());
    }
    snprintf(out, outSize, "/%s/%u/win/%u", bundleID, (unsigned)getpid(),
             (unsigned)windowID);
}

/* Backing store sized to the frame, ARGB8888 - which is what AppKit's
 * O2Surface writes (premultiplied-first, host byte order). */
static int windowAttachBuffer(struct wsWindow *window) {
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
    wl_surface_attach(window->surface, window->buffer, 0, 0);
    wl_surface_damage(window->surface, 0, 0, width, height);
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
            ws.focusWindowID = w->windowID;
            break;
        }
    }
}

static void keyboardLeave(void *data, struct wl_keyboard *keyboard, uint32_t serial,
                          struct wl_surface *surface) {
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

    if (layerForWindowLevel(window->level, &layer)) {
        window->layerSurface = zwlr_layer_shell_v1_get_layer_surface(
            ws.layerShell, window->surface, NULL, layer,
            title != NULL && title[0] != '\0' ? title : "puredarwin");
        zwlr_layer_surface_v1_add_listener(window->layerSurface,
                                           &layerSurfaceListener, window);
        zwlr_layer_surface_v1_set_size(window->layerSurface,
                                       (uint32_t)window->w, (uint32_t)window->h);
        /* Anchoring to all four edges makes the compositor size the surface to
         * the output, which is what a desktop wants. */
        if (layer == ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND) {
            zwlr_layer_surface_v1_set_anchor(window->layerSurface,
                ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
                ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);
            zwlr_layer_surface_v1_set_exclusive_zone(window->layerSurface, -1);
        }
        return;
    }

    window->xdgSurface = xdg_wm_base_get_xdg_surface(ws.wmBase, window->surface);
    xdg_surface_add_listener(window->xdgSurface, &xdgSurfaceListener, window);
    window->toplevel = xdg_surface_get_toplevel(window->xdgSurface);

    if (title != NULL && title[0] != '\0') {
        xdg_toplevel_set_title(window->toplevel, title);
    }
}

static void windowDestroyRole(struct wsWindow *window) {
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
    windowCreateRole(window, msg->title);

    window->next = ws.windows;
    ws.windows = window;

    /* Commit the role first so the compositor sends its configure. The
     * dispatch thread creates and attaches the buffer from the configure
     * callback; it is the only thread that reads from the Wayland display. */
    wl_surface_commit(window->surface);
    wl_display_flush(ws.display);

    pthread_mutex_lock(&ws.queueLock);
    while (!window->configured && ws.dispatchRunning) {
        pthread_cond_wait(&ws.queueReady, &ws.queueLock);
    }
    pthread_mutex_unlock(&ws.queueLock);

    if (!window->configured) {
        return KERN_FAILURE;
    }
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
                if (window->toplevel != NULL) { xdg_toplevel_set_minimized(window->toplevel); }
                break;
            case MAXIMIZED:
                if (window->toplevel != NULL) { xdg_toplevel_set_maximized(window->toplevel); }
                break;
            case NORMAL:
                if (window->toplevel != NULL) { xdg_toplevel_unset_maximized(window->toplevel); }
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

    window->x = msg->x;
    window->y = msg->y;
    window->w = msg->w;
    window->h = msg->h;

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

            fprintf(stderr, "WindowServer: flush win=%d found=%d buffer=%d\n",
                    msg->windowID, window != NULL,
                    window != NULL && window->buffer != NULL);
            if (window == NULL || window->buffer == NULL) {
                return KERN_SUCCESS;
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
