#include "raster.h"

#include <dispatch/dispatch.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "airjitd.h"
#include "blend.h"
#include "probe.h"
#include "runtime.h"
#include "stage.h"
#include "texture.h"

#define PD_MAX_FLOATS (PD_MAX_RESULTS * 16)
// a batch whose triangles' bounds add up to this many pixels or more shades in tiles across the cores
#define PD_RASTER_MIN_PIXELS 4096
#define PD_RASTER_TILE 64
#define PD_MAX_WORKERS 128
// triangles set up before they are rastered, in order
#define PD_PRIM_BATCH 4096

// where a fragment stage-in parameter comes from: a float offset into the varyings, its lanes and interpolation
struct pd_fin {
  int16_t at;
  uint8_t count, interp;
};

struct pd_draw {
  struct PDDrawRequest *r;
  // the request's extension, NULL when the sender left it out
  const struct PDDrawExt *x;
  uint8_t *base, *target;
  uint32_t bsize;
  const struct StageInfo *vs, *fs;
  struct PDTex texs[PD_DRAW_TEXS], vtexs[PD_DRAW_TEXS];
  struct PDSampler samp[PD_DRAW_TEXS], vsamp[PD_DRAW_TEXS];
  // the depth/stencil attachment, and the tests that use it
  uint8_t *zs;
  uint32_t zbpr, zfmt, zw, zh;
  int ztest, stest;
  // culling: 1 front, 2 back, counter-clockwise front faces, set while triangles of a triangle primitive
  // are set up
  unsigned cull;
  int ccw, culling;
  // pixels outside the scissor rectangle are left alone
  int sx0, sy0, sx1, sy1;
  // the fragment's derivative passes also serve implicit levels of detail
  int lod;
  uint8_t args[PD_ARG_BYTES] __attribute__((aligned(16))), out[PD_OUT_BYTES] __attribute__((aligned(16)));
  float vpos[3][4], vvar[3][PD_MAX_VARYINGS];
  unsigned nvar;
  struct pd_fin fin[PD_MAX_PARAMS];
  // fragment arguments every pixel shares (buffers, textures, samplers), and the slots set per pixel
  uint8_t ftmpl[PD_ARG_BYTES];
  uint8_t fslots[PD_MAX_PARAMS];
  unsigned nfslots;
  // varyings interpolated per pixel: the fragment's inputs and the probe's four, all when watching
  unsigned ninterp;
  // a fragment buffer the encoder left unbound fails the draw at its first pixel
  int ferr;
  unsigned ferr_loc;
  // the batch of set-up triangles and the tiles they are binned into
  unsigned nprims;
  uint32_t ntiles, tile, tiles_x, tile_x0, tile_y0;
  volatile int stop;
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
  // facing the viewer: points and lines always are
  int front;
  // depth bias for this triangle, window depth units
  float bias;
  // the vertices' w differ, so perspective inputs need the correction
  int persp;
  // positions snapped to 1/256 pixel, so a shared edge's two triangles get exactly negated edge values
  int64_t fx[3], fy[3], farea;
  // which edges own the pixels exactly on them, and the vertices' varyings
  int own[3];
  const float (*vvar)[PD_MAX_VARYINGS];
};

// a triangle waiting in the batch, with its clipped pixel bounds
struct pd_prim {
  struct pd_tri t;
  int minx, maxx, miny, maxy;
  float vvar[3][PD_MAX_VARYINGS];
};

static struct pd_prim prims[PD_PRIM_BATCH];

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

// metal leaves a buffer the shader reads but the encoder never bound undefined, here it reads zeros, so a draw
// that forgets one (Apple's OpenGL-to-Metal sample, its reflection pass) still runs
static const uint8_t unbound_zeros[65536] __attribute__((aligned(16)));

static const void *bound_buffer(struct pd_draw *d, const struct PDDrawBuffer *bufs, unsigned loc) {
  if (loc >= PD_DRAW_BUFS)
    return 0;
  return bufs[loc].size ? d->base + bufs[loc].off : unbound_zeros;
}

// 0 with the vertex id, 1 at a strip restart. The id counts from the draw's first vertex, an index adds
// its base vertex
static int fetch_vertex_id(struct pd_draw *d, uint32_t i, uint32_t *vid) {
  struct PDDrawRequest *r = d->r;
  int wide = d->x && r->indexBytes == 4;

  if (r->indexed) {
    uint32_t ix = 0;
    unsigned n = wide ? 4 : 2;

    if ((uint64_t)r->indexOff + (uint64_t)i * n + n > d->bsize)
      return PD_ERR_OUT_OF_RANGE;

    memcpy(&ix, d->base + r->indexOff + (uint64_t)i * n, n);
    if ((r->prim == PD_PRIM_TRIANGLE_STRIP || r->prim == PD_PRIM_LINE_STRIP) && ix == (wide ? 0xffffffffu : 0xffffu))
      return 1;
    i = ix + (d->x ? (uint32_t)d->x->baseVertex : 0);
  } else if (d->x) {
    i += d->x->firstVertex;
  }

  *vid = i;
  return 0;
}

// the MTLVertexFormat description: lanes, bytes per lane, kind ('u' 's' unsigned or signed integer, 'U' 'S' normalized,
// 'h' half, 'f' float, special packed ones by their own letter)
static const struct {
  uint8_t n, bytes;
  char kind;
} vfmts[56] = {
  [1] = { 2, 1, 'u' },  [2] = { 3, 1, 'u' },  [3] = { 4, 1, 'u' },  [4] = { 2, 1, 's' },  [5] = { 3, 1, 's' },
  [6] = { 4, 1, 's' },  [7] = { 2, 1, 'U' },  [8] = { 3, 1, 'U' },  [9] = { 4, 1, 'U' },  [10] = { 2, 1, 'S' },
  [11] = { 3, 1, 'S' }, [12] = { 4, 1, 'S' }, [13] = { 2, 2, 'u' }, [14] = { 3, 2, 'u' }, [15] = { 4, 2, 'u' },
  [16] = { 2, 2, 's' }, [17] = { 3, 2, 's' }, [18] = { 4, 2, 's' }, [19] = { 2, 2, 'U' }, [20] = { 3, 2, 'U' },
  [21] = { 4, 2, 'U' }, [22] = { 2, 2, 'S' }, [23] = { 3, 2, 'S' }, [24] = { 4, 2, 'S' }, [25] = { 2, 2, 'h' },
  [26] = { 3, 2, 'h' }, [27] = { 4, 2, 'h' }, [28] = { 1, 4, 'f' }, [29] = { 2, 4, 'f' }, [30] = { 3, 4, 'f' },
  [31] = { 4, 4, 'f' }, [32] = { 1, 4, 's' }, [33] = { 2, 4, 's' }, [34] = { 3, 4, 's' }, [35] = { 4, 4, 's' },
  [36] = { 1, 4, 'u' }, [37] = { 2, 4, 'u' }, [38] = { 3, 4, 'u' }, [39] = { 4, 4, 'u' }, [40] = { 4, 4, 'A' },
  [41] = { 4, 4, 'B' }, [42] = { 4, 1, 'C' }, [45] = { 1, 1, 'u' }, [46] = { 1, 1, 's' }, [47] = { 1, 1, 'U' },
  [48] = { 1, 1, 'S' }, [49] = { 1, 2, 'u' }, [50] = { 1, 2, 's' }, [51] = { 1, 2, 'U' }, [52] = { 1, 2, 'S' },
  [53] = { 1, 2, 'h' },
};

