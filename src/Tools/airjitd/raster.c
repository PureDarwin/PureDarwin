#include "raster.h"

#include <dispatch/dispatch.h>
#include <math.h>
#include <string.h>

#include "airjitd.h"
#include "blend.h"
#include "probe.h"
#include "runtime.h"
#include "stage.h"
#include "texture.h"

#define PD_MAX_FLOATS (PD_MAX_RESULTS * 16)
// a triangle this many pixels or bigger shades in up to PD_RASTER_BANDS bands of rows
#define PD_RASTER_MIN_PIXELS 4096
#define PD_RASTER_BANDS 16

struct pd_draw {
  struct PDDrawRequest *r;
  uint8_t *base, *target;
  uint32_t bsize;
  const struct StageInfo *vs, *fs;
  struct PDTex texs[PD_DRAW_TEXS];
  struct PDSampler samp[PD_DRAW_TEXS];
  uint8_t args[PD_ARG_BYTES], out[PD_OUT_BYTES];
  float vpos[3][4], vvar[3][PD_MAX_VARYINGS];
  unsigned nvar;
  // the first triangle's clip positions, logged when a traced draw covers nothing
  float first[3][4];
  uint32_t first_vid[3];
  int have_first;
  struct pd_probe probe;
};

// a triangle in target pixels: x, y, depth, 1/w per vertex
struct pd_tri {
  float sx[3], sy[3], sz[3], sw[3];
  float area;
  // positions snapped to 1/256 pixel, so a shared edge's two triangles get exactly negated edge values
  int64_t fx[3], fy[3], farea;
};

#define PD_SUBPIXEL 256

static float min3(const float *v) {
  return v[0] < v[1] ? (v[0] < v[2] ? v[0] : v[2]) : (v[1] < v[2] ? v[1] : v[2]);
}

static float max3(const float *v) {
  return v[0] > v[1] ? (v[0] > v[2] ? v[0] : v[2]) : (v[1] > v[2] ? v[1] : v[2]);
}

static uint8_t *arg_slot(struct pd_draw *d, unsigned i) {
  return d->args + i * PD_ARG_SLOT;
}

static void put_ptr(uint8_t *slot, const void *p) {
  uint64_t v = (uint64_t)(uintptr_t)p;

  memcpy(slot, &v, 8);
}

// one float lane of a slot, as half or float
static void put_lane(uint8_t *slot, unsigned c, int half, float v) {
  if (half) {
    uint16_t h = float_to_half(v);
    memcpy(slot + c * 2, &h, 2);
  } else {
    memcpy(slot + c * 4, &v, 4);
  }
}

static const void *bound_buffer(struct pd_draw *d, const struct PDDrawBuffer *bufs, unsigned loc) {
  return loc < PD_DRAW_BUFS && bufs[loc].size ? d->base + bufs[loc].off : 0;
}

// 0 with the vertex id, 1 at a strip restart
static int fetch_vertex_id(struct pd_draw *d, uint32_t i, uint32_t *vid) {
  struct PDDrawRequest *r = d->r;

  if (r->indexed) {
    uint16_t ix;

    if (r->indexOff + i * 2 + 2 > d->bsize)
      return PD_ERR_OUT_OF_RANGE;

    memcpy(&ix, d->base + r->indexOff + i * 2, 2);
    i = ix;
  }

  if (r->prim == PD_PRIM_TRIANGLE_STRIP && r->indexed && i == 0xffff)
    return 1;

  *vid = i;
  return 0;
}

// vertex params: air.buffer(location N) is bound buffer N of this stage, vertex_id the index
static int shade_vertex(struct pd_draw *d, uint32_t vid, uint32_t iid, unsigned t) {
  const struct StageInfo *vs = d->vs;
  float vals[PD_MAX_FLOATS];
  unsigned n, i;

  pd_job_blame(d->r->vfn);

  memset(d->args, 0, sizeof(d->args));

  for (i = 0; i < vs->nparams && i < PD_MAX_PARAMS; i++) {
    const void *p = 0;

    if (vs->pkind[i] == PK_VERTEX_ID) {
      memcpy(arg_slot(d, i), &vid, 4);
      continue;
    }

    if (vs->pkind[i] == PK_INSTANCE_ID) {
      memcpy(arg_slot(d, i), &iid, 4);
      continue;
    }

    if (vs->pkind[i] == PK_BUFFER) {
      p = bound_buffer(d, d->r->vbuf, vs->ploc[i]);

      if (!p) {
        logmsg("AIRJITD: vertex buffer %u not bound", vs->ploc[i]);
        return PD_ERR_BUFFER;
      }
    }

    put_ptr(arg_slot(d, i), p);
  }

  pd_stage_run(vs, d->args, d->out);
  n = unpack_result(vs, d->out, vals);

  if (n < 4)
    return PD_ERR_SHORT_VERTEX;

  memcpy(d->vpos[t], vals, 16);
  d->nvar = n - 4 > PD_MAX_VARYINGS ? PD_MAX_VARYINGS : n - 4;
  memcpy(d->vvar[t], vals + 4, d->nvar * 4);
  return 0;
}

