#ifndef SWC_WINDOW_POSITION_H
#define SWC_WINDOW_POSITION_H

struct wl_display;

// tells clients where their windows are, Wayland keeps that with the compositor
struct wl_global *
window_position_create(struct wl_display *display);

#endif
