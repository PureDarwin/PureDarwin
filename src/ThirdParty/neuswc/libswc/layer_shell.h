#ifndef SWC_LAYER_SHELL_H
#define SWC_LAYER_SHELL_H

struct wl_display;
struct wl_global;

struct wl_global *
layer_shell_create(struct wl_display *display);

#endif
