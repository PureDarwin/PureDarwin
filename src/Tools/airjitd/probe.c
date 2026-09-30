#include "probe.h"

#include <stdio.h>
#include <string.h>

#include "airjitd.h"
#include "texture.h"

static const int watch_pts[][2] = { { 375, 482 }, { 425, 482 }, { 400, 470 } };
static unsigned watch_logs, row_logs;
static char row_buf[PD_ROW_N + 1];
static int row_hit, ab_draws;
static float ab_real[PD_AB_GRID][PD_AB_GRID], ab_zero[PD_AB_GRID][PD_AB_GRID];

static int is_scanout(const struct PDDrawRequest *r) {
  return r->tw == PD_WATCH_W && r->th == PD_WATCH_H;
}

static float luminance(const float *c) {
  return (c[0] + c[1] + c[2]) / 3;
}

static char ramp_char(const char *ramp, int steps, float v) {
  int k = (int)(v * (steps + 0.99f));

  return ramp[k < 0 ? 0 : k > steps ? steps : k];
}

void pd_probe_begin(struct pd_probe *p, const struct PDDrawRequest *r, uint32_t serial, int uses_deriv) {
  memset(p, 0, sizeof(*p));
  p->r = r;
  p->serial = serial;
  p->best = -1;
  p->bx = p->by = -1;
  p->x0 = p->y0 = 1 << 30;
  p->x1 = p->y1 = -1;

  memset(row_buf, '.', PD_ROW_N);
  row_buf[PD_ROW_N] = 0;
  row_hit = 0;

  p->ab = uses_deriv && r->tw <= PD_AB_MAX_TARGET && r->th <= PD_AB_MAX_TARGET && ab_draws < PD_AB_MAX_DRAWS;

  if (p->ab) {
    memset(ab_real, 0, sizeof(ab_real));
    memset(ab_zero, 0, sizeof(ab_zero));
    ab_draws++;
  }
}

void pd_probe_triangle(struct pd_probe *p, uint32_t tri, const float *sx, const float *sy, const float vpos[3][4],
                       const float *var0, unsigned nvar, float area) {
  if (p->r->count > 12)
    return;

  logv("AIRJITD:   tri %u: (%.1f,%.1f) (%.1f,%.1f) (%.1f,%.1f) clip w %.3f %.3f %.3f var0 %.3f %.3f %.3f %.3f "
         "area %.1f",
         tri / 3, sx[0], sy[0], sx[1], sy[1], sx[2], sy[2], vpos[0][3], vpos[1][3], vpos[2][3],
         nvar > 0 ? var0[0] : 0, nvar > 1 ? var0[1] : 0, nvar > 2 ? var0[2] : 0, nvar > 3 ? var0[3] : 0, area);
}

static void probe_row(int x, int y, const float *col) {
  static const char ramp[] = " -:=+*#%@";
  float l = luminance(col);

  if (y != PD_ROW_Y || x < PD_ROW_X0 || x >= PD_ROW_X0 + 2 * PD_ROW_N || ((x - PD_ROW_X0) & 1))
    return;

  row_buf[(x - PD_ROW_X0) / 2] = col[3] < 0.02f && l < 0.02f ? '_' : ramp_char(ramp, 8, l);
  row_hit = 1;
}

static void probe_watch(const struct pd_probe *p, int x, int y, const float *in, const float *col) {
  const struct PDDrawRequest *r = p->r;
  unsigned i;

  for (i = 0; i < PD_ARRAY_LEN(watch_pts); i++) {
    if (x != watch_pts[i][0] || y != watch_pts[i][1])
      continue;

    watch_logs++;
    logv("AIRJITD:   watch (%d,%d) draw %u stages %u/%u count %u prim %u in %.3f %.3f %.3f %.3f -> "
           "%.3f %.3f %.3f %.3f",
           x, y, p->serial, r->vfn, r->ffn, r->count, r->prim, in[4], in[5], in[6], in[7], col[0], col[1], col[2],
           col[3]);
  }
}

