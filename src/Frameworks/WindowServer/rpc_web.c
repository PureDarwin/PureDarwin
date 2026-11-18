/*
 * rpc_web.c - the window-server RPCs, served by a web page: a canvas per window, DOM input as events.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#include <WindowServer/rpc.h>
#include <CoreGraphics/CGDirectDisplay_puredarwin.h>
#include <emscripten.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

// NSEventType values, as the Wayland backend sends them
enum {
    WS_LEFT_MOUSE_DOWN = 1, WS_LEFT_MOUSE_UP = 2, WS_RIGHT_MOUSE_DOWN = 3, WS_RIGHT_MOUSE_UP = 4,
    WS_MOUSE_MOVED = 5, WS_LEFT_MOUSE_DRAGGED = 6, WS_RIGHT_MOUSE_DRAGGED = 7, WS_KEY_DOWN = 10, WS_KEY_UP = 11,
    WS_SCROLL_WHEEL = 22, WS_OTHER_MOUSE_DOWN = 25, WS_OTHER_MOUSE_UP = 26, WS_OTHER_MOUSE_DRAGGED = 27,
};

struct wsWindow {
    struct wsWindow *next;
    int windowID;
    double x, y, w, h; // Cocoa screen coordinates, origin bottom left
    int pixelsW, pixelsH;
    uint32_t *pixels;
    int state, level, style;
    unsigned order;
};

static struct {
    int connected;
    struct wsWindow *windows;
    unsigned nextOrder;
    int screenW, screenH;
    int keyWindowID, activeAppWindowID;
    int lastMouseX, lastMouseY; // screen coordinates from the top left
    struct mach_event queue[512];
    unsigned head, tail;
    void (*pump)(void);
    uint64_t lastInput;
    // the clipboard while this page owns it
    char **selectionTypes;
    int selectionCount;
    WSSelectionSendHandler selectionHandler;
    unsigned selectionGeneration;
} ws;

static struct wsWindow *windowForID(int windowID)
{
    for (struct wsWindow *w = ws.windows; w; w = w->next)
        if (w->windowID == windowID)
            return w;
    return NULL;
}

// the page: a screen element filling the viewport, a canvas per window, input listeners that call back here
EM_JS(void, wsWebInstall, (void), {
    if (Module.pdScreen)
        return;
    var screen = document.getElementById('pd-screen');
    if (!screen) {
        screen = document.createElement('div');
        screen.id = 'pd-screen';
        screen.style.cssText = 'position:fixed;inset:0;overflow:hidden;background:#3a6ea5;touch-action:none;user-select:none';
        document.body.appendChild(screen);
    }
    Module.pdScreen = screen;
    // KeyboardEvent.code to the Mac virtual keycode AppKit expects (ANSI layout)
    var keys = { KeyA: 0x00, KeyS: 0x01, KeyD: 0x02, KeyF: 0x03, KeyH: 0x04, KeyG: 0x05, KeyZ: 0x06, KeyX: 0x07,
        KeyC: 0x08, KeyV: 0x09, KeyB: 0x0B, KeyQ: 0x0C, KeyW: 0x0D, KeyE: 0x0E, KeyR: 0x0F, KeyY: 0x10, KeyT: 0x11,
        Digit1: 0x12, Digit2: 0x13, Digit3: 0x14, Digit4: 0x15, Digit6: 0x16, Digit5: 0x17, Equal: 0x18, Digit9: 0x19,
        Digit7: 0x1A, Minus: 0x1B, Digit8: 0x1C, Digit0: 0x1D, BracketRight: 0x1E, KeyO: 0x1F, KeyU: 0x20,
        BracketLeft: 0x21, KeyI: 0x22, KeyP: 0x23, Enter: 0x24, KeyL: 0x25, KeyJ: 0x26, Quote: 0x27, KeyK: 0x28,
        Semicolon: 0x29, Backslash: 0x2A, Comma: 0x2B, Slash: 0x2C, KeyN: 0x2D, KeyM: 0x2E, Period: 0x2F, Tab: 0x30,
        Space: 0x31, Backquote: 0x32, Backspace: 0x33, Escape: 0x35, MetaRight: 0x36, MetaLeft: 0x37, ShiftLeft: 0x38,
        CapsLock: 0x39, AltLeft: 0x3A, ControlLeft: 0x3B, ShiftRight: 0x3C, AltRight: 0x3D, ControlRight: 0x3E,
        F5: 0x60, F6: 0x61, F7: 0x62, F3: 0x63, F8: 0x64, F9: 0x65, F11: 0x67, F10: 0x6D, F12: 0x6F, Home: 0x73,
        PageUp: 0x74, Delete: 0x75, F4: 0x76, End: 0x77, F2: 0x78, PageDown: 0x79, F1: 0x7A, ArrowLeft: 0x7B,
        ArrowRight: 0x7C, ArrowDown: 0x7D, ArrowUp: 0x7E };
    // keys with no text get the function-key characters NSEvent uses
    var functions = { ArrowUp: 0xF700, ArrowDown: 0xF701, ArrowLeft: 0xF702, ArrowRight: 0xF703, F1: 0xF704,
        F2: 0xF705, F3: 0xF706, F4: 0xF707, F5: 0xF708, F6: 0xF709, F7: 0xF70A, F8: 0xF70B, F9: 0xF70C, F10: 0xF70D,
        F11: 0xF70E, F12: 0xF70F, Home: 0xF729, End: 0xF72B, PageUp: 0xF72C, PageDown: 0xF72D, Delete: 0xF728 };
    var controls = { Enter: 13, Tab: 9, Backspace: 127, Escape: 27 };
    function mods(e) {
        return (e.shiftKey ? 1 << 17 : 0) | (e.ctrlKey ? 1 << 18 : 0) | (e.altKey ? 1 << 19 : 0) | (e.metaKey ? 1 << 20 : 0);
    }
    function windowAt(e) {
        var t = e.target;
        while (t && t !== screen && !t.pdWindow)
            t = t.parentNode;
        return t && t.pdWindow ? t : null;
    }
    var buttons = 0, grab = null;
    function pointer(e, phase) {
        var c = grab || windowAt(e);
        var r = c ? c.getBoundingClientRect() : screen.getBoundingClientRect();
        var x = e.clientX - r.left, y = e.clientY - r.top;
        var id = c ? c.pdWindow : 0;
        var code;
        if (phase == 0) {
            buttons |= 1 << e.button;
            code = e.button == 0 ? 1 : e.button == 2 ? 3 : 25;
            grab = c;
        } else if (phase == 1) {
            buttons &= ~(1 << e.button);
            code = e.button == 0 ? 2 : e.button == 2 ? 4 : 26;
            if (!buttons)
                grab = null;
        } else {
            code = buttons & 1 ? 6 : buttons & 4 ? 7 : buttons ? 27 : 5;
        }
        Module._wsWebPointer(id, code, x, c ? c.height - y : y, e.movementX || 0, e.movementY || 0, mods(e),
            e.clientX, e.clientY);
    }
    screen.addEventListener('pointerdown', function(e) { pointer(e, 0); e.preventDefault(); });
    window.addEventListener('pointerup', function(e) { pointer(e, 1); });
    window.addEventListener('pointermove', function(e) { pointer(e, 2); });
    screen.addEventListener('contextmenu', function(e) { e.preventDefault(); });
    screen.addEventListener('wheel', function(e) {
        var c = windowAt(e);
        if (!c)
            return;
        var r = c.getBoundingClientRect();
        // DOM wheel deltas grow downwards in pixels, NSEvent scroll lines grow upwards
        var scale = e.deltaMode == 1 ? 1 : e.deltaMode == 2 ? 20 : 1 / 10;
        Module._wsWebScroll(c.pdWindow, e.clientX - r.left, c.height - (e.clientY - r.top), -e.deltaX * scale,
            -e.deltaY * scale, mods(e));
        e.preventDefault();
    }, { passive: false });
    function key(e, down) {
        var k = keys[e.code];
        var ch = 0, ign = 0;
        if (functions[e.code] !== undefined) {
            ch = ign = functions[e.code];
        } else if (controls[e.key] !== undefined) {
            ch = ign = controls[e.key];
        } else if (e.key.length >= 1 && [...e.key].length == 1) {
            ch = e.key.codePointAt(0);
            ign = e.shiftKey ? ch : e.key.toLowerCase().codePointAt(0);
        }
        Module._wsWebKey(down ? 10 : 11, k === undefined ? 0xFFFF : k, mods(e) | (functions[e.code] !== undefined ? 1 << 23 : 0),
            ch, ign, e.repeat ? 1 : 0);
        // the page keeps the browser's own shortcuts away from AppKit's
        if (k !== undefined || ch)
            e.preventDefault();
    }
    window.addEventListener('keydown', function(e) { key(e, 1); });
    window.addEventListener('keyup', function(e) { key(e, 0); });
    window.addEventListener('resize', function() { Module._wsWebResize(screen.clientWidth, screen.clientHeight); });
});

EM_JS(int, wsWebScreenWidth, (void), { return Module.pdScreen.clientWidth; });
EM_JS(int, wsWebScreenHeight, (void), { return Module.pdScreen.clientHeight; });

EM_JS(void, wsWebWindowCreate, (int id, int w, int h), {
    var c = document.createElement('canvas');
    c.pdWindow = id;
    c.id = 'pd-window-' + id;
    c.width = Math.max(1, w);
    c.height = Math.max(1, h);
    c.style.cssText = 'position:absolute;display:none';
    Module.pdScreen.appendChild(c);
});

EM_JS(void, wsWebWindowDestroy, (int id), {
    var c = document.getElementById('pd-window-' + id);
    if (c)
        c.remove();
});

// left and top in CSS pixels, z the stacking order, visible 0 or 1
EM_JS(void, wsWebWindowPlace, (int id, int left, int top, int w, int h, int z, int visible), {
    var c = document.getElementById('pd-window-' + id);
    if (!c)
        return;
    if (c.width != w || c.height != h) {
        c.width = Math.max(1, w);
        c.height = Math.max(1, h);
    }
    c.style.left = left + 'px';
    c.style.top = top + 'px';
    c.style.zIndex = z;
    c.style.display = visible ? 'block' : 'none';
});

EM_JS(void, wsWebWindowTitle, (int id, const char *title), {
    var c = document.getElementById('pd-window-' + id);
    if (c)
        c.title = UTF8ToString(title);
});

// AppKit draws premultiplied BGRA (32-bit little endian, alpha first), a canvas takes straight RGBA
EM_JS(void, wsWebWindowBlit, (int id, const uint8_t *pixels, int w, int h, int dx, int dy, int dw, int dh), {
    var c = document.getElementById('pd-window-' + id);
    if (!c || dw <= 0 || dh <= 0)
        return;
    var ctx = c.pdContext || (c.pdContext = c.getContext('2d'));
    var image = ctx.createImageData(dw, dh);
    var out = image.data;
    for (var row = 0; row < dh; row++) {
        var src = pixels + ((dy + row) * w + dx) * 4;
        var o = row * dw * 4;
        for (var col = 0; col < dw; col++, src += 4, o += 4) {
            var a = HEAPU8[src + 3];
            var b = HEAPU8[src], g = HEAPU8[src + 1], r = HEAPU8[src + 2];
            if (a && a != 255) {
                r = r * 255 / a;
                g = g * 255 / a;
                b = b * 255 / a;
            }
            out[o] = r;
            out[o + 1] = g;
            out[o + 2] = b;
            out[o + 3] = a;
        }
    }
    ctx.putImageData(image, dx, dy);
});

// follows the pointer until the button comes up, then reports where the window ended
EM_JS(void, wsWebBeginMove, (int id), {
    var c = document.getElementById('pd-window-' + id);
    if (!c)
        return;
    var startX = null, startY = null, left = parseInt(c.style.left) || 0, top = parseInt(c.style.top) || 0;
    function move(e) {
        if (startX === null) {
            startX = e.clientX;
            startY = e.clientY;
        }
        c.style.left = (left + e.clientX - startX) + 'px';
        c.style.top = Math.max(0, top + e.clientY - startY) + 'px';
        e.stopPropagation();
    }
    function up(e) {
        window.removeEventListener('pointermove', move, true);
        window.removeEventListener('pointerup', up, true);
        Module._wsWebMoved(id, parseInt(c.style.left) || 0, parseInt(c.style.top) || 0);
    }
    window.addEventListener('pointermove', move, true);
    window.addEventListener('pointerup', up, true);
});

EM_JS(void, wsWebSetCursor, (const char *css), { Module.pdScreen.style.cursor = UTF8ToString(css); });

static uint64_t nowNanos(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static void queuePush(const struct mach_event *event)
{
    unsigned next = (ws.tail + 1) % (unsigned)(sizeof(ws.queue) / sizeof(ws.queue[0]));
    if (next == ws.head)
        return; // full: the page is not keeping up, the oldest events stay
    ws.queue[ws.tail] = *event;
    ws.tail = next;
}

// AppKit drains the queue from the same browser callback that filled it
static void deliver(const struct mach_event *event)
{
    ws.lastInput = nowNanos();
    queuePush(event);
    if (ws.pump)
        ws.pump();
}

void _windowServerWebSetPump(void (*pump)(void))
{
    ws.pump = pump;
}

static int screenTop(const struct wsWindow *w)
{
    return ws.screenH - (int)(w->y + w->h);
}

static void place(struct wsWindow *w)
{
    int visible = w->state != MINIMIZED && w->state != HIDDEN && w->state != CLOSED && w->order != 0;
    // levels group the stacking, the order within a level is the last time the window was brought forward
    int z = (w->level + 100) * 100000 + (int)(w->order % 100000);
    wsWebWindowPlace(w->windowID, (int)w->x, screenTop(w), w->pixelsW, w->pixelsH, z, visible);
}

static void allocatePixels(struct wsWindow *w)
{
    int pw = (int)w->w, ph = (int)w->h;
    if (pw < 1)
        pw = 1;
    if (ph < 1)
        ph = 1;
    if (w->pixels != NULL && pw == w->pixelsW && ph == w->pixelsH)
        return;
    free(w->pixels);
    w->pixels = calloc((size_t)pw * ph, 4);
    w->pixelsW = pw;
    w->pixelsH = ph;
}

EMSCRIPTEN_KEEPALIVE void wsWebPointer(int windowID, int code, double x, double y, double dx, double dy, unsigned mods,
                                       double screenX, double screenY)
{
    struct mach_event event = {0};
    event.windowID = windowID;
    event.code = code;
    event.x = x;
    event.y = y;
    event.dx = dx;
    event.dy = dy;
    event.mods = mods;
    ws.lastMouseX = (int)screenX;
    ws.lastMouseY = (int)screenY;
    if ((code == WS_LEFT_MOUSE_DOWN || code == WS_RIGHT_MOUSE_DOWN) && windowID != 0)
        ws.keyWindowID = ws.activeAppWindowID = windowID;
    if (windowID == 0)
        return;
    deliver(&event);
}

EMSCRIPTEN_KEEPALIVE void wsWebScroll(int windowID, double x, double y, double dx, double dy, unsigned mods)
{
    struct mach_event event = {0};
    event.windowID = windowID;
    event.code = WS_SCROLL_WHEEL;
    event.x = x;
    event.y = y;
    event.dx = dx;
    event.dy = dy;
    event.mods = mods;
    deliver(&event);
}

static void encodeUTF8(unsigned c, char out[8])
{
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

EMSCRIPTEN_KEEPALIVE void wsWebKey(int code, int keycode, unsigned mods, unsigned chars, unsigned charsIg, int repeat)
{
    struct mach_event event = {0};
    if (ws.keyWindowID == 0)
        return;
    event.windowID = ws.keyWindowID;
    event.code = code;
    event.keycode = (unsigned short)keycode;
    event.mods = mods;
    event.repeat = repeat;
    encodeUTF8(chars, event.chars);
    encodeUTF8(charsIg, event.charsIg);
    deliver(&event);
}

// the end of a titlebar drag: the frame follows, and AppKit hears of the move as from a compositor
EMSCRIPTEN_KEEPALIVE void wsWebMoved(int windowID, int left, int top)
{
    struct wsWindow *w = windowForID(windowID);
    struct mach_event event = {0};
    if (w == NULL)
        return;
    w->x = left;
    w->y = ws.screenH - top - w->h;
    event.windowID = windowID;
    event.code = WS_EVENT_MOVED;
    event.x = left;
    event.y = top;
    deliver(&event);
}

EMSCRIPTEN_KEEPALIVE void wsWebResize(int width, int height)
{
    struct mach_event event = {0};
    ws.screenW = width;
    ws.screenH = height;
    CGDirectDisplaySetMainDisplayBounds(CGRectMake(0, 0, width, height));
    for (struct wsWindow *w = ws.windows; w; w = w->next)
        place(w);
    event.code = WS_EVENT_USABLE_AREA;
    deliver(&event);
}

static int ensureConnected(void)
{
    if (ws.connected)
        return 1;
    wsWebInstall();
    ws.screenW = wsWebScreenWidth();
    ws.screenH = wsWebScreenHeight();
    CGDirectDisplaySetMainDisplayBounds(CGRectMake(0, 0, ws.screenW, ws.screenH));
    ws.connected = 1;
    return 1;
}

int _windowServerConnect(void)
{
    return ensureConnected();
}

// the page is the display, so CoreGraphics learns its size before AppKit asks. The discoverer is called
// through a void (void) pointer, and wasm traps a call through any other type, so it is a function of its own
static void discoverDisplay(void)
{
    ensureConnected();
}

__attribute__((constructor)) static void registerDiscoverer(void)
{
    CGDirectDisplaySetDiscoverer(discoverDisplay);
}

// never blocks: 0 once the queue is empty, AppKit runs again when the next event arrives
int _windowServerReceiveMessage(PortMessage *msg)
{
    if (msg == NULL || !ws.connected || ws.head == ws.tail)
        return 0;
    struct mach_event event = ws.queue[ws.head];
    ws.head = (ws.head + 1) % (unsigned)(sizeof(ws.queue) / sizeof(ws.queue[0]));
    memset(msg, 0, sizeof(*msg));
    msg->header.msgh_id = MSG_ID_INLINE;
    msg->code = CODE_INPUT_EVENT;
    msg->len = (int)sizeof(event);
    msg->pid = (int)getpid();
    memcpy(msg->data, &event, sizeof(event));
    return 1;
}

void *_windowServerWindowBuffer(int windowID, size_t *sizeOut)
{
    struct wsWindow *w = windowForID(windowID);
    if (w == NULL || w->pixels == NULL)
        return NULL;
    if (sizeOut)
        *sizeOut = (size_t)w->pixelsW * w->pixelsH * 4;
    return w->pixels;
}

static void setFrame(struct wsWindow *w, const struct wsRPCWindow *msg)
{
    w->x = msg->x;
    w->y = msg->y;
    w->w = msg->w;
    w->h = msg->h;
    allocatePixels(w);
    place(w);
}

kern_return_t _windowServerRPC(void *data, size_t len, void *reply, int *replyLen)
{
    struct wsRPCBase *base = data;
    if (!ensureConnected() || base == NULL)
        return KERN_FAILURE;

    switch (base->code) {
    case kWSWindowCreate: {
        struct wsRPCWindow *msg = data;
        if (windowForID(msg->windowID) != NULL)
            return KERN_SUCCESS;
        struct wsWindow *w = calloc(1, sizeof(*w));
        w->windowID = msg->windowID;
        w->style = msg->style;
        w->level = msg->level;
        w->state = NORMAL;
        w->next = ws.windows;
        ws.windows = w;
        wsWebWindowCreate(w->windowID, (int)msg->w, (int)msg->h);
        setFrame(w, msg);
        return KERN_SUCCESS;
    }
    case kWSWindowDestroy: {
        struct wsRPCWindow *msg = data;
        for (struct wsWindow **p = &ws.windows; *p; p = &(*p)->next) {
            if ((*p)->windowID != msg->windowID)
                continue;
            struct wsWindow *dead = *p;
            *p = dead->next;
            wsWebWindowDestroy(dead->windowID);
            free(dead->pixels);
            free(dead);
            break;
        }
        if (ws.keyWindowID == msg->windowID)
            ws.keyWindowID = 0;
        return KERN_SUCCESS;
    }
    // AppKit sends the whole window each time anything changes: frame, title, level and state together
    case kWSWindowModifyState: {
        struct wsRPCWindow *msg = data;
        struct wsWindow *w = windowForID(msg->windowID);
        if (w == NULL)
            return KERN_FAILURE;
        int shown = w->state == NORMAL || w->state == MAXIMIZED || w->state == FULLSCREEN;
        w->state = msg->state;
        w->level = msg->level;
        w->style = msg->style;
        if (msg->title[0] != '\0')
            wsWebWindowTitle(w->windowID, msg->title);
        // a window coming on screen goes in front, as a compositor maps a new toplevel
        int showing = w->state == NORMAL || w->state == MAXIMIZED || w->state == FULLSCREEN;
        if (showing && (!shown || w->order == 0)) {
            w->order = ++ws.nextOrder;
            if (ws.keyWindowID == 0 || !shown)
                ws.keyWindowID = ws.activeAppWindowID = w->windowID;
        }
        if (msg->w > 0 && msg->h > 0)
            setFrame(w, msg);
        else
            place(w);
        return KERN_SUCCESS;
    }
    case kWSWindowSetFrame: {
        struct wsRPCWindow *msg = data;
        struct wsWindow *w = windowForID(msg->windowID);
        if (w == NULL)
            return KERN_FAILURE;
        setFrame(w, msg);
        return KERN_SUCCESS;
    }
    // the titlebar asks the server to move the window with the pointer until the button comes up
    case kWSWindowMove: {
        struct wsRPCWindow *msg = data;
        if (windowForID(msg->windowID) == NULL)
            return KERN_FAILURE;
        wsWebBeginMove(msg->windowID);
        return KERN_SUCCESS;
    }
    case kWSWindowResize:
        return KERN_SUCCESS;
    case kWSWindowSetTitle: {
        struct wsRPCWindow *msg = data;
        wsWebWindowTitle(msg->windowID, msg->title);
        return KERN_SUCCESS;
    }
    case kWSWindowFlush: {
        struct wsRPCWindow *msg = data;
        struct wsWindow *w = windowForID(msg->windowID);
        if (w == NULL || w->pixels == NULL)
            return KERN_SUCCESS;
        int x = 0, y = 0, dw = w->pixelsW, dh = w->pixelsH;
        if (msg->damageValid) {
            x = msg->damageX;
            y = msg->damageY;
            dw = msg->damageW;
            dh = msg->damageH;
            if (x < 0) { dw += x; x = 0; }
            if (y < 0) { dh += y; y = 0; }
            if (x + dw > w->pixelsW) dw = w->pixelsW - x;
            if (y + dh > w->pixelsH) dh = w->pixelsH - y;
        }
        wsWebWindowBlit(w->windowID, (const uint8_t *)w->pixels, w->pixelsW, w->pixelsH, x, y, dw, dh);
        return KERN_SUCCESS;
    }
    case kWSWindowSetBlur:
    case kWSWindowSetIgnoresMouse:
    case kWSWindowSetConfinement:
    case kCGAssociateMouseAndMouseCursorPosition:
    case kCGWarpMouseCursorPosition:
        return KERN_SUCCESS;
    case kCGGetLastMouseDelta:
    case kCGGetMouseLocation: {
        if (reply == NULL || replyLen == NULL || (size_t)*replyLen < sizeof(struct wsRPCSimple))
            return KERN_INVALID_ARGUMENT;
        struct wsRPCSimple *out = reply;
        memset(out, 0, sizeof(*out));
        if (base->code == kCGGetMouseLocation) {
            out->val1 = ws.lastMouseX;
            out->val2 = ws.lastMouseY;
        }
        *replyLen = sizeof(struct wsRPCSimple);
        return KERN_SUCCESS;
    }
    case kWSGetWindowInfo: {
        struct wsRPCWindow *in = data;
        if (reply == NULL || replyLen == NULL || (size_t)*replyLen < sizeof(struct wsRPCWindow))
            return KERN_INVALID_ARGUMENT;
        struct wsWindow *w = windowForID(in->windowID != 0 ? in->windowID : ws.activeAppWindowID);
        if (w == NULL)
            return KERN_FAILURE;
        struct wsRPCWindow *out = reply;
        memset(out, 0, sizeof(*out));
        out->windowID = w->windowID;
        out->x = w->x;
        out->y = w->y;
        out->w = w->w;
        out->h = w->h;
        out->state = w->state;
        out->level = w->level;
        out->style = w->style;
        *replyLen = sizeof(struct wsRPCWindow);
        return KERN_SUCCESS;
    }
    case kWSGetUsableArea: {
        if (reply == NULL || replyLen == NULL || (size_t)*replyLen < sizeof(struct wsRPCSimple))
            return KERN_INVALID_ARGUMENT;
        struct wsRPCSimple *out = reply;
        out->val1 = 0;
        out->val2 = 0;
        out->val3 = ws.screenW;
        out->val4 = ws.screenH;
        *replyLen = sizeof(struct wsRPCSimple);
        return KERN_SUCCESS;
    }
    }
    return KERN_SUCCESS;
}

int _windowServerActivateWindow(int windowID)
{
    struct wsWindow *w = windowForID(windowID);
    if (w == NULL)
        return 0;
    ws.keyWindowID = ws.activeAppWindowID = windowID;
    w->order = ++ws.nextOrder;
    place(w);
    return 1;
}

uint64_t _windowServerLastInputTime(void)
{
    return ws.lastInput;
}

// AppKit's cursor names to CSS cursors
void _windowServerSetCursorNamed(const char *name)
{
    static const char *const map[][2] = {
        { "default", "default" }, { "left_ptr", "default" }, { "arrow", "default" }, { "text", "text" },
        { "xterm", "text" }, { "pointer", "pointer" }, { "hand2", "pointer" }, { "grab", "grab" },
        { "grabbing", "grabbing" }, { "crosshair", "crosshair" }, { "not-allowed", "not-allowed" },
        { "col-resize", "col-resize" }, { "row-resize", "row-resize" }, { "ew-resize", "ew-resize" },
        { "ns-resize", "ns-resize" }, { "nwse-resize", "nwse-resize" }, { "nesw-resize", "nesw-resize" },
        { "copy", "copy" }, { "alias", "alias" }, { "context-menu", "context-menu" }, { "wait", "wait" },
    };
    const char *css = "default";
    for (size_t i = 0; name != NULL && i < sizeof(map) / sizeof(map[0]); i++)
        if (strcmp(map[i][0], name) == 0)
            css = map[i][1];
    wsWebSetCursor(css);
}

void _windowServerSetCursorImage(const void *argb, int width, int height, int hotX, int hotY)
{
    (void)argb;
    (void)width;
    (void)height;
    (void)hotX;
    (void)hotY;
    wsWebSetCursor("default");
}

void _windowServerSetCursorHidden(int hidden)
{
    wsWebSetCursor(hidden ? "none" : "default");
}

// the clipboard inside the page: what this app offers, read back through its own send handler
int _windowServerSetSelection(const char *const *mimeTypes, int count, WSSelectionSendHandler handler)
{
    for (int i = 0; i < ws.selectionCount; i++)
        free(ws.selectionTypes[i]);
    free(ws.selectionTypes);
    ws.selectionTypes = count > 0 ? calloc((size_t)count, sizeof(char *)) : NULL;
    for (int i = 0; i < count; i++)
        ws.selectionTypes[i] = strdup(mimeTypes[i]);
    ws.selectionCount = count;
    ws.selectionHandler = handler;
    ws.selectionGeneration++;
    return 1;
}

int _windowServerSelectionIsOurs(void)
{
    return ws.selectionHandler != NULL;
}

unsigned _windowServerSelectionGeneration(void)
{
    return ws.selectionGeneration;
}

int _windowServerCopySelectionMimeTypes(char ***mimeTypesOut)
{
    *mimeTypesOut = NULL;
    if (ws.selectionCount == 0)
        return 0;
    char **out = calloc((size_t)ws.selectionCount, sizeof(char *));
    for (int i = 0; i < ws.selectionCount; i++)
        out[i] = strdup(ws.selectionTypes[i]);
    *mimeTypesOut = out;
    return ws.selectionCount;
}

void *_windowServerReadSelection(const char *mimeType, size_t *lengthOut)
{
    int fds[2];
    if (ws.selectionHandler == NULL || pipe(fds) != 0)
        return NULL;
    ws.selectionHandler(mimeType, fds[1]);
    size_t len = 0, cap = 4096;
    char *buf = malloc(cap);
    ssize_t n;
    while ((n = read(fds[0], buf + len, cap - len)) > 0) {
        len += (size_t)n;
        if (len == cap)
            buf = realloc(buf, cap *= 2);
    }
    close(fds[0]);
    if (lengthOut)
        *lengthOut = len;
    return buf;
}

// a page has no other clients to drag to or from yet
int _windowServerDragStart(int windowID, const char *const *mimeTypes, int count, uint32_t actions,
                           const void *argb, int width, int height, int hotX, int hotY,
                           WSSelectionSendHandler handler)
{
    (void)argb;
    (void)width;
    (void)height;
    (void)hotX;
    (void)hotY;
    (void)windowID;
    (void)mimeTypes;
    (void)count;
    (void)actions;
    (void)handler;
    return 0;
}

int _windowServerDragState(uint32_t *actionOut)
{
    if (actionOut)
        *actionOut = 0;
    return 0;
}

void _windowServerDragEnd(void) {}

int _windowServerCopyDragMimeTypes(char ***mimeTypesOut, uint32_t *sourceActionsOut)
{
    *mimeTypesOut = NULL;
    if (sourceActionsOut)
        *sourceActionsOut = 0;
    return 0;
}

void _windowServerDragAccept(const char *mimeType, uint32_t actions, uint32_t preferred)
{
    (void)mimeType;
    (void)actions;
    (void)preferred;
}

uint32_t _windowServerDragAction(void)
{
    return 0;
}

void *_windowServerReadDrag(const char *mimeType, size_t *lengthOut)
{
    (void)mimeType;
    if (lengthOut)
        *lengthOut = 0;
    return NULL;
}

void _windowServerDragFinish(void) {}

// Metal and Vulkan surfaces need a compositor, a page has none: callers fall back as they do without one
void *_windowServerWaylandDisplay(void) { return NULL; }
void *_windowServerCreateSurface(void) { return NULL; }
void *_windowServerAttachSubsurface(int windowID, void *surface, int x, int y) { return NULL; }
uint32_t _windowServerExportSurface(void *surface) { return 0; }
void *_windowServerImportSubsurface(int windowID, uint32_t handle, int x, int y) { return NULL; }
void *_windowServerCreateSubsurface(int windowID, int x, int y, void **subsurfaceOut)
{
    if (subsurfaceOut)
        *subsurfaceOut = NULL;
    return NULL;
}
void _windowServerMoveSubsurface(int windowID, void *subsurface, int x, int y) {}
void _windowServerDestroySubsurface(void *surface, void *subsurface) {}
void *_windowServerCreateFramePresenter(void *surface) { return NULL; }
void *_windowServerAcquireFrame(void *presenter, int width, int height, int *pitch) { return NULL; }
int _windowServerCommitFrame(void *presenter, void *pixels, int width, int height) { return 0; }
void _windowServerCancelFrame(void *presenter, void *pixels) {}
int _windowServerPresentFrame(void *presenter, const void *pixels, int width, int height, int pitch) { return 0; }
void _windowServerDestroyFramePresenter(void *presenter) {}
