#ifndef SWC_BACKGROUND_EFFECT_H
#define SWC_BACKGROUND_EFFECT_H

struct wl_display;
struct wl_global;

// ext-background-effect-v1: clients mark the part of a surface whose background is blurred
struct wl_global *
background_effect_create(struct wl_display *display);

#endif