void pd_probe_pixel(struct pd_probe *p, int x, int y, const float *in, const float *col) {
  float sum = col[0] + col[1] + col[2];

  if (p->covered++ == 0 && !p->quiet) {
    logv("AIRJITD:   first pixel (%d,%d) in %.3f %.3f %.3f %.3f -> %.3f %.3f %.3f %.3f", x, y, in[4], in[5], in[6],
           in[7], col[0], col[1], col[2], col[3]);
  }

  if (sum > 0.004f)
    p->lit++;

  if (x < p->x0)
    p->x0 = x;

  if (y < p->y0)
    p->y0 = y;

  if (x > p->x1)
    p->x1 = x;

  if (y > p->y1)
    p->y1 = y;

  p->sum[0] += col[0], p->sum[1] += col[1], p->sum[2] += col[2], p->sum[3] += col[3];

  if (is_scanout(p->r))
    probe_row(x, y, col);

  if (is_scanout(p->r) && watch_logs < PD_WATCH_MAX_LOGS)
    probe_watch(p, x, y, in, col);

  if (sum > p->best) {
    p->best = sum;
    memcpy(p->bestcol, col, sizeof(p->bestcol));
    p->bx = x, p->by = y;
  }
}

void pd_probe_worker(struct pd_probe *w, const struct pd_probe *p) {
  *w = *p;
  w->covered = w->lit = 0;
  w->best = -1;
  w->quiet = p->covered > 0;
  w->x0 = w->y0 = 1 << 30;
  w->x1 = w->y1 = -1;
  memset(w->sum, 0, sizeof(w->sum));
}

void pd_probe_merge(struct pd_probe *p, const struct pd_probe *w) {
  p->covered += w->covered, p->lit += w->lit;
  p->x0 = w->x0 < p->x0 ? w->x0 : p->x0, p->y0 = w->y0 < p->y0 ? w->y0 : p->y0;
  p->x1 = w->x1 > p->x1 ? w->x1 : p->x1, p->y1 = w->y1 > p->y1 ? w->y1 : p->y1;
  p->sum[0] += w->sum[0], p->sum[1] += w->sum[1], p->sum[2] += w->sum[2], p->sum[3] += w->sum[3];

  if (w->best > p->best) {
    p->best = w->best;
    memcpy(p->bestcol, w->bestcol, sizeof(p->bestcol));
    p->bx = w->bx, p->by = w->by;
  }
}

void pd_probe_ab(struct pd_probe *p, int x, int y, const float *real, const float *zero) {
  unsigned gx = (unsigned)x * PD_AB_GRID / p->r->tw, gy = (unsigned)y * PD_AB_GRID / p->r->th;
  float l1 = luminance(real), l0 = luminance(zero);

  if (gx >= PD_AB_GRID || gy >= PD_AB_GRID)
    return;

  if (l1 > ab_real[gy][gx])
    ab_real[gy][gx] = l1;

  if (l0 > ab_zero[gy][gx])
    ab_zero[gy][gx] = l0;
}

static void ab_print(const char *what, float g[PD_AB_GRID][PD_AB_GRID]) {
  static const char ramp[] = " .:-=+*#%@";
  char line[PD_AB_GRID + 1];
  int x, y;

  logv("AIRJITD:   %s:", what);

  for (y = 0; y < PD_AB_GRID; y++) {
    for (x = 0; x < PD_AB_GRID; x++) {
      line[x] = ramp_char(ramp, 9, g[y][x]);
    }

    line[PD_AB_GRID] = 0;
    logv("AIRJITD:   |%s|", line);
  }
}