// clip space to the viewport, Metal's y points down
static void project(struct pd_draw *d, struct pd_tri *tri) {
  const double *vp = d->r->viewport;
  unsigned t;

  for (t = 0; t < 3; t++) {
    float w = d->vpos[t][3] != 0 ? d->vpos[t][3] : 1;
    tri->sx[t] = (float)(vp[0] + (d->vpos[t][0] / w + 1) * 0.5 * vp[2]);
    tri->sy[t] = (float)(vp[1] + (1 - d->vpos[t][1] / w) * 0.5 * vp[3]);
    tri->sz[t] = d->vpos[t][2] / w;
    tri->sw[t] = 1 / w;
  }

  tri->area = (tri->sx[1] - tri->sx[0]) * (tri->sy[2] - tri->sy[0]) -
              (tri->sx[2] - tri->sx[0]) * (tri->sy[1] - tri->sy[0]);

  for (t = 0; t < 3; t++) {
    tri->fx[t] = (int64_t)llroundf(tri->sx[t] * PD_SUBPIXEL);
    tri->fy[t] = (int64_t)llroundf(tri->sy[t] * PD_SUBPIXEL);
  }
  tri->farea = (tri->fx[1] - tri->fx[0]) * (tri->fy[2] - tri->fy[0]) -
               (tri->fx[2] - tri->fx[0]) * (tri->fy[1] - tri->fy[0]);
}

static void barycentric(const struct pd_tri *t, float px, float py, float u[3]) {
  u[0] = ((t->sx[1] - px) * (t->sy[2] - py) - (t->sx[2] - px) * (t->sy[1] - py)) / t->area;
  u[1] = ((t->sx[2] - px) * (t->sy[0] - py) - (t->sx[0] - px) * (t->sy[2] - py)) / t->area;
  // its own edge function, so a pixel exactly on edge 2 gets an exact 0 for the fill rule
  u[2] = ((t->sx[0] - px) * (t->sy[1] - py) - (t->sx[1] - px) * (t->sy[0] - py)) / t->area;
}

// top-left fill rule: a pixel exactly on an edge belongs to one of the two triangles sharing it.
// Edge i runs from vertex i+1 to i+2, turned to one winding; the pair walks it in opposite directions
static void edge_owners(const struct pd_tri *t, int own[3]) {
  unsigned i;

  for (i = 0; i < 3; i++) {
    unsigned a = (i + 1) % 3, b = (i + 2) % 3;
    int64_t dx = t->fx[b] - t->fx[a], dy = t->fy[b] - t->fy[a];

    if (t->farea < 0)
      dx = -dx, dy = -dy;

    own[i] = dy > 0 || (dy == 0 && dx > 0);
  }
}

// in: position x, y, depth, 1/w, then each varying
static void interpolate(struct pd_draw *d, const struct pd_tri *t, const float u[3], float qx, float qy, float *in) {
  unsigned i;

  in[0] = qx, in[1] = qy;
  in[2] = u[0] * t->sz[0] + u[1] * t->sz[1] + u[2] * t->sz[2];
  in[3] = u[0] * t->sw[0] + u[1] * t->sw[1] + u[2] * t->sw[2];

  for (i = 0; i < d->nvar; i++) {
    in[4 + i] = u[0] * d->vvar[0][i] + u[1] * d->vvar[1][i] + u[2] * d->vvar[2][i];
  }
}

// one thread's pixel work: its own argument and result blocks and probe counters
struct pd_frag {
  struct pd_draw *d;
  uint8_t args[PD_ARG_BYTES], out[PD_OUT_BYTES];
  struct pd_probe probe;
};

