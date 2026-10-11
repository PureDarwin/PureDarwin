#ifndef SWC_LAYER_SHELL_H
#define SWC_LAYER_SHELL_H

struct wl_display;
struct wl_global;

struct wl_global *
layer_shell_create(struct wl_display *display);

struct screen;
// the screen changed size, anchored surfaces move and get a configure for their new size
void
layer_shell_screen_resized(struct screen *screen);

#endif