void pd_probe_end(struct pd_probe *p) {
  const struct PDDrawRequest *r = p->r;
  float n = p->covered ? (float)p->covered : 1;

  // the kext marks draws into display buffers (pad1): always one line, verbose or not
  if (r->pad1) {
    char src[320] = "";
    unsigned t, n_src = 0, len = 0;

    for (t = 0; t < PD_DRAW_TEXS && n_src < 4; t++) {
      if (!r->ftex[t].id && !r->ftex[t].w)
        continue;

      len += snprintf(src + len, sizeof(src) - len, " %u:%u %ux%u", t, r->ftex[t].id, r->ftex[t].w, r->ftex[t].h);

      if (p->texs && p->texs[t].data && p->texs[t].w && p->texs[t].h && len < sizeof(src)) {
        const struct PDTex *x = &p->texs[t];
        float m[4] = { 0 }, px[4];
        unsigned gx, gy;

        for (gy = 0; gy < 8; gy++) {
          for (gx = 0; gx < 8; gx++) {
            pd_read_tex(x, (gx * 2 + 1) * x->w / 16, (gy * 2 + 1) * x->h / 16, px);
            m[0] += px[0], m[1] += px[1], m[2] += px[2], m[3] += px[3];
          }
        }

        len += snprintf(src + len, sizeof(src) - len, " (%.2f %.2f %.2f %.2f)", m[0] / 64, m[1] / 64, m[2] / 64,
                        m[3] / 64);
      }
      n_src++;
    }

    logmsg("AIRJITD: scan draw %u stages %u/%u bbox %d,%d-%d,%d covered %u mean %.2f %.2f %.2f %.2f src%s",
           p->serial, r->vfn, r->ffn, p->x0, p->y0, p->x1, p->y1, p->covered, p->sum[0] / n, p->sum[1] / n,
           p->sum[2] / n, p->sum[3] / n, n_src ? src : " none");

    // small icons as an alpha map: whether the image itself is right
    for (t = 0; p->texs && t < PD_DRAW_TEXS; t++) {
      static unsigned maps;
      const struct PDTex *x = &p->texs[t];
      static const char ramp[] = " .:-=+*#%@";
      unsigned yy, xx;

      if (!x->data || x->w < 8 || x->h < 8 || x->w * x->h > 1024 || maps >= 30)
        continue;

      maps++;
      logmsg("AIRJITD:   draw %u tex %u %ux%u alpha:", p->serial, r->ftex[t].id, x->w, x->h);
      for (yy = 0; yy < x->h; yy++) {
        char row[40];

        for (xx = 0; xx < x->w && xx < 32; xx++) {
          float px[4];

          pd_read_tex(x, xx, yy, px);
          row[xx] = ramp[(int)(pd_clamp(px[3], 0, 1) * 9 + 0.5f)];
        }
        row[xx] = 0;
        logmsg("AIRJITD:   |%s|", row);
      }
    }
  }

  if (!pd_log_verbose)
    return;

  logv("AIRJITD:   covered %u px, %u non-black; brightest (%d,%d) %.3f %.3f %.3f %.3f", p->covered, p->lit, p->bx,
         p->by, p->bestcol[0], p->bestcol[1], p->bestcol[2], p->bestcol[3]);

  if (p->ab) {
    ab_print("derivatives real", ab_real);
    ab_print("derivatives zero", ab_zero);
  }

  if (row_hit && row_logs < PD_ROW_MAX_LOGS) {
    row_logs++;
    logv("AIRJITD:   row draw %u stages %u/%u count %u |%s|", p->serial, r->vfn, r->ffn, r->count, row_buf);
  }
}

// each texture a draw samples: id, size and a 16x16 grid summary, to follow which layer feeds which
void pd_probe_textures(const struct PDDrawRequest *r, const struct PDTex *texs, uint32_t serial) {
  static unsigned logs;
  unsigned t;

  for (t = 0; t < PD_DRAW_TEXS && logs < PD_TEX_MAX_LOGS; t++) {
    const struct PDTex *x = &texs[t];
    float sum[4] = { 0 }, px[4];
    unsigned opaque = 0, gx, gy;

    if (!x->data || !x->w || !x->h)
      continue;

    for (gy = 0; gy < 16; gy++) {
      for (gx = 0; gx < 16; gx++) {
        pd_read_tex(x, (gx * 2 + 1) * x->w / 32, (gy * 2 + 1) * x->h / 32, px);
        sum[0] += px[0], sum[1] += px[1], sum[2] += px[2], sum[3] += px[3];

        if (px[3] > 0.02f)
          opaque++;
      }
    }

    logs++;
    logv("AIRJITD:   draw %u tex slot %u id %u %ux%u fmt %u mean %.2f %.2f %.2f %.2f opaque %u/256", serial, t,
           r->ftex[t].id, x->w, x->h, x->fmt, sum[0] / 256, sum[1] / 256, sum[2] / 256, sum[3] / 256, opaque);
  }
}