// lane k of an attribute as a float (f) and as integer bits (u)
static void vertex_lanes(unsigned fmt, const uint8_t *p, float *f, uint32_t *u) {
  unsigned k, n = vfmts[fmt].n, b = vfmts[fmt].bytes;
  char kind = vfmts[fmt].kind;

  f[0] = f[1] = f[2] = 0, f[3] = 1;
  u[0] = u[1] = u[2] = 0, u[3] = 1;
  if (kind == 'A' || kind == 'B') {
    uint32_t w;
    memcpy(&w, p, 4);
    for (k = 0; k < 4; k++) {
      uint32_t bits = k < 3 ? (w >> (10 * k)) & 0x3ff : w >> 30;
      if (kind == 'B') {
        f[k] = bits / (k < 3 ? 1023.0f : 3.0f);
      } else {
        int32_t s = k < 3 ? (int32_t)(bits << 22) >> 22 : (int32_t)(bits << 30) >> 30;
        float v = s / (k < 3 ? 511.0f : 1.0f);
        f[k] = v < -1 ? -1 : v;
      }
      u[k] = bits;
    }
    return;
  }
  if (kind == 'C') {
    f[0] = p[2] / 255.0f, f[1] = p[1] / 255.0f, f[2] = p[0] / 255.0f, f[3] = p[3] / 255.0f;
    u[0] = p[2], u[1] = p[1], u[2] = p[0], u[3] = p[3];
    return;
  }
  for (k = 0; k < n; k++) {
    const uint8_t *q = p + k * b;
    uint32_t v = 0;
    float x;

    memcpy(&v, q, b);
    switch (kind) {
    case 'h': x = half_to_float((uint16_t)v); break;
    case 'f': memcpy(&x, &v, 4); break;
    case 's':
    case 'S': {
      int32_t s = b == 1 ? (int8_t)v : b == 2 ? (int16_t)v : (int32_t)v;
      x = kind == 'S' ? s / (b == 1 ? 127.0f : 32767.0f) : (float)s;
      if (x < -1 && kind == 'S')
        x = -1;
      v = (uint32_t)s;
      break;
    }
    case 'U': x = v / (b == 1 ? 255.0f : 65535.0f); break;
    default: x = (float)v; break;
    }
    f[k] = x, u[k] = v;
  }
}

