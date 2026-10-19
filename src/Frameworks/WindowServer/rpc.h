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

// the clipboard through wl_data_device: the handler writes the bytes of one MIME type to fd and closes it,
// it runs on the event thread. The generation changes whenever the selection does.
typedef void (*WSSelectionSendHandler)(const char *mimeType, int fd);
int _windowServerSetSelection(const char *const *mimeTypes, int count, WSSelectionSendHandler handler);
int _windowServerSelectionIsOurs(void);
unsigned _windowServerSelectionGeneration(void);
int _windowServerCopySelectionMimeTypes(char ***mimeTypesOut);
void *_windowServerReadSelection(const char *mimeType, size_t *lengthOut);

// drag and drop between clients. The source side starts a drag on the window holding the pointer
// button and polls its state, the bytes of each MIME type go out through the handler on the event thread.
enum { WS_DRAG_NONE, WS_DRAG_ACTIVE, WS_DRAG_DROPPED, WS_DRAG_FINISHED, WS_DRAG_CANCELLED };
int _windowServerDragStart(int windowID, const char *const *mimeTypes, int count, uint32_t actions,
                           const void *argb, int width, int height, int hotX, int hotY,
                           WSSelectionSendHandler handler);
int _windowServerDragState(uint32_t *actionOut);
void _windowServerDragEnd(void);
// the destination side follows WS_EVENT_DRAG_* events and answers through these
int _windowServerCopyDragMimeTypes(char ***mimeTypesOut, uint32_t *sourceActionsOut);
void _windowServerDragAccept(const char *mimeType, uint32_t actions, uint32_t preferred);
uint32_t _windowServerDragAction(void);
void *_windowServerReadDrag(const char *mimeType, size_t *lengthOut);
void _windowServerDragFinish(void);

// when this process last saw a key, button, wheel or pointer motion, in nanoseconds of mach_absolute_time, 0 for never
uint64_t _windowServerLastInputTime(void);

#ifdef __cplusplus
}
#endif

#endif /* WINDOWSERVER_RPC_H */
