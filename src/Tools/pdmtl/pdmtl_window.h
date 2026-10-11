// pdmtl windows: GLFW plus a swapchain on the executor's device. With the Vulkan executor on the
// executor draws straight into the swapchain image, otherwise (or PDMTL_PRESENT=copy) frames are copied
#ifndef PDMTL_WINDOW_H
#define PDMTL_WINDOW_H

#include "pdmtl.h"

struct pdmtl_window;

// like CAMetalLayer.nextDrawable: texture is BGRA8Unorm at the window's framebuffer size
struct pdmtl_drawable {
  struct pdmtl_tex texture;
  struct pdmtl_window *win;
  uint32_t index;
};

enum {
  PDMTL_EV_NONE,
  PDMTL_EV_CLOSE,
  PDMTL_EV_RESIZE,
  PDMTL_EV_KEY,
  PDMTL_EV_MOUSE_MOVE,
  PDMTL_EV_MOUSE_BUTTON,
};

struct pdmtl_event {
  int type;
  // keys: GLFW key codes, action 1 press, 0 release, 2 repeat. buttons: GLFW button numbers
  int key, button, action;
  double x, y;
  int w, h;
};

struct pdmtl_window *pdmtl_window_create(int w, int h, const char *title);
void pdmtl_window_destroy(struct pdmtl_window *win);

// a swapchain on a VkSurfaceKHR someone else made, like a CAMetalLayer on a view: the executor has to
// have been started with the surface's instance extensions and VK_KHR_swapchain (pd_vk_want_extensions)
typedef int (*pdmtl_frame_present)(void *context, const void *pixels, int width, int height, int pitch);
typedef void *(*pdmtl_frame_acquire)(void *context, int width, int height, int *pitch);
typedef int (*pdmtl_frame_commit)(void *context, void *pixels, int width, int height);
typedef void (*pdmtl_frame_cancel)(void *context, void *pixels);
struct pdmtl_window *pdmtl_frame_attach(void *context, pdmtl_frame_present present, int width, int height);
struct pdmtl_window *pdmtl_frame_attach_buffers(void *context, pdmtl_frame_present present,
    pdmtl_frame_acquire acquire, pdmtl_frame_commit commit, pdmtl_frame_cancel cancel, int width, int height);
struct pdmtl_window *pdmtl_glfw_attach(void *window);

struct pdmtl_window *pdmtl_surface_attach(uint64_t surface, int width, int height);

// 1 zero-copy, 0 copy present
int pdmtl_window_zero_copy(const struct pdmtl_window *win);

// 1: an sRGB swapchain (BGRA8Unorm_sRGB drawables, format 18), 0: BGRA8Unorm (the default), takes effect
// with the next drawable
void pdmtl_window_set_srgb(struct pdmtl_window *win, int srgb);

// waits for a swapchain image, 0 when the window is gone
struct pdmtl_drawable *pdmtl_next_drawable(struct pdmtl_window *win);

// the MTLLoadActionClear action for the drawable: fills it with one colour before the first draw
void pdmtl_clear(struct pdmtl_drawable *d, float r, float g, float b, float a);

// queues the drawable for display (FIFO, so this waits for vsync), 0 or -1
int pdmtl_present(struct pdmtl_drawable *d);

// one pending event, 0 when there are none
int pdmtl_poll_event(struct pdmtl_window *win, struct pdmtl_event *ev);

#endif
