/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/*
 * The window-server message ABI AppKit is written against
 * Do not reorder these fields without checking those initialisers.
 */

#ifndef WINDOWSERVER_MESSAGE_H
#define WINDOWSERVER_MESSAGE_H

#include <stdint.h>
#include <mach/mach.h>
#include <mach/message.h>

#define WINDOWSERVER_SVC_NAME "org.puredarwin.WindowServer"

enum {
    kWSWindowCreate = 1,
    kWSWindowDestroy = 2,
    kWSWindowModifyState = 3,
    kWSWindowSetFrame = 4,
    kWSWindowSetTitle = 5,
    kWSWindowOrder = 6,
    kWSWindowFlush = 7,
    kWSWindowMove = 8,
    kWSWindowResize = 9,
    kCGGetLastMouseDelta = 100,
    kCGGetMouseLocation = 101,
};

enum {
    kWSResizeTop = 1,
    kWSResizeBottom = 2,
    kWSResizeLeft = 4,
    kWSResizeTopLeft = 5,
    kWSResizeBottomLeft = 6,
    kWSResizeRight = 8,
    kWSResizeTopRight = 9,
    kWSResizeBottomRight = 10,
};

enum {
    NORMAL = 0,
    MINIMIZED = 1,
    MAXIMIZED = 2,
    HIDDEN = 3,
    CLOSED = 4,
};

enum {
    MSG_ID_INLINE = 1000,
    MSG_ID_PORT = 1001,
};

enum {
    CODE_WINDOW_STATE = 1,
    CODE_EVENT = 2,
    CODE_INPUT_EVENT = 3,
    CODE_ACTIVATION_STATE = 4,
    CODE_APP_LAUNCHED = 5,
    CODE_APP_EXITED = 6,
    CODE_APP_HIDE = 7,
    CODE_APP_BECAME_ACTIVE = 8,
    CODE_APP_BECAME_INACTIVE = 9,
    CODE_MENU_FOR_APP = 10,
    CODE_ADD_STATUS_ITEM = 11,
    CODE_STATUS_ITEM_ADDED = 12,
    CODE_ITEM_CLICKED = 13,
    CODE_ADD_RECENT_ITEM = 14,
};

enum {
    CODE_X = 90,
    CODE_XX = 91,
    CODE_XXX = 92,
    CODE_XXXX = 93,
};

#define WS_TITLE_MAX 256
#define WS_BUNDLEID_MAX 256
#define WS_INLINE_MAX 16384

struct wsRPCBase {
    uint32_t code;
    uint32_t len;
};

struct wsRPCWindow {
    struct wsRPCBase base;
    int windowID;
    double x;
    double y;
    double w;
    double h;
    unsigned int style;
    int state;
    char title[WS_TITLE_MAX];
    int level;
};

struct wsRPCSimple {
    struct wsRPCBase base;
    int val1;
    int val2;
    int val3;
    int val4;
};

struct mach_event {
    int windowID;
    int code;
    double x;
    double y;
    double dx;
    double dy;
    unsigned int mods;
    unsigned short keycode;
    int repeat;
    char chars[8];
    char charsIg[8];
};

struct mach_activation_data {
    int active;
    int windowID;
};

typedef struct {
    mach_msg_header_t header;
    mach_msg_size_t msgh_descriptor_count;
    mach_msg_port_descriptor_t descriptor;
    int code;
    int len;
    int pid;
    char bundleID[WS_BUNDLEID_MAX];
    char data[WS_INLINE_MAX];
} PortMessage;

typedef PortMessage ReceiveMessage;
typedef PortMessage Message;

#endif /* WINDOWSERVER_MESSAGE_H */
