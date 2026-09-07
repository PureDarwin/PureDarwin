#ifndef SWC_DECOR_H
#define SWC_DECOR_H

#include <stdbool.h>
#include <pixman.h>

struct compositor_view;
struct swc_decor;
struct swc_rectangle;
struct wld_renderer;

bool
decor_initialize(void);
void
decor_finalize(void);

void
decor_view_initialize(struct compositor_view *view);
void
decor_view_finalize(struct compositor_view *view);

void
decor_repaint(struct wld_renderer *renderer,
              const struct swc_rectangle *target_geom,
              struct compositor_view *view, pixman_region32_t *damage);
void
decor_view_set(struct compositor_view *view, const struct swc_decor *decor);
void
decor_view_damage(struct compositor_view *view);

#endif