// a stage_in attribute through the draw's vertex descriptor into its argument slot
static int fetch_attribute(struct pd_draw *d, unsigned slot, uint32_t vid, uint32_t inst) {
  const struct StageInfo *vs = d->vs;
  unsigned loc = vs->ploc[slot], k, n = vs->pcount[slot] < 4 ? vs->pcount[slot] : 4;
  const struct PDVertexAttr *at;
  const struct PDVertexLayout *l;
  const struct PDDrawBuffer *b;
  uint8_t *s = arg_slot(d, slot);
  uint64_t idx, addr;
  uint32_t u[4];
  float f[4];

  memset(s, 0, PD_ARG_SLOT);
  if (!d->x || loc >= PD_VERTEX_ATTRS || !(at = &d->x->attr[loc])->format || at->format >= 56 || !vfmts[at->format].n)
    return 0;
  l = &d->x->layout[at->buffer < PD_VERTEX_ATTRS ? at->buffer : 0];
  if (at->buffer >= PD_DRAW_BUFS || !(b = &d->r->vbuf[at->buffer])->size) {
    logmsg("AIRJITD: vertex attribute %u reads unbound buffer %u", loc, at->buffer);
    return PD_ERR_BUFFER;
  }
  idx = l->step == 0 ? 0 : l->step == 2 ? d->x->baseInstance + (inst - d->x->baseInstance) / (l->rate ? l->rate : 1) : vid;
  addr = idx * l->stride + at->offset;
  if (addr + (uint64_t)vfmts[at->format].n * vfmts[at->format].bytes > b->size)
    return PD_ERR_OUT_OF_RANGE;
  vertex_lanes(at->format, d->base + b->off + addr, f, u);

  for (k = 0; k < n; k++) {
    if (vs->pint[slot])
      memcpy(s + k * vs->pwidth[slot], &u[k], vs->pwidth[slot] < 4 ? vs->pwidth[slot] : 4);
    else
      put_lane(s, k, vs->pwidth[slot] == 2, f[k]);
  }
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
    int rc;

    if (vs->pkind[i] == PK_VERTEX_ID) {
      memcpy(arg_slot(d, i), &vid, 4);
      continue;
    }

    if (vs->pkind[i] == PK_INSTANCE_ID) {
      memcpy(arg_slot(d, i), &iid, 4);
      continue;
    }

    if (vs->pkind[i] == PK_BASE_INSTANCE || vs->pkind[i] == PK_BASE_VERTEX) {
      uint32_t v = !d->x ? 0 : vs->pkind[i] == PK_BASE_INSTANCE ? d->x->baseInstance : (uint32_t)d->x->baseVertex;
      memcpy(arg_slot(d, i), &v, 4);
      continue;
    }

    if (vs->pkind[i] == PK_VERTEX_INPUT) {
      if ((rc = fetch_attribute(d, i, vid, iid)))
        return rc;
      continue;
    }

    // the vertex stage's own textures and samplers, when the request has them
    if (vs->pkind[i] == PK_TEXTURE && d->x) {
      unsigned loc = vs->ploc[i];
      put_ptr(arg_slot(d, i), loc < PD_DRAW_TEXS && d->vtexs[loc].data ? &d->vtexs[loc] : 0);
      continue;
    }

    if (vs->pkind[i] == PK_SAMPLER && d->x) {
      put_ptr(arg_slot(d, i), &d->vsamp[vs->ploc[i] < PD_DRAW_TEXS ? vs->ploc[i] : 0]);
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

  tri->persp = tri->sw[0] != tri->sw[1] || tri->sw[1] != tri->sw[2];
  tri->front = 1, tri->bias = 0;

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

// fill rule: a pixel exactly on a shared edge goes to the triangle Metal picks (checked against M1). Edge i
// runs from vertex i+1 to i+2 in one winding, so the pair walks it in opposite directions
static void edge_owners(const struct pd_tri *t, int own[3]) {
  unsigned i;

  for (i = 0; i < 3; i++) {
    unsigned a = (i + 1) % 3, b = (i + 2) % 3;
    int64_t dx = t->fx[b] - t->fx[a], dy = t->fy[b] - t->fy[a];

    if (t->farea < 0)
      dx = -dx, dy = -dy;

    own[i] = dy < 0 || (dy == 0 && dx > 0);
  }
}

// in: position x, y, depth, 1/w, then each varying
static void interpolate(struct pd_draw *d, const struct pd_tri *t, const float u[3], float qx, float qy, float *in) {
  unsigned i;

  in[0] = qx, in[1] = qy;
  in[2] = u[0] * t->sz[0] + u[1] * t->sz[1] + u[2] * t->sz[2];
  in[3] = u[0] * t->sw[0] + u[1] * t->sw[1] + u[2] * t->sw[2];

  for (i = 0; i < d->nvar && i < d->ninterp; i++) {
    in[4 + i] = u[0] * t->vvar[0][i] + u[1] * t->vvar[1][i] + u[2] * t->vvar[2][i];
  }

  // the probe logs four varyings
  for (; i < 4; i++) {
    in[4 + i] = 0;
  }
}

// one lane of a fragment input: in[] is already the screen-linear value
static float varying(const struct pd_tri *t, const float u[3], const float *in, unsigned at, unsigned interp) {
  float w0, w1, w2;

  if (interp == PD_INTERP_FLAT)
    return t->vvar[0][at];

  if (interp == PD_INTERP_LINEAR || !t->persp)
    return in[4 + at];

  w0 = u[0] * t->sw[0], w1 = u[1] * t->sw[1], w2 = u[2] * t->sw[2];
  return (w0 * t->vvar[0][at] + w1 * t->vvar[1][at] + w2 * t->vvar[2][at]) / (w0 + w1 + w2);
}

// pairs each fragment stage-in parameter with the vertex output of the same name, an unnamed
// one takes the next output in order
static void link_varyings(struct pd_draw *d) {
  const struct StageInfo *vs = d->vs, *fs = d->fs;
  int at[PD_MAX_RESULTS], used[PD_MAX_RESULTS] = { 0 };
  unsigned i, slot, off = 0, next = 0;

  // unpack_result puts the position first and the varyings after it, in their order
  for (i = 0; i < vs->nelems && i < PD_MAX_RESULTS; i++) {
    at[i] = -1;
    if (vs->elemSkip[i] || i == vs->elemPos)
      continue;
    at[i] = (int)off;
    off += vs->elemCount[i];
  }
  used[vs->elemPos < PD_MAX_RESULTS ? vs->elemPos : 0] = 1;

  for (slot = 0; slot < fs->nparams && slot < PD_MAX_PARAMS; slot++) {
    struct pd_fin *b = &d->fin[slot];
    unsigned e = vs->nelems;

    b->count = 0;
    if (fs->pkind[slot] != PK_STAGE_IN)
      continue;

    for (i = 0; i < vs->nelems && fs->pvar[slot]; i++) {
      if (at[i] >= 0 && vs->elemVar[i] == fs->pvar[slot]) {
        e = i;
        break;
      }
    }

    if (e == vs->nelems) {
      while (next < vs->nelems && (at[next] < 0 || used[next]))
        next++;
      e = next;
      if (fs->pvar[slot])
        logv("AIRJITD:   %s input %u has no output of its name in %s", fs->name, slot, vs->name);
    }

    if (e >= vs->nelems)
      continue;

    used[e] = 1;
    b->at = (int16_t)at[e];
    b->count = (uint8_t)(vs->elemCount[e] < fs->pcount[slot] ? vs->elemCount[e] : fs->pcount[slot]);
    b->interp = fs->pinterp[slot];
  }
}

// one thread's pixel work: its own argument and result blocks and probe counters
struct pd_frag {
  struct pd_draw *d;
  uint8_t args[PD_ARG_BYTES] __attribute__((aligned(16))), out[PD_OUT_BYTES] __attribute__((aligned(16)));
  struct pd_probe probe;
};

static const uint8_t unbound_texel[8];
static const struct PDTex unbound_tex = { unbound_texel, 1, 1, 8, PD_FMT_RGBA16F };

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

// fragment params by kind: positions and stage-in values per pixel, buffers, textures and samplers are the
// same for every pixel, so they live in the template
static void prepare_fragment(struct pd_draw *d) {
  const struct StageInfo *fs = d->fs;
  unsigned slot;

  memset(d->ftmpl, 0, sizeof(d->ftmpl));
  d->nfslots = 0;
  d->ferr = 0;
  d->ninterp = pd_watch_x >= 0 ? PD_MAX_VARYINGS : 4;

  for (slot = 0; slot < fs->nparams && slot < PD_MAX_PARAMS; slot++) {
    unsigned loc = fs->ploc[slot];
    const void *p = 0;

    switch (fs->pkind[slot]) {
    case PK_STAGE_IN:
      if (d->fin[slot].count && d->fin[slot].at + d->fin[slot].count > (int)d->ninterp)
        d->ninterp = (unsigned)(d->fin[slot].at + d->fin[slot].count);
      d->fslots[d->nfslots++] = (uint8_t)slot;
      continue;
    case PK_POSITION:
    case PK_DEST:
    case PK_FRONT_FACING:
      d->fslots[d->nfslots++] = (uint8_t)slot;
      continue;
    case PK_BUFFER:
      p = bound_buffer(d, d->r->fbuf, loc);
      if (!p && !d->ferr)
        d->ferr = PD_ERR_FRAGMENT_BUFFER, d->ferr_loc = loc;
      break;
    case PK_TEXTURE:
      // a slot the encoder left empty reads as zeros, as on the GPU: a shader may sample it on a path
      // this draw never takes
      p = loc < PD_DRAW_TEXS && d->texs[loc].data ? &d->texs[loc] : &unbound_tex;
      break;
    case PK_SAMPLER:
      p = &d->samp[loc < PD_DRAW_TEXS ? loc : 0];
      break;
    default:
      continue;
    }

    put_ptr(d->ftmpl + slot * PD_ARG_SLOT, p);
  }
}

// the per-pixel slots on top of the template already in f->args
static int bind_fragment(struct pd_frag *f, const struct pd_tri *t, const float u[3], const float *in, int x, int y) {
  struct pd_draw *d = f->d;
  const struct StageInfo *fs = d->fs;
  unsigned i, c;

  if (d->ferr) {
    logmsg("AIRJITD: fragment buffer %u not bound", d->ferr_loc);
    return d->ferr;
  }

  for (i = 0; i < d->nfslots; i++) {
    unsigned slot = d->fslots[i];
    const struct pd_fin *b = &d->fin[slot];

    switch (fs->pkind[slot]) {
    case PK_POSITION:
      memcpy(frag_slot(f, slot), in, 16);
      break;
    case PK_STAGE_IN:
      for (c = 0; c < b->count; c++) {
        put_lane(frag_slot(f, slot), c, fs->pwidth[slot] == 2, varying(t, u, in, b->at + c, b->interp));
      }
      break;
    case PK_DEST:
      put_dest(f, slot, x, y);
      break;
    case PK_FRONT_FACING:
      *(uint8_t *)frag_slot(f, slot) = (uint8_t)t->front;
      break;
    }
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

  // with mipmapped textures the neighbours are the pixel's quad partners, as a GPU takes them for the level
  // of detail, without, the pixels right and below as always
  int fx = f->d->lod && (x & 1), fy = f->d->lod && (y & 1);

  pd_deriv_flip(fx, fy);
  for (pass = f->d->fs->usesDeriv || f->d->lod ? PD_DERIV_RECORD_X : PD_DERIV_USE; pass >= 0; pass = next_pass(pass)) {
    float qx = px + (pass == PD_DERIV_RECORD_X ? (fx ? -1 : 1) : 0), qy = py + (pass == PD_DERIV_RECORD_Y ? (fy ? -1 : 1) : 0);

    barycentric(t, qx, qy, u);
    interpolate(f->d, t, u, qx, qy, in);

    if ((rc = bind_fragment(f, t, u, in, x, y)))
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
      if (r->rtBlend[rt] && !pd_fmt_is_int(r->rt[rt].fmt) && pd_blend_needs_dest(r->rtBlend[rt], col)) {
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

  if (x == pd_watch_x && y == pd_watch_y) {
    float cur[4];

    pd_texel_load(r->tfmt, dst, cur);
    logmsg("AIRJITD: watch %d,%d dest %g %g %g %g out %g %g %g %g reads dest %d", x, y, cur[0], cur[1], cur[2],
           cur[3], col[0], col[1], col[2], col[3], fs->readsDest);
    for (i = 0; i < f->d->nvar && i < 16; i += 4) {
      logmsg("AIRJITD: watch varyings %u: %g %g %g %g", i, in[4 + i], in[5 + i], in[6 + i], in[7 + i]);
    }
    for (i = 0; i < n && i < 16; i += 4) {
      logmsg("AIRJITD: watch result %u: %g %g %g %g", i, all[i], all[i + 1], all[i + 2], all[i + 3]);
    }
  }

  // a shader that read the destination blended itself
  if (!f->d->fs->readsDest && !pd_fmt_is_int(r->tfmt) && pd_blend_needs_dest(r->blend, col)) {
    float cur[4];
    pd_texel_load(r->tfmt, dst, cur);
    pd_blend_pixel(r->blend, col, cur);
  }

  pd_texel_store(r->tfmt, dst, col);
  return 0;
}

// the MTLCompareFunction values
static int compare(unsigned fn, float a, float b) {
  switch (fn & 7) {
  case 0: return 0;
  case 1: return a < b;
  case 2: return a == b;
  case 3: return a <= b;
  case 4: return a > b;
  case 5: return a != b;
  case 6: return a >= b;
  default: return 1;
  }
}

// the MTLStencilOperation applied to value s with reference ref
static uint8_t stencil_op(unsigned op, uint8_t s, uint8_t ref) {
  switch (op & 7) {
  case 1: return 0;
  case 2: return ref;
  case 3: return s == 255 ? 255 : s + 1;
  case 4: return s == 0 ? 0 : s - 1;
  case 5: return (uint8_t)~s;
  case 6: return (uint8_t)(s + 1);
  case 7: return (uint8_t)(s - 1);
  default: return s;
  }
}

// the window depth a fragment of t at (px, py) gets: the viewport's range over the clip z, with the bias
static float frag_depth(const struct pd_draw *d, const struct pd_tri *t, float px, float py) {
  const double *vp = d->r->viewport;
  float u[3], z;

  barycentric(t, px, py, u);
  z = u[0] * t->sz[0] + u[1] * t->sz[1] + u[2] * t->sz[2];
  return (float)(vp[4] + z * (vp[5] - vp[4])) + t->bias;
}

// the depth and stencil tests at (x, y) for depth z, with apply the stencil operations and the depth write
// happen too. Nonzero when the fragment passes
static int zs_test(struct pd_draw *d, int x, int y, int front, float z, int apply) {
  const struct PDDrawExt *e = d->x;
  uint8_t *px = d->zs + (uint64_t)y * d->zbpr + (uint64_t)x * tex_bpp(d->zfmt);
  const double *vp = d->r->viewport;
  float lo = (float)(vp[4] < vp[5] ? vp[4] : vp[5]), hi = (float)(vp[4] < vp[5] ? vp[5] : vp[4]);
  uint32_t st = e->stencil[front ? 0 : 1];
  uint8_t s = 0, ref = (uint8_t)(e->stencilRef >> (front ? 0 : 8)), rm = (uint8_t)(st >> 12), wm = (uint8_t)(st >> 20);
  int pass = 1;
  unsigned op = 0;

  z = z < lo ? lo : z > hi ? hi : z;
  if (d->stest) {
    s = pd_stencil_load(d->zfmt, px);
    if (!compare(st, (float)(ref & rm), (float)(s & rm)))
      pass = 0, op = st >> 3;
  }
  if (pass && d->ztest) {
    uint8_t q[8];
    float cur = pd_depth_load(d->zfmt, px);

    // compared at the attachment's precision
    pd_depth_store(d->zfmt, q, z);
    if (!compare(e->depthState, pd_depth_load(d->zfmt, q), cur))
      pass = 0, op = st >> 6;
    else if (apply && (e->depthState & PD_DEPTH_WRITE))
      pd_depth_store(d->zfmt, px, z);
  }
  if (pass)
    op = st >> 9;
  if (d->stest && (apply || !pass)) {
    uint8_t n = stencil_op(op, s, ref);
    pd_stencil_store(d->zfmt, px, (uint8_t)((s & ~wm) | (n & wm)));
  }
  return pass;
}

// the depth a fragment function wrote, when it writes one
static float written_depth(struct pd_frag *f, float z) {
  const struct StageInfo *fs = f->d->fs;
  float all[PD_MAX_FLOATS];
  unsigned i, at = 0;

  unpack_result(fs, f->out, all);
  for (i = 0; i < fs->nelems; i++) {
    if (fs->elemRT[i] == PD_RT_DEPTH)
      return all[at];
    at += fs->elemCount[i];
  }
  return z;
}

// pixels [x0, x1) x [y0, y1) of the triangle
static int raster_rows(struct pd_frag *f, const struct pd_tri *t, int x0, int x1, int y0, int y1) {
  int x, y, rc;
  float z = 0;
  int64_t s = t->farea > 0 ? 1 : -1;

  for (y = y0; y < y1; y++) {
    int64_t py = (int64_t)y * PD_SUBPIXEL + PD_SUBPIXEL / 2;

    for (x = x0; x < x1; x++) {
      float in[4 + PD_MAX_VARYINGS];
      int64_t px = (int64_t)x * PD_SUBPIXEL + PD_SUBPIXEL / 2;
      int64_t w[3];

      // edge i runs from vertex i+1 to i+2, w[i] / farea is the barycentric weight of vertex i
      w[0] = (t->fx[2] - t->fx[1]) * (py - t->fy[1]) - (t->fy[2] - t->fy[1]) * (px - t->fx[1]);
      w[1] = (t->fx[0] - t->fx[2]) * (py - t->fy[2]) - (t->fy[0] - t->fy[2]) * (px - t->fx[2]);
      w[2] = (t->fx[1] - t->fx[0]) * (py - t->fy[0]) - (t->fy[1] - t->fy[0]) * (px - t->fx[0]);

      if (w[0] * s < 0 || w[1] * s < 0 || w[2] * s < 0)
        continue;

      if ((w[0] == 0 && !t->own[0]) || (w[1] == 0 && !t->own[1]) || (w[2] == 0 && !t->own[2]))
        continue;

      // early tests when the fragment function leaves the depth alone, it may still discard, so the
      // writes wait for its result
      if (f->d->zs) {
        z = frag_depth(f->d, t, x + 0.5f, y + 0.5f);
        if (!f->d->fs->writesDepth && !zs_test(f->d, x, y, t->front, z, 0))
          continue;
      }

      if ((rc = shade_fragment(f, t, x, y, in)))
        return rc;

      if (f->probe.ab && !pd_discarded())
        shade_ab(f, x, y);

      if (pd_discarded())
        continue;

      if (f->d->zs && !zs_test(f->d, x, y, t->front, f->d->fs->writesDepth ? written_depth(f, z) : z, 1))
        continue;

      // a depth-only pass is done once the depth and stencil are written
      if (f->d->x && (f->d->x->raster & PD_RASTER_SET) && (f->d->x->raster & PD_RASTER_NOCOLOR))
        continue;

      if ((rc = write_fragment(f, x, y, in)))
        return rc;
    }
  }

  return 0;
}

// a raster worker: its own argument block and probe, pulling tiles until none are left
struct pd_worker {
  struct pd_frag frag;
  int rc, sig;
};

static struct pd_worker workers[PD_MAX_WORKERS];
// tile k's triangles, in draw order, are bin_list[bin_start[k] .. bin_start[k + 1])
static uint32_t *bin_start, *bin_list;
static size_t bin_start_cap, bin_list_cap;

static void worker_begin(struct pd_worker *w, struct pd_draw *d, int quiet) {
  w->frag.d = d;
  memcpy(w->frag.args, d->ftmpl, sizeof(w->frag.args));
  pd_probe_worker(&w->frag.probe, &d->probe);
  w->frag.probe.quiet |= quiet;
  w->rc = w->sig = 0;
}

// every triangle of a tile in order, so each pixel sees the draw's triangles in order
static void run_worker(void *arg) {
  struct pd_worker *w = arg;
  struct pd_draw *d = w->frag.d;

  pd_lod_active(d->lod);
  while (!d->stop) {
    uint32_t k = __atomic_fetch_add(&d->tile, 1, __ATOMIC_RELAXED), e;
    int tx0, ty0, tx1, ty1;

    if (k >= d->ntiles)
      break;

    tx0 = (int)(d->tile_x0 + k % d->tiles_x * PD_RASTER_TILE), tx1 = tx0 + PD_RASTER_TILE;
    ty0 = (int)(d->tile_y0 + k / d->tiles_x * PD_RASTER_TILE), ty1 = ty0 + PD_RASTER_TILE;

    for (e = bin_start[k]; e < bin_start[k + 1]; e++) {
      const struct pd_prim *p = &prims[bin_list[e]];
      int x0 = p->minx > tx0 ? p->minx : tx0, x1 = p->maxx < tx1 ? p->maxx : tx1;
      int y0 = p->miny > ty0 ? p->miny : ty0, y1 = p->maxy < ty1 ? p->maxy : ty1;

      if ((w->rc = raster_rows(&w->frag, &p->t, x0, x1, y0, y1))) {
        d->stop = 1;
        pd_lod_active(0);
        return;
      }
    }
  }
  pd_lod_active(0);
}

static void apply_worker(void *ctx, size_t i) {
  struct pd_worker *w = (struct pd_worker *)ctx + i;

  w->sig = pd_guarded(run_worker, w);
  if (w->sig) {
    w->rc = PD_ERR_FAULT;
    w->frag.d->stop = 1;
  }
}

static int grow(uint32_t **v, size_t *cap, size_t n) {
  uint32_t *m;

  if (n <= *cap)
    return 0;

  if (!(m = realloc(*v, n * sizeof(**v))))
    return -1;

  *v = m, *cap = n;
  return 0;
}

// the tile range a triangle's bounds touch, in tiles from the batch's origin
static void prim_tiles(const struct pd_prim *p, int x0, int y0, uint32_t c[4]) {
  c[0] = (uint32_t)(p->minx - x0) / PD_RASTER_TILE, c[1] = (uint32_t)(p->maxx - 1 - x0) / PD_RASTER_TILE;
  c[2] = (uint32_t)(p->miny - y0) / PD_RASTER_TILE, c[3] = (uint32_t)(p->maxy - 1 - y0) / PD_RASTER_TILE;
}

// bins the batch's triangles into the tiles their bounds touch, 0 tiles when it is not worth it
static uint32_t bin_prims(struct pd_draw *d) {
  int x0 = 1 << 30, y0 = 1 << 30, x1 = 0, y1 = 0;
  uint32_t i, k, tx, ty, ntiles, c[4], cx, cy;
  uint64_t pixels = 0, entries = 0;

  for (i = 0; i < d->nprims; i++) {
    const struct pd_prim *p = &prims[i];

    pixels += (uint64_t)(p->maxx - p->minx) * (uint64_t)(p->maxy - p->miny);
    x0 = p->minx < x0 ? p->minx : x0, x1 = p->maxx > x1 ? p->maxx : x1;
    y0 = p->miny < y0 ? p->miny : y0, y1 = p->maxy > y1 ? p->maxy : y1;
  }

  if (pixels < PD_RASTER_MIN_PIXELS || pd_cpus() < 2)
    return 0;

  tx = (uint32_t)(x1 - x0 + PD_RASTER_TILE - 1) / PD_RASTER_TILE;
  ty = (uint32_t)(y1 - y0 + PD_RASTER_TILE - 1) / PD_RASTER_TILE;
  ntiles = tx * ty;

  if (grow(&bin_start, &bin_start_cap, (size_t)ntiles + 1))
    return 0;

  memset(bin_start, 0, ((size_t)ntiles + 1) * sizeof(*bin_start));

  for (i = 0; i < d->nprims; i++) {
    prim_tiles(&prims[i], x0, y0, c);
    for (cy = c[2]; cy <= c[3]; cy++) {
      for (cx = c[0]; cx <= c[1]; cx++) {
        bin_start[cy * tx + cx + 1]++;
      }
    }

    entries += (uint64_t)(c[1] - c[0] + 1) * (c[3] - c[2] + 1);
  }

  if (entries > 0xffffffffu || grow(&bin_list, &bin_list_cap, (size_t)entries))
    return 0;

  for (k = 0; k < ntiles; k++) {
    bin_start[k + 1] += bin_start[k];
  }

  // each tile fills from its start, which leaves bin_start[k] at tile k + 1's start
  for (i = 0; i < d->nprims; i++) {
    prim_tiles(&prims[i], x0, y0, c);
    for (cy = c[2]; cy <= c[3]; cy++) {
      for (cx = c[0]; cx <= c[1]; cx++) {
        bin_list[bin_start[cy * tx + cx]++] = i;
      }
    }
  }

  for (k = ntiles; k > 0; k--) {
    bin_start[k] = bin_start[k - 1];
  }
  bin_start[0] = 0;

  d->tiles_x = tx, d->tile_x0 = (uint32_t)x0, d->tile_y0 = (uint32_t)y0;
  return ntiles;
}

// rasters the batch: a small one on this thread triangle by triangle, a big one tile by tile on
// every core. Either way each pixel sees the triangles in draw order
static int raster_prims(struct pd_draw *d) {
  uint32_t ntiles, i, n;
  int rc = 0;

  if (!d->nprims)
    return 0;

  pd_job_blame(d->r->ffn);
  ntiles = bin_prims(d);

  if (!ntiles) {
    struct pd_worker *w = &workers[0];

    worker_begin(w, d, 0);
    pd_lod_active(d->lod);
    for (i = 0; i < d->nprims && !rc; i++) {
      const struct pd_prim *p = &prims[i];

      rc = raster_rows(&w->frag, &p->t, p->minx, p->maxx, p->miny, p->maxy);
    }
    pd_lod_active(0);

    pd_probe_merge(&d->probe, &w->frag.probe);
    d->nprims = 0;
    return rc;
  }

  n = pd_cpus();
  n = n < ntiles ? n : ntiles;
  n = n < PD_MAX_WORKERS ? n : PD_MAX_WORKERS;
  d->ntiles = ntiles, d->tile = 0, d->stop = 0;

  for (i = 0; i < n; i++) {
    worker_begin(&workers[i], d, i > 0);
  }

  dispatch_apply_f(n, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0), workers, apply_worker);
  d->nprims = 0;

  for (i = 0; i < n; i++) {
    pd_probe_merge(&d->probe, &workers[i].frag.probe);

    if (workers[i].sig)
      pd_job_abort(workers[i].sig, "fault on a raster worker");

    if (!rc)
      rc = workers[i].rc;
  }

  return rc;
}

// projects the shaded triangle and queues it for the next batch
static int setup_triangle(struct pd_draw *d, uint32_t index) {
  struct pd_prim *p;
  struct pd_tri t;
  int minx, miny, maxx, maxy, rc;

  project(d, &t);
  pd_probe_triangle(&d->probe, index, t.sx, t.sy, (const float(*)[4])d->vpos, d->vvar[0], d->nvar, t.area);

  if (t.area == 0 || t.farea == 0)
    return 0;

  // clockwise in the window (y down) faces the viewer unless the winding says counter-clockwise
  if (d->culling) {
    t.front = (t.area > 0) != d->ccw;
    if ((d->cull == 1 && t.front) || (d->cull == 2 && !t.front))
      return 0;
  } else if (!d->x) {
    t.front = t.area > 0;
  }

  if (d->x && d->zs && (d->x->depthBias != 0 || d->x->depthSlope != 0)) {
    const double *vp = d->r->viewport;
    // the depth slope from the plane through the three window depths
    float z[3], ax, ay, b;
    unsigned k;

    for (k = 0; k < 3; k++) {
      z[k] = (float)(vp[4] + t.sz[k] * (vp[5] - vp[4]));
    }
    ax = fabsf(((z[1] - z[0]) * (t.sy[2] - t.sy[0]) - (z[2] - z[0]) * (t.sy[1] - t.sy[0])) / t.area);
    ay = fabsf(((z[2] - z[0]) * (t.sx[1] - t.sx[0]) - (z[1] - z[0]) * (t.sx[2] - t.sx[0])) / t.area);
    b = d->x->depthSlope * (ax > ay ? ax : ay) + d->x->depthBias * (d->zfmt == PD_FMT_DEPTH16 ? 1.0f / 65535 : 1.0f / 16777216);
    if (d->x->depthBiasClamp > 0 && b > d->x->depthBiasClamp)
      b = d->x->depthBiasClamp;
    if (d->x->depthBiasClamp < 0 && b < d->x->depthBiasClamp)
      b = d->x->depthBiasClamp;
    t.bias = b;
  }

  minx = (int)min3(t.sx), maxx = (int)max3(t.sx) + 1;
  miny = (int)min3(t.sy), maxy = (int)max3(t.sy) + 1;

  if (minx < d->sx0)
    minx = d->sx0;

  if (miny < d->sy0)
    miny = d->sy0;

  if (maxx > d->sx1)
    maxx = d->sx1;

  if (maxy > d->sy1)
    maxy = d->sy1;

  if (maxx <= minx || maxy <= miny)
    return 0;

  if (d->nprims == PD_PRIM_BATCH && (rc = raster_prims(d)))
    return rc;

  p = &prims[d->nprims++];
  p->t = t;
  edge_owners(&p->t, p->t.own);
  memcpy(p->vvar, d->vvar, sizeof(p->vvar));
  p->t.vvar = (const float(*)[PD_MAX_VARYINGS])p->vvar;
  p->minx = minx, p->maxx = maxx, p->miny = miny, p->maxy = maxy;
  return 0;
}

// a clip-space vertex with its varyings
struct pd_cvert {
  float pos[4], var[PD_MAX_VARYINGS];
};

// metal clips to 0 <= z <= w (unless depth clamps) and w > 0: a triangle partly outside becomes the fan of
// its clipped polygon, varyings interpolated in clip space
static int clip_triangle(struct pd_draw *d, uint32_t index) {
  struct pd_cvert in[9], out[9];
  unsigned n = 3, i, k, plane, m, nv = d->nvar;
  int clamp = d->x && (d->x->raster & PD_RASTER_CLAMP), all = 1;

  for (i = 0; i < 3; i++) {
    float z = d->vpos[i][2], w = d->vpos[i][3];
    if (!(w > 0 && (clamp || (z >= 0 && z <= w))))
      all = 0;
  }
  if (all)
    return setup_triangle(d, index);

  for (i = 0; i < 3; i++) {
    memcpy(in[i].pos, d->vpos[i], 16);
    memcpy(in[i].var, d->vvar[i], sizeof(in[i].var));
  }
  for (plane = clamp ? 2 : 0; plane < 3 && n; plane++) {
    m = 0;
    for (i = 0; i < n; i++) {
      const struct pd_cvert *a = &in[i], *b = &in[(i + 1) % n];
      // signed distances: near z, far w - z, and w itself kept above zero
      float da = plane == 0 ? a->pos[2] : plane == 1 ? a->pos[3] - a->pos[2] : a->pos[3] - 1e-6f;
      float db = plane == 0 ? b->pos[2] : plane == 1 ? b->pos[3] - b->pos[2] : b->pos[3] - 1e-6f;

      if (da >= 0 && m < 9)
        out[m++] = *a;
      if ((da >= 0) != (db >= 0) && m < 9) {
        float s = da / (da - db);
        for (k = 0; k < 4; k++) {
          out[m].pos[k] = a->pos[k] + (b->pos[k] - a->pos[k]) * s;
        }
        for (k = 0; k < nv; k++) {
          out[m].var[k] = a->var[k] + (b->var[k] - a->var[k]) * s;
        }
        m++;
      }
    }
    memcpy(in, out, m * sizeof(in[0]));
    n = m;
  }

  for (i = 1; i + 1 < n; i++) {
    unsigned v[3] = { 0, i, i + 1 };
    int rc;

    for (k = 0; k < 3; k++) {
      memcpy(d->vpos[k], in[v[k]].pos, 16);
      memcpy(d->vvar[k], in[v[k]].var, sizeof(d->vvar[k]));
    }
    if ((rc = setup_triangle(d, index)))
      return rc;
  }
  return 0;
}

// slot dst becomes slot src's vertex, its clip position moved by (dx, dy) target pixels
static void nudge_slot(struct pd_draw *d, unsigned dst, const float *pos, const float *var, float dx, float dy) {
  float w = pos[3];

  memcpy(d->vvar[dst], var, sizeof(d->vvar[dst]));
  d->vpos[dst][0] = pos[0] + dx * 2.0f / d->r->viewport[2] * w;
  d->vpos[dst][1] = pos[1] - dy * 2.0f / d->r->viewport[3] * w;
  d->vpos[dst][2] = pos[2];
  d->vpos[dst][3] = w;
}

// one pixel's square around clip position c (w in c[3]) with varyings var: the pixel whose centre is at
// target (px, py)
static int pixel_square(struct pd_draw *d, uint32_t index, const float *c, const float *var) {
  int rc;

  nudge_slot(d, 0, c, var, -0.5f, -0.5f);
  nudge_slot(d, 1, c, var, 0.5f, -0.5f);
  nudge_slot(d, 2, c, var, -0.5f, 0.5f);
  if ((rc = setup_triangle(d, index)))
    return rc;
  nudge_slot(d, 0, c, var, 0.5f, 0.5f);
  return setup_triangle(d, index);
}

// a point (slot 0) as a one-pixel square, a line (slots 0, 1) one pixel per step of its major axis, start in
// and end out: the pixels Metal draws (checked against M1)
static int setup_point_or_line(struct pd_draw *d, uint32_t index, int line) {
  const double *vp = d->r->viewport;
  float a[4], b[4], va[PD_MAX_VARYINGS], vb[PD_MAX_VARYINGS];
  float sa[2], sb[2], ddx, ddy;
  int rc, xmajor, n, k, start, end, dir;

  memcpy(a, d->vpos[0], sizeof(a));
  memcpy(va, d->vvar[0], sizeof(va));
  if (!line)
    return pixel_square(d, index, a, va);

  memcpy(b, d->vpos[1], sizeof(b));
  memcpy(vb, d->vvar[1], sizeof(vb));
  if (a[3] == 0 || b[3] == 0)
    return 0;

  sa[0] = (float)(vp[0] + (a[0] / a[3] + 1) * 0.5 * vp[2]), sa[1] = (float)(vp[1] + (1 - a[1] / a[3]) * 0.5 * vp[3]);
  sb[0] = (float)(vp[0] + (b[0] / b[3] + 1) * 0.5 * vp[2]), sb[1] = (float)(vp[1] + (1 - b[1] / b[3]) * 0.5 * vp[3]);
  ddx = sb[0] - sa[0], ddy = sb[1] - sa[1];
  xmajor = fabsf(ddx) >= fabsf(ddy);
  if ((xmajor ? ddx : ddy) == 0)
    return 0;

  // pixel centres c + 0.5 along the major axis from the start up to the end (exclusive)
  {
    float s0 = xmajor ? sa[0] : sa[1], s1 = xmajor ? sb[0] : sb[1];
    dir = s1 > s0 ? 1 : -1;
    start = dir > 0 ? (int)ceilf(s0 - 0.5f) : (int)floorf(s0 - 0.5f);
    end = dir > 0 ? (int)ceilf(s1 - 0.5f) : (int)floorf(s1 - 0.5f);
    n = (end - start) * dir;
  }

  // diamond exit: a pixel counts when the line leaves its diamond |x - cx| + |y - cy| < 1/2, the start's
  // diamond counts, the end's does not
  for (k = -1; k < n; k++) {
    int m = start + k * dir;
    float centre = m + 0.5f, tt = (centre - (xmajor ? sa[0] : sa[1])) / (xmajor ? ddx : ddy), minor, c[4], var[PD_MAX_VARYINGS];
    float px, py;
    unsigned i;

    minor = (xmajor ? sa[1] : sa[0]) + tt * (xmajor ? ddy : ddx);
    px = xmajor ? centre : floorf(minor) + 0.5f;
    py = xmajor ? floorf(minor) + 0.5f : centre;
    if (k < 0) {
      px = floorf(sa[0]) + 0.5f, py = floorf(sa[1]) + 0.5f;
      if (fabsf(sa[0] - px) + fabsf(sa[1] - py) >= 0.5f || (xmajor ? px : py) != centre)
        continue;
    }
    if (fabsf(sb[0] - px) + fabsf(sb[1] - py) < 0.5f)
      continue;
    for (i = 0; i < 4; i++) {
      c[i] = a[i] + (b[i] - a[i]) * tt;
    }
    for (i = 0; i < PD_MAX_VARYINGS; i++) {
      var[i] = va[i] + (vb[i] - va[i]) * tt;
    }
    // the clip position of that pixel's centre at the interpolated w
    c[0] = (float)(((px - vp[0]) / vp[2] * 2 - 1) * c[3]);
    c[1] = (float)((1 - (py - vp[1]) / vp[3] * 2) * c[3]);
    if ((rc = pixel_square(d, index, c, var)))
      return rc;
  }

  return 0;
}

static int check_draw(struct pd_draw *d) {
  struct PDDrawRequest *r = d->r;

  if (r->prim > PD_PRIM_TRIANGLE_STRIP)
    return PD_ERR_BAD_REQUEST;

  if (r->count < 1 || r->targetOff >= d->bsize || (uint64_t)r->tbpr * r->th > d->bsize - r->targetOff)
    return PD_ERR_BAD_REQUEST;

  if (!d->vs->entry || !d->fs->entry)
    return PD_ERR_NOT_COMPILED;

  if (d->x && d->x->depth.w &&
      (d->x->depth.off >= d->bsize || (uint64_t)d->x->depth.bpr * d->x->depth.h > d->bsize - d->x->depth.off))
    return PD_ERR_BAD_REQUEST;

  // BC6H and BC7 have no CPU decoder: such a draw needs the Vulkan executor
  for (unsigned t = 0; t < PD_DRAW_TEXS; t++) {
    unsigned f[2] = { r->ftex[t].fmt, d->x ? d->x->vtex[t].fmt : 0 };
    unsigned w[2] = { r->ftex[t].w, d->x ? d->x->vtex[t].w : 0 };
    for (unsigned k = 0; k < 2; k++) {
      if (w[k] && pd_fmt_is_compressed(f[k]) && !pd_fmt_cpu_decodes(f[k])) {
        logmsg("AIRJITD: texture format %u has no CPU decoder", f[k]);
        return PD_ERR_TEXTURE;
      }
    }
  }

  return 0;
}

// the extension's state: textures with their layouts, depth/stencil, culling and scissor
static void apply_ext(struct pd_draw *d) {
  const struct PDDrawExt *x = d->x;
  const struct PDDrawRequest *r = d->r;
  unsigned t;

  d->sx0 = d->sy0 = 0, d->sx1 = (int)r->tw, d->sy1 = (int)r->th;
  memset(pd_blend_constant, 0, sizeof(pd_blend_constant));
  if (!x)
    return;

  for (t = 0; t < PD_DRAW_TEXS; t++) {
    pd_tex_bind_layout(&d->texs[t], d->base, &r->ftex[t], &x->ftexLayout[t]);
    pd_tex_bind_layout(&d->vtexs[t], d->base, &x->vtex[t], &x->vtexLayout[t]);
    d->vsamp[t].magic = PD_SAMPLER_MAGIC;
    d->vsamp[t].flags = x->vsamp[t];
    if (d->fs->usesLod && d->texs[t].levels > 1)
      d->lod = 1;
  }
  memcpy(pd_blend_constant, x->blendColor, sizeof(pd_blend_constant));

  if (x->depth.w && (pd_fmt_is_depth(x->depth.fmt) || x->depth.fmt == PD_FMT_STENCIL8)) {
    d->zs = d->base + x->depth.off, d->zbpr = x->depth.bpr, d->zfmt = x->depth.fmt;
    d->zw = x->depth.w, d->zh = x->depth.h;
    // metal's default depth state compares always and writes nothing
    d->ztest = pd_fmt_is_depth(d->zfmt) && (x->depthState & PD_DEPTH_SET) &&
               ((x->depthState & 7) != 7 || (x->depthState & PD_DEPTH_WRITE));
    d->stest = pd_fmt_has_stencil(d->zfmt) && ((x->stencil[0] | x->stencil[1]) & PD_STENCIL_ON);
    if (!d->ztest && !d->stest)
      d->zs = 0;
    if (d->zs) {
      if (d->sx1 > (int)d->zw)
        d->sx1 = (int)d->zw;
      if (d->sy1 > (int)d->zh)
        d->sy1 = (int)d->zh;
    }
  }

  if (x->raster & PD_RASTER_SET) {
    d->cull = x->raster & 3, d->ccw = (x->raster & PD_RASTER_CCW) != 0;
    if (x->raster & PD_RASTER_SCISSOR) {
      int x0 = (int)x->scissor[0], y0 = (int)x->scissor[1];
      int x1 = (int)(x->scissor[0] + x->scissor[2]), y1 = (int)(x->scissor[1] + x->scissor[3]);
      d->sx0 = x0 > d->sx0 ? x0 : d->sx0, d->sy0 = y0 > d->sy0 ? y0 : d->sy0;
      d->sx1 = x1 < d->sx1 ? x1 : d->sx1, d->sy1 = y1 < d->sy1 ? y1 : d->sy1;
    }
  }
}

// shades every primitive's vertices and queues its triangles, rastering whenever the batch fills
static int setup_draw(struct pd_draw *d) {
  struct PDDrawRequest *r = d->r;
  // vertices per primitive and the step to the next one
  uint32_t nv = r->prim == PD_PRIM_POINTS ? 1 : r->prim <= PD_PRIM_LINE_STRIP ? 2 : 3;
  uint32_t step = r->prim == PD_PRIM_LINES ? 2 : r->prim == PD_PRIM_TRIANGLES ? 3 : 1;
  uint32_t tri, inst;
  uint32_t instances = r->instances ? r->instances : 1, base = d->x ? d->x->baseInstance : 0;
  unsigned t;
  int rc;

  for (inst = 0; inst < instances; inst++) {
    for (tri = 0; tri + nv <= r->count; tri += step) {
      int restart = 0;

      for (t = 0; t < nv; t++) {
        uint32_t vid;

        if ((rc = fetch_vertex_id(d, tri + t, &vid)) < 0)
          return rc;

        if (rc > 0) {
          restart = 1;
          break;
        }

        if (!d->have_first)
          d->first_vid[t] = vid;

        if ((rc = shade_vertex(d, vid, base + inst, t)))
          return rc;
      }

      if (restart)
        continue;

      if (!d->have_first) {
        memcpy(d->first, d->vpos, sizeof(d->first));
        d->have_first = 1;
      }

      if (nv == 3) {
        d->culling = d->x && (d->x->raster & PD_RASTER_SET);
        rc = d->x ? clip_triangle(d, tri) : setup_triangle(d, tri);
        d->culling = 0;
      } else {
        rc = setup_point_or_line(d, tri, nv == 2);
      }
      if (rc)
        return rc;
    }
  }

  return 0;
}

int pd_run_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial) {
  static struct pd_draw d;
  unsigned t;
  int rc, frc;

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

  d.x = pd_draw_ext(r);
  if ((rc = check_draw(&d)))
    return rc;

  apply_ext(&d);
  link_varyings(&d);
  prepare_fragment(&d);

  pd_probe_begin(&d.probe, r, serial, d.fs->usesDeriv);
  t1 = pd_now_us();
  if (pd_log_verbose)
    pd_probe_textures(r, d.texs, serial);
  t2 = pd_now_us();

  rc = setup_draw(&d);
  // triangles set up before a failing vertex still draw, as they did one by one
  frc = raster_prims(&d);
  if (rc || (rc = frc))
    return rc;

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
  logv("AIRJITD:   draw %u time stage %llu tex %llu raster %llu us, %u px covered, vids %u %u %u first (%.3g %.3g %.3g %.3g) "
       "(%.3g %.3g %.3g %.3g) (%.3g %.3g %.3g %.3g)", serial,
         (unsigned long long)(t1 - t0), (unsigned long long)(t2 - t1), (unsigned long long)(pd_now_us() - t2),
         d.probe.covered, d.first_vid[0], d.first_vid[1], d.first_vid[2], d.first[0][0], d.first[0][1], d.first[0][2],
         d.first[0][3], d.first[1][0], d.first[1][1], d.first[1][2], d.first[1][3], d.first[2][0], d.first[2][1],
         d.first[2][2], d.first[2][3]);
  return 0;
}
