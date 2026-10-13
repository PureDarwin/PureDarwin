#ifndef SWC_PD_DECORATION_H
#define SWC_PD_DECORATION_H

#include <stdbool.h>
#include <stdint.h>

struct button;
struct wl_display;
struct window;

// the decorator draws title strips for windows that asked for server side decorations
struct wl_global *
pd_decoration_create(struct wl_display *display);

void
pd_decoration_window_wanted(struct window *window);
void
pd_decoration_window_mapped(struct window *window);
void
pd_decoration_window_unmapped(struct window *window);
void
pd_decoration_window_changed(struct window *window);
void
pd_decoration_window_destroyed(struct window *window);

// a press on a window's strip is the compositor's: a window button or a move, never the client's
void
pd_decoration_button(struct window *window, struct button *button, int32_t x, int32_t y);

#endif
