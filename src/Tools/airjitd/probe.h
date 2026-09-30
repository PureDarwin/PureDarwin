#ifndef AIRJITD_PROBE_H
#define AIRJITD_PROBE_H

#include <stdint.h>

#include "bridge.h"
#include "texture.h"

// pixel watch: who writes these points of an 800x600 scanout (top edge line, text row)
#define PD_WATCH_W 800
#define PD_WATCH_H 600
#define PD_WATCH_MAX_LOGS 4000

// row trace: along PD_ROW_Y from PD_ROW_X0, every other pixel, what each scanout draw writes
#define PD_ROW_Y 482
#define PD_ROW_X0 290
#define PD_ROW_N 115
#define PD_ROW_MAX_LOGS 3000

// derivative A/B: small derivative-using targets also run with derivatives zeroed,
// both logged as luminance grids to judge which looks like a shape
#define PD_AB_GRID 16
#define PD_AB_MAX_TARGET 128
#define PD_AB_MAX_DRAWS 40

// sampled texture summaries, per boot
#define PD_TEX_MAX_LOGS 40000

// what one draw did, logged when it ends
struct pd_probe {
  const struct PDDrawRequest *r;
  uint32_t serial;
  unsigned covered, lit;
  float best, bestcol[4];
  int bx, by, ab;
  // a raster worker's copy: counts pixels without logging the first one
  int quiet;
  // bounds and colour sum of what the draw wrote, for the kext's display-buffer trace
  int x0, y0, x1, y1;
  float sum[4];
  // the draw's bound textures, summarised on the scan line
  const struct PDTex *texs;
};

void pd_probe_begin(struct pd_probe *p, const struct PDDrawRequest *r, uint32_t serial, int uses_deriv);
void pd_probe_triangle(struct pd_probe *p, uint32_t tri, const float *sx, const float *sy, const float vpos[3][4],
                       const float *var0, unsigned nvar, float area);
void pd_probe_pixel(struct pd_probe *p, int x, int y, const float *in, const float *col);
void pd_probe_ab(struct pd_probe *p, int x, int y, const float *real, const float *zero);
void pd_probe_worker(struct pd_probe *w, const struct pd_probe *p);
void pd_probe_merge(struct pd_probe *p, const struct pd_probe *w);
void pd_probe_end(struct pd_probe *p);
void pd_probe_textures(const struct PDDrawRequest *r, const struct PDTex *texs, uint32_t serial);

#endif
