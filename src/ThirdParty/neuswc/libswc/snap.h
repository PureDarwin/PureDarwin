#ifndef SWC_SNAP_H
#define SWC_SNAP_H

struct wl_display;
struct wl_global;

struct wl_global *
snap_manager_create(struct wl_display *display);

#endif