static uint8_t *frag_slot(struct pd_frag *f, unsigned i) {
  return f->args + i * PD_ARG_SLOT;
}

// framebuffer fetch: the target's colour at (x, y) in the parameter's lane type
static void put_dest(struct pd_frag *f, unsigned slot, int x, int y) {
  const struct StageInfo *fs = f->d->fs;
  const struct PDDrawRequest *r = f->d->r;
  unsigned c, loc = fs->ploc[slot];
  float dst[4] = { 0 };

  if (loc == 0)
    pd_texel_load(r->tfmt, pd_texel_addr(f->d->target, r->tbpr, r->tfmt, x, y), dst);
  else if (loc < 4 && r->rt[loc].w && (uint32_t)x < r->rt[loc].w && (uint32_t)y < r->rt[loc].h)
    pd_texel_load(r->rt[loc].fmt, pd_texel_addr(f->d->base + r->rt[loc].off, r->rt[loc].bpr, r->rt[loc].fmt, x, y), dst);

  memset(frag_slot(f, slot), 0, PD_ARG_SLOT);

  for (c = 0; c < fs->pcount[slot] && c < 4; c++) {
    put_lane(frag_slot(f, slot), c, fs->pwidth[slot] == 2, dst[c]);
  }
}

// fragment params by kind: stage-in ones take the position float4, then each varying as the
// vertex stage typed it; buffers, textures and samplers wherever they sit (a stage may have no position)
static int bind_fragment(struct pd_frag *f, const float *in, int x, int y) {
  struct pd_draw *d = f->d;
  const struct StageInfo *vs = d->vs, *fs = d->fs;
  unsigned slot, elem = 1, k = 4, c;
  int have_pos = 0;

  memset(f->args, 0, sizeof(f->args));

  for (slot = 0; slot < fs->nparams && slot < PD_MAX_PARAMS; slot++) {
    unsigned loc = fs->ploc[slot];
    const void *p = 0;

    switch (fs->pkind[slot]) {
    case PK_STAGE_IN:
      if (!have_pos) {
        memcpy(frag_slot(f, slot), in, 16);
        have_pos = 1;
        continue;
      }

      while (elem < vs->nelems && vs->elemSkip[elem])
        elem++;

      if (elem < vs->nelems) {
        for (c = 0; c < vs->elemCount[elem]; c++) {
          put_lane(frag_slot(f, slot), c, vs->elemHalf[elem], in[k++]);
        }

        elem++;
      }
      continue;
    case PK_BUFFER:
      p = bound_buffer(d, d->r->fbuf, loc);
      if (!p) {
        logmsg("AIRJITD: fragment buffer %u not bound", loc);
        return PD_ERR_FRAGMENT_BUFFER;
      }
      break;
    case PK_TEXTURE:
      if (loc >= PD_DRAW_TEXS || !d->texs[loc].data) {
        logmsg("AIRJITD: texture %u not bound", loc);
        return PD_ERR_TEXTURE;
      }
      p = &d->texs[loc];
      break;
    case PK_SAMPLER:
      p = &d->samp[loc < PD_DRAW_TEXS ? loc : 0];
      break;
    case PK_DEST:
      put_dest(f, slot, x, y);
      continue;
    }

    put_ptr(frag_slot(f, slot), p);
  }

  return 0;
}

// with derivatives: right neighbour, lower neighbour, then this pixel
static int next_pass(int pass) {
  return pass == PD_DERIV_RECORD_X ? PD_DERIV_RECORD_Y : pass == PD_DERIV_RECORD_Y ? PD_DERIV_USE : -1;
}

// leaves the stage's result in f->out and this pixel's inputs in in
static int shade_fragment(struct pd_frag *f, const struct pd_tri *t, int x, int y, float *in) {
  float px = x + 0.5f, py = y + 0.5f, u[3];
  int pass, rc;

  for (pass = f->d->fs->usesDeriv ? PD_DERIV_RECORD_X : PD_DERIV_USE; pass >= 0; pass = next_pass(pass)) {
    float qx = px + (pass == PD_DERIV_RECORD_X), qy = py + (pass == PD_DERIV_RECORD_Y);

    barycentric(t, qx, qy, u);
    interpolate(f->d, t, u, qx, qy, in);

    if ((rc = bind_fragment(f, in, x, y)))
      return rc;

    pd_discard_reset();
    pd_deriv_mode(pass);
    pd_stage_run(f->d->fs, f->args, f->out);
    pd_deriv_mode(PD_DERIV_USE);
  }

  return 0;
}

