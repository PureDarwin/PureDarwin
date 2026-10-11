/*
 * rpc.h - entry point AppKit calls to reach the window server.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */

#ifndef WINDOWSERVER_RPC_H
#define WINDOWSERVER_RPC_H

#include <WindowServer/message.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * reply/replyLen may be NULL for a message that expects nothing back.
 */
kern_return_t _windowServerRPC(void *data, size_t len, void *reply, int *replyLen);

/* Establishes the compositor connection and discovers display geometry before
 * AppKit constructs NSDisplay. */
int _windowServerConnect(void);

/*
 * Returns 1 when msg was filled, 0 if the connection is gone.
 */
int _windowServerReceiveMessage(PortMessage *msg);

// metal: the process's wl_display, and a desynchronised child surface of a window at (x, y) from its top left
// (wl_surface, with its wl_subsurface in *subsurfaceOut) for CAMetalLayer to present into with Vulkan. NULL without a wl_subcompositor or window.
void *_windowServerWaylandDisplay(void);
void *_windowServerCreateSubsurface(int windowID, int x, int y, void **subsurfaceOut);
void _windowServerMoveSubsurface(int windowID, void *subsurface, int x, int y);
void _windowServerDestroySubsurface(void *surface, void *subsurface);

// a release-tracked SHM presenter for a child surface. Pixels are premultiplied BGRA8,
// the presenter owns three buffers and never overwrites a busy one.
void *_windowServerCreateFramePresenter(void *surface);
void *_windowServerAcquireFrame(void *presenter, int width, int height, int *pitch);
int _windowServerCommitFrame(void *presenter, void *pixels, int width, int height);
void _windowServerCancelFrame(void *presenter, void *pixels);
int _windowServerPresentFrame(void *presenter, const void *pixels, int width, int height, int pitch);
void _windowServerDestroyFramePresenter(void *presenter);


// the pointer over this process's windows: an NSCursor name (arrowCursor, IBeamCursor, ...) or an
// xcursor name, an ARGB8888 image with its hot spot, or hidden
void _windowServerSetCursorNamed(const char *name);
void _windowServerSetCursorImage(const void *argb, int width, int height, int hotX, int hotY);
void _windowServerSetCursorHidden(int hidden);

#ifdef __cplusplus
}
#endif

#endif /* WINDOWSERVER_RPC_H */