// the derivative A/B probe: the same pixel again with derivatives zeroed
static void shade_ab(struct pd_frag *f, int x, int y) {
  float real[PD_MAX_FLOATS] = { 0 }, zero[PD_MAX_FLOATS] = { 0 };
  uint8_t save[PD_OUT_BYTES];

  unpack_result(f->d->fs, f->out, real);
  memcpy(save, f->out, sizeof(save));
  pd_deriv_mode(PD_DERIV_ZERO);
  pd_stage_run(f->d->fs, f->args, f->out);
  pd_deriv_mode(PD_DERIV_USE);
  pd_discard_reset();
  unpack_result(f->d->fs, f->out, zero);
  memcpy(f->out, save, sizeof(save));
  pd_probe_ab(&f->probe, x, y, real, zero);
}

// one output lane group padded like Metal does: green and blue 0, alpha 1
static void pad_output(float *col, unsigned n) {
  unsigned k;

  for (k = n; k < 4; k++) {
    col[k] = k == 3 ? 1 : 0;
  }

  // NaN (an edge's 0/0) is written as 0 rather than spreading through later blends
  for (k = 0; k < 4; k++) {
    if (col[k] != col[k])
      col[k] = 0;
  }
}

// outputs for attachments 1-3 are stored as they are, no blending
static void write_attachments(struct pd_frag *f, int x, int y, const float *all) {
  const struct StageInfo *fs = f->d->fs;
  const struct PDDrawRequest *r = f->d->r;
  unsigned i, at = 0;

  for (i = 0; i < fs->nelems; i++) {
    unsigned rt = fs->elemRT[i], n = fs->elemCount[i] < 4 ? fs->elemCount[i] : 4;

    if (rt > 0 && rt < 4 && r->rt[rt].w && (uint32_t)x < r->rt[rt].w && (uint32_t)y < r->rt[rt].h) {
      uint8_t *px = pd_texel_addr(f->d->base + r->rt[rt].off, r->rt[rt].bpr, r->rt[rt].fmt, x, y);
      float col[4];

      memcpy(col, all + at, n * sizeof(float));
      pad_output(col, n);

      // coverage and layers accumulate through their own blend states
      if (r->rtBlend[rt] && pd_blend_needs_dest(r->rtBlend[rt], col)) {
        float cur[4];

        pd_texel_load(r->rt[rt].fmt, px, cur);
        pd_blend_pixel(r->rtBlend[rt], col, cur);
      }

      pd_texel_store(r->rt[rt].fmt, px, col);
    }

    at += fs->elemCount[i];
  }
}

static int write_fragment(struct pd_frag *f, int x, int y, const float *in) {
  struct PDDrawRequest *r = f->d->r;
  const struct StageInfo *fs = f->d->fs;
  uint8_t *dst = pd_texel_addr(f->d->target, r->tbpr, r->tfmt, x, y);
  float all[PD_MAX_FLOATS], col[4];
  unsigned n = unpack_result(fs, f->out, all), i, at = 0, cn = 0;
  int has0 = 0;

  if (n == 0)
    return PD_ERR_SHORT_FRAGMENT;

  write_attachments(f, x, y, all);

  // attachment 0 takes the output marked for it; a stage writing only others leaves it alone
  for (i = 0; i < fs->nelems; i++) {
    if (fs->elemRT[i] == 0) {
      cn = fs->elemCount[i] < 4 ? fs->elemCount[i] : 4;
      memcpy(col, all + at, cn * sizeof(float));
      has0 = 1;
      break;
    }

    at += fs->elemCount[i];
  }

  if (!has0)
    return 0;

  pad_output(col, cn);

  pd_probe_pixel(&f->probe, x, y, in, col);

  // a shader that read the destination blended itself
  if (!f->d->fs->readsDest && pd_blend_needs_dest(r->blend, col)) {
    float cur[4];
    pd_texel_load(r->tfmt, dst, cur);
    pd_blend_pixel(r->blend, col, cur);
  }

  pd_texel_store(r->tfmt, dst, col);
  return 0;
}

// rows [y0, y1) of the triangle's bounding box
static int raster_rows(struct pd_frag *f, const struct pd_tri *t, int minx, int maxx, int y0, int y1) {
  int x, y, rc, own[3];

  edge_owners(t, own);

  for (y = y0; y < y1; y++) {
    for (x = minx; x < maxx; x++) {
      float u[3], in[4 + PD_MAX_VARYINGS];
      int64_t px = (int64_t)x * PD_SUBPIXEL + PD_SUBPIXEL / 2, py = (int64_t)y * PD_SUBPIXEL + PD_SUBPIXEL / 2;
      int64_t w[3], s = t->farea > 0 ? 1 : -1;
      unsigned i;

      // edge i runs from vertex i+1 to i+2, w[i] / farea is the barycentric weight of vertex i
      w[0] = (t->fx[2] - t->fx[1]) * (py - t->fy[1]) - (t->fy[2] - t->fy[1]) * (px - t->fx[1]);
      w[1] = (t->fx[0] - t->fx[2]) * (py - t->fy[2]) - (t->fy[0] - t->fy[2]) * (px - t->fx[2]);
      w[2] = (t->fx[1] - t->fx[0]) * (py - t->fy[0]) - (t->fy[1] - t->fy[0]) * (px - t->fx[0]);

      if (w[0] * s < 0 || w[1] * s < 0 || w[2] * s < 0)
        continue;

      if ((w[0] == 0 && !own[0]) || (w[1] == 0 && !own[1]) || (w[2] == 0 && !own[2]))
        continue;

      for (i = 0; i < 3; i++)
        u[i] = (float)((double)w[i] / (double)t->farea);

      if ((rc = shade_fragment(f, t, x, y, in)))
        return rc;

      if (f->probe.ab && !pd_discarded())
        shade_ab(f, x, y);

      if (pd_discarded())
        continue;

      if ((rc = write_fragment(f, x, y, in)))
        return rc;
    }
  }

  return 0;
}

// a band of rows for one worker: rows never overlap, so bands need no locking
struct pd_band {
  struct pd_frag frag;
  const struct pd_tri *t;
  int minx, maxx, y0, y1, rc, sig;
};

static struct pd_band bands[PD_RASTER_BANDS];

static void run_band(void *arg) {
  struct pd_band *b = arg;

  b->rc = raster_rows(&b->frag, b->t, b->minx, b->maxx, b->y0, b->y1);
}

static void apply_band(void *ctx, size_t i) {
  struct pd_band *b = (struct pd_band *)ctx + i;

  b->sig = pd_guarded(run_band, b);
  if (b->sig)
    b->rc = PD_ERR_FAULT;
}

// small triangles shade on this thread, big ones in bands across the cores
static int raster_bands(struct pd_draw *d, const struct pd_tri *t, int minx, int maxx, int miny, int maxy) {
  int rows = maxy - miny, n = 1, rc = 0, i;

  if ((int64_t)rows * (maxx - minx) >= PD_RASTER_MIN_PIXELS)
    n = rows < PD_RASTER_BANDS ? rows : PD_RASTER_BANDS;

  for (i = 0; i < n; i++) {
    struct pd_band *b = &bands[i];

    b->frag.d = d;
    pd_probe_worker(&b->frag.probe, &d->probe);
    b->frag.probe.quiet |= i > 0;
    b->t = t;
    b->minx = minx, b->maxx = maxx;
    b->y0 = miny + rows * i / n, b->y1 = miny + rows * (i + 1) / n;
    b->rc = b->sig = 0;
  }

  if (n == 1)
    run_band(&bands[0]);
  else
    dispatch_apply_f((size_t)n, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), bands, apply_band);

  for (i = 0; i < n; i++) {
    pd_probe_merge(&d->probe, &bands[i].frag.probe);

    if (bands[i].sig)
      pd_job_abort(bands[i].sig, "fault on a raster worker");

    if (!rc)
      rc = bands[i].rc;
  }

  return rc;
}

static int raster_triangle(struct pd_draw *d, uint32_t index) {
  struct pd_tri t;
  int minx, miny, maxx, maxy;

  project(d, &t);
  pd_probe_triangle(&d->probe, index, t.sx, t.sy, (const float(*)[4])d->vpos, d->vvar[0], d->nvar, t.area);

  if (t.area == 0 || t.farea == 0)
    return 0;

  minx = (int)min3(t.sx), maxx = (int)max3(t.sx) + 1;
  miny = (int)min3(t.sy), maxy = (int)max3(t.sy) + 1;

  if (minx < 0)
    minx = 0;

  if (miny < 0)
    miny = 0;

  if (maxx > (int)d->r->tw)
    maxx = d->r->tw;

  if (maxy > (int)d->r->th)
    maxy = d->r->th;

  if (maxx <= minx || maxy <= miny)
    return 0;

  return raster_bands(d, &t, minx, maxx, miny, maxy);
}

static int check_draw(struct pd_draw *d) {
  struct PDDrawRequest *r = d->r;

  if (r->prim != PD_PRIM_TRIANGLES && r->prim != PD_PRIM_TRIANGLE_STRIP)
    return PD_ERR_BAD_REQUEST;

  if (r->count < 3 || r->targetOff >= d->bsize || (uint64_t)r->tbpr * r->th > d->bsize - r->targetOff)
    return PD_ERR_BAD_REQUEST;

  if (!d->vs->entry || !d->fs->entry)
    return PD_ERR_NOT_COMPILED;

  return 0;
}

int pd_run_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial) {
  static struct pd_draw d;
  uint32_t tri, inst, step = r->prim == PD_PRIM_TRIANGLE_STRIP ? 1 : 3;
  uint32_t instances = r->instances ? r->instances : 1;
  unsigned t;
  int rc;

  uint64_t t0 = pd_now_us(), t1, t2;

  memset(&d, 0, sizeof(d));
  d.r = r, d.base = base, d.bsize = bsize;
  d.vs = pd_stage(r->vfn), d.fs = pd_stage(r->ffn);
  d.target = base + r->targetOff;

  for (t = 0; t < PD_DRAW_TEXS; t++) {
    pd_tex_bind(&d.texs[t], base, &r->ftex[t]);
    d.samp[t].magic = PD_SAMPLER_MAGIC;
    d.samp[t].flags = r->samp[t];
  }

  if ((rc = check_draw(&d)))
    return rc;

  pd_probe_begin(&d.probe, r, serial, d.fs->usesDeriv);
  t1 = pd_now_us();
  if (pd_log_verbose)
    pd_probe_textures(r, d.texs, serial);
  t2 = pd_now_us();

  for (inst = 0; inst < instances; inst++) {
    for (tri = 0; tri + 3 <= r->count; tri += step) {
      int restart = 0;

      for (t = 0; t < 3; t++) {
        uint32_t vid;

        if ((rc = fetch_vertex_id(&d, tri + t, &vid)) < 0)
          return rc;

        if (rc > 0) {
          restart = 1;
          break;
        }

        if (!d.have_first)
          d.first_vid[t] = vid;

        if ((rc = shade_vertex(&d, vid, inst, t)))
          return rc;
      }

      if (restart)
        continue;

      if (!d.have_first) {
        memcpy(d.first, d.vpos, sizeof(d.first));
        d.have_first = 1;
      }

      pd_job_blame(r->ffn);

      if ((rc = raster_triangle(&d, tri)))
        return rc;
    }
  }

  d.probe.texs = d.texs;
  pd_probe_end(&d.probe);

  if (r->pad1 && !d.probe.covered)
    logmsg("AIRJITD: scan draw %u empty: prim %u count %u%s instances %u vp %.0f,%.0f %.0fx%.0f z %.2f-%.2f, vids %u %u "
           "%u first (%.3g %.3g %.3g %.3g) (%.3g %.3g %.3g %.3g) (%.3g %.3g %.3g %.3g)",
           serial, r->prim, r->count, r->indexed ? " indexed" : "", r->instances, r->viewport[0], r->viewport[1],
           r->viewport[2], r->viewport[3], r->viewport[4], r->viewport[5], d.first_vid[0], d.first_vid[1],
           d.first_vid[2], d.first[0][0], d.first[0][1],
           d.first[0][2], d.first[0][3], d.first[1][0], d.first[1][1], d.first[1][2], d.first[1][3], d.first[2][0],
           d.first[2][1], d.first[2][2], d.first[2][3]);
  logv("AIRJITD:   draw %u time stage %llu tex %llu raster %llu us, %u px covered", serial,
         (unsigned long long)(t1 - t0), (unsigned long long)(t2 - t1), (unsigned long long)(pd_now_us() - t2),
         d.probe.covered);
  return 0;
}
