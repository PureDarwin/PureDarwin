#include "runtime.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "airjitd.h"
#include "fiber.h"
#include "texture.h"

#define PD_QUAD_CALLS 16
#define PD_QUAD_VAL 32

struct pd_quad_state {
  int on;
  uint32_t lane, pass, idx, calls;
  uint8_t prev[PD_QUAD_CALLS][4][PD_QUAD_VAL], cur[PD_QUAD_CALLS][4][PD_QUAD_VAL];
};

// everything a shader's runtime calls touch is per thread: raster bands, compute groups and tiles
// run on several cores at once
struct pd_thread_state {
  int deriv_mode, deriv_idx;
  // the neighbour recorded is to the left or above (odd pixels of a quad): the difference turns round
  int deriv_flip_x, deriv_flip_y;
  float deriv_rec[2][PD_DERIV_MAX];
  volatile int discarded;
  int dry;
  uint8_t *ib, *tgmem;
  uint32_t ib_w, ib_h, ib_stride;
  struct PDTex tile_rt[4];
  uint32_t tile_ox, tile_oy;
  struct pd_quad_state quad;
};

static pthread_key_t state_key;
static pthread_once_t state_once = PTHREAD_ONCE_INIT;

static void state_free(void *p) {
  struct pd_thread_state *ts = p;

  free(ts->ib);
  free(ts->tgmem);
  free(ts);
}

static void state_key_init(void) {
  pthread_key_create(&state_key, state_free);
}

// the key frees a thread's state when it exits, the thread-local copy makes the lookup cheap
static __thread struct pd_thread_state *state_tls;

static struct pd_thread_state *thread_state(void) {
  struct pd_thread_state *ts = state_tls;

  if (ts)
    return ts;

  pthread_once(&state_once, state_key_init);
  ts = pthread_getspecific(state_key);

  if (!ts) {
    ts = calloc(1, sizeof(*ts));
    ts->ib_w = ts->ib_h = 1, ts->ib_stride = 16;
    pthread_setspecific(state_key, ts);
  }

  state_tls = ts;
  return ts;
}

static uint32_t ib_logs;
static uint8_t ib_none[64];

// the thread's imageblock tile, allocated on first use
static uint8_t *ib_tile(struct pd_thread_state *ts) {
  if (!ts->ib)
    ts->ib = calloc(1, PD_IB_BYTES);

  return ts->ib;
}

uint8_t *pd_tg_mem(unsigned slot) {
  struct pd_thread_state *ts = thread_state();

  if (!ts->tgmem)
    ts->tgmem = calloc(PD_TG_SLOTS, PD_TG_BYTES);

  return ts->tgmem ? ts->tgmem + (size_t)(slot % PD_TG_SLOTS) * PD_TG_BYTES : 0;
}

void pd_set_dry(int dry) {
  thread_state()->dry = dry;
}

int pd_is_dry(void) {
  return thread_state()->dry;
}

#define PD_FIBER_STACK (64 * 1024)
#define PD_FIBERS 1024

// one OS thread's fibers: stacks are kept between groups
struct pd_fibers {
  void *sched, *sp[PD_FIBERS];
  uint8_t *stack[PD_FIBERS];
  uint8_t done[PD_FIBERS];
  unsigned cur, n;
  void (*fn)(void *, unsigned);
  void *arg;
  // each fiber's thread index in its group, and the lane exchange area by thread index
  uint32_t lane[PD_FIBERS];
  uint8_t present[PD_FIBERS];
  uint8_t val[PD_FIBERS][64];
};

static __thread struct pd_fibers *fibers_tls;

static void fiber_entry(void *p) {
  struct pd_fibers *f = p;
  unsigned i = f->cur;

  f->fn(f->arg, i);
  f->done[i] = 1;
  pd_fiber_switch(&f->sp[i], f->sched);
}

int pd_fibers_run(unsigned n, void (*fn)(void *, unsigned), void *arg, const uint32_t *lane) {
  struct pd_fibers *f = fibers_tls;
  unsigned i, left = n;

  if (n > PD_FIBERS)
    return -1;
  if (!f && !(f = fibers_tls = calloc(1, sizeof(*f))))
    return -1;

  f->fn = fn, f->arg = arg, f->n = n;
  for (i = 0; i < n; i++) {
    if (!f->stack[i] && !(f->stack[i] = malloc(PD_FIBER_STACK)))
      return -1;
    f->sp[i] = pd_fiber_init(f->stack[i], PD_FIBER_STACK, fiber_entry, f);
    f->done[i] = 0;
    f->lane[i] = lane && lane[i] < PD_FIBERS ? lane[i] : i;
    f->present[i] = 0;
  }

  // round robin: every pass runs each live thread to its next barrier or its end
  while (left) {
    for (i = 0; i < n; i++) {
      if (f->done[i])
        continue;
      f->cur = i;
      pd_fiber_switch(&f->sched, f->sp[i]);
      if (f->done[i])
        left--;
    }
  }

  f->n = 0;
  return 0;
}

void pd_barrier(void) {
  struct pd_fibers *f = fibers_tls;

  if (f && f->n && !f->done[f->cur])
    pd_fiber_switch(&f->sp[f->cur], f->sched);
}

int pd_in_fibers(void) {
  return fibers_tls && fibers_tls->n;
}

static unsigned lane_bytes(uint32_t kind) {
  return kind == PD_LANES_BOOL ? 1 : kind == PD_LANES_F16 || kind == PD_LANES_U16 || kind == PD_LANES_S16 ? 2 : 4;
}

static double lane_get(uint32_t kind, const uint8_t *p) {
  switch (kind) {
  case PD_LANES_S32: { int32_t v; memcpy(&v, p, 4); return v; }
  case PD_LANES_F32: { float v; memcpy(&v, p, 4); return v; }
  case PD_LANES_F16: { uint16_t v; memcpy(&v, p, 2); return half_to_float(v); }
  case PD_LANES_U16: { uint16_t v; memcpy(&v, p, 2); return v; }
  case PD_LANES_S16: { int16_t v; memcpy(&v, p, 2); return v; }
  case PD_LANES_BOOL: return p[0] != 0;
  default: { uint32_t v; memcpy(&v, p, 4); return v; }
  }
}

static void lane_put(uint32_t kind, uint8_t *p, double d) {
  switch (kind) {
  case PD_LANES_S32: { int32_t v = (int32_t)(int64_t)d; memcpy(p, &v, 4); break; }
  case PD_LANES_F32: { float v = (float)d; memcpy(p, &v, 4); break; }
  case PD_LANES_F16: { uint16_t v = float_to_half((float)d); memcpy(p, &v, 2); break; }
  case PD_LANES_U16: { uint16_t v = (uint16_t)(int64_t)d; memcpy(p, &v, 2); break; }
  case PD_LANES_S16: { int16_t v = (int16_t)(int64_t)d; memcpy(p, &v, 2); break; }
  case PD_LANES_BOOL: p[0] = d != 0; break;
  default: { uint32_t v = (uint32_t)(int64_t)d; memcpy(p, &v, 4); break; }
  }
}

// integer reductions keep their bits: and/or/xor and wrapping sums go through uint64
static uint64_t lane_bits(uint32_t kind, const uint8_t *p) {
  uint64_t v = 0;
  memcpy(&v, p, lane_bytes(kind));
  return v;
}

void pd_lanes(uint32_t op, uint32_t width, uint32_t kind, uint32_t count, const void *in, void *out, uint32_t arg) {
  struct pd_fibers *f = fibers_tls;
  unsigned eb = lane_bytes(kind), size = eb * (count ? count : 1), me, base, k, c, src;
  int isint = kind != PD_LANES_F32 && kind != PD_LANES_F16;

  if (size > 64)
    size = 64, count = 64 / eb;

  // alone: every lane is this one
  if (!f || !f->n) {
    memcpy(out, in, size);
    return;
  }

  me = f->lane[f->cur];
  memcpy(f->val[me], in, size);
  f->present[me] = 1;
  pd_barrier();

  base = me & ~(width - 1);
  switch (op) {
  case PD_LANES_INDEX:
  case PD_LANES_XOR:
  case PD_LANES_UP:
  case PD_LANES_DOWN:
  case PD_LANES_FIRST:
    src = op == PD_LANES_INDEX ? base + (arg & (width - 1)) : op == PD_LANES_XOR ? me ^ (arg & (width - 1))
          : op == PD_LANES_UP ? me - arg : op == PD_LANES_DOWN ? me + arg : base;
    if (op == PD_LANES_FIRST) {
      for (src = base; src < base + width && src < PD_FIBERS && !f->present[src]; src++) {
      }
    }
    // a lane outside the group (up/down past an edge) keeps its own value
    if (src < base || src >= base + width || src >= PD_FIBERS || !f->present[src])
      src = me;
    memcpy(out, f->val[src], size);
    break;
  default:
    for (c = 0; c < count; c++) {
      double acc = op == PD_LANES_PRODUCT || op == PD_LANES_PREFIX_EX_PRODUCT || op == PD_LANES_PREFIX_IN_PRODUCT ? 1 : 0;
      uint64_t bits = op == PD_LANES_AND ? ~0ull : 0, ibits = 0;
      int first = 1, all = 1, any = 0;
      unsigned end = op >= PD_LANES_PREFIX_EX_SUM && op <= PD_LANES_PREFIX_IN_PRODUCT ? me + 1 : base + width;

      for (k = base; k < end && k < PD_FIBERS; k++) {
        const uint8_t *p = f->val[k] + c * eb;
        double v;

        if (!f->present[k])
          continue;
        if ((op == PD_LANES_PREFIX_EX_SUM || op == PD_LANES_PREFIX_EX_PRODUCT) && k == me)
          break;
        v = lane_get(kind, p);
        switch (op) {
        case PD_LANES_SUM: case PD_LANES_PREFIX_EX_SUM: case PD_LANES_PREFIX_IN_SUM:
          acc += v, ibits += lane_bits(kind, p);
          break;
        case PD_LANES_PRODUCT: case PD_LANES_PREFIX_EX_PRODUCT: case PD_LANES_PREFIX_IN_PRODUCT:
          acc *= v;
          break;
        case PD_LANES_MIN: acc = first || v < acc ? v : acc; break;
        case PD_LANES_MAX: acc = first || v > acc ? v : acc; break;
        case PD_LANES_AND: bits &= lane_bits(kind, p); break;
        case PD_LANES_OR: bits |= lane_bits(kind, p); break;
        case PD_LANES_XOR_ALL: bits ^= lane_bits(kind, p); break;
        case PD_LANES_ALL: all &= v != 0; break;
        case PD_LANES_ANY: any |= v != 0; break;
        }
        first = 0;
      }
      if (op == PD_LANES_AND || op == PD_LANES_OR || op == PD_LANES_XOR_ALL) {
        memcpy((uint8_t *)out + c * eb, &bits, eb);
      } else if (isint && (op == PD_LANES_SUM || op == PD_LANES_PREFIX_EX_SUM || op == PD_LANES_PREFIX_IN_SUM)) {
        memcpy((uint8_t *)out + c * eb, &ibits, eb);
      } else if (op == PD_LANES_ALL || op == PD_LANES_ANY) {
        lane_put(kind, (uint8_t *)out + c * eb, op == PD_LANES_ALL ? all : any);
      } else {
        lane_put(kind, (uint8_t *)out + c * eb, acc);
      }
    }
  }

  pd_barrier();
  f->present[me] = 0;
}

void pd_deriv_flip(int x, int y) {
  struct pd_thread_state *ts = thread_state();

  ts->deriv_flip_x = x, ts->deriv_flip_y = y;
}

void pd_deriv_mode(int mode) {
  struct pd_thread_state *ts = thread_state();

  ts->deriv_mode = mode;
  ts->deriv_idx = 0;
}

float pd_deriv(int32_t kind, float v) {
  struct pd_thread_state *ts = thread_state();
  int i = ts->deriv_idx++;
  float dx, dy;

  if (i >= PD_DERIV_MAX || ts->deriv_mode == PD_DERIV_ZERO)
    return 0;

  if (ts->deriv_mode != PD_DERIV_USE) {
    ts->deriv_rec[ts->deriv_mode - 1][i] = v;
    return 0;
  }

  dx = ts->deriv_rec[0][i] - v, dy = ts->deriv_rec[1][i] - v;
  if (ts->deriv_flip_x)
    dx = -dx;
  if (ts->deriv_flip_y)
    dy = -dy;
  return kind == PD_DERIV_DX ? dx : kind == PD_DERIV_DY ? dy : fabsf(dx) + fabsf(dy);
}

void pd_discard(void) {
  thread_state()->discarded = 1;
}

void pd_discard_reset(void) {
  thread_state()->discarded = 0;
}

int pd_discarded(void) {
  return thread_state()->discarded;
}

void pd_ib_begin_group(uint32_t w, uint32_t h) {
  struct pd_thread_state *ts = thread_state();
  uint8_t *ib = ib_tile(ts);
  uint64_t used;

  ts->ib_w = w, ts->ib_h = h;
  used = (uint64_t)w * h * ts->ib_stride;
  if (ib)
    memset(ib, 0, used < PD_IB_BYTES ? used : PD_IB_BYTES);
}

// this thread's element in the group's tile
void *pd_ib_data(uint32_t coord, uint32_t size, uint32_t extra) {
  struct pd_thread_state *ts = thread_state();
  uint8_t *ib = ib_tile(ts);
  uint32_t x = coord & 0xffff, y = coord >> 16;
  uint64_t at;

  if (!ib)
    return ib_none;

  if (size)
    ts->ib_stride = size;

  if (ib_logs < 6) {
    ib_logs++;
    logmsg("AIRJITD: imageblock_data coord %u,%u size %u extra %u tile %ux%u", x, y, size, extra, ts->ib_w, ts->ib_h);
  }

  at = ((uint64_t)y * ts->ib_w + x) * ts->ib_stride;
  return at + ts->ib_stride <= PD_IB_BYTES ? ib + at : ib;
}

// c0 is the region's origin in the tile, c1 its size, c2 where it lands in the texture
void pd_ib_write(const struct PDTex *t, const uint8_t *slice, uint32_t f1, uint32_t c0, uint32_t c1, uint32_t c2,
                 uint32_t lod, uint32_t f2, uint32_t n) {
  struct pd_thread_state *ts = thread_state();
  uint8_t *ib = ib_tile(ts);
  uint32_t ib_w = ts->ib_w, ib_h = ts->ib_h, ib_stride = ts->ib_stride;
  uint32_t ox = c0 & 0xffff, oy = c0 >> 16, x, y;
  uint32_t rw = c1 & 0xffff, rh = c1 >> 16, dx = c2 & 0xffff, dy = c2 >> 16;

  if (ib_logs < 12) {
    ib_logs++;
    logmsg("AIRJITD: imageblock write slice +%ld f1 %u c0 %u,%u c1 %u,%u c2 %u,%u lod %u f2 %u n %u tile %ux%u stride "
           "%u tex %ux%u",
           (long)(slice - ib), f1, ox, oy, rw, rh, dx, dy, lod, f2, n, ib_w, ib_h, ib_stride, t ? t->w : 0,
           t ? t->h : 0);
  }

  if (!ib || slice < ib || slice >= ib + PD_IB_BYTES || lod != 0)
    return;

  if (!rw || rw > ib_w)
    rw = ib_w;

  if (!rh || rh > ib_h)
    rh = ib_h;

  for (y = 0; y < rh && oy + y < ib_h; y++) {
    for (x = 0; x < rw && ox + x < ib_w; x++) {
      const uint8_t *e = slice + ((uint64_t)(oy + y) * ib_w + ox + x) * ib_stride;
      float v[4];

      if (e + 8 > ib + PD_IB_BYTES)
        return;

      pd_texel_load(PD_FMT_RGBA16F, e, v);
      pd_write_tex(t, dx + x, dy + y, v);
    }
  }
}

void pd_tile_begin(const struct PDTex *rt, uint32_t ox, uint32_t oy) {
  struct pd_thread_state *ts = thread_state();

  memcpy(ts->tile_rt, rt, sizeof(ts->tile_rt));
  ts->tile_ox = ox, ts->tile_oy = oy;
}

// coord is the pixel in the tile as <2 x i16>, x in the low half
void pd_ib_load(uint32_t rt, uint32_t coord, float *out) {
  struct pd_thread_state *ts = thread_state();

  if (rt >= 4) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }

  pd_read_tex(&ts->tile_rt[rt], ts->tile_ox + (coord & 0xffff), ts->tile_oy + (coord >> 16), out);
}

void pd_ib_store(uint32_t rt, uint32_t coord, const float *in) {
  struct pd_thread_state *ts = thread_state();

  if (rt >= 4 || ts->dry)
    return;

  pd_write_tex(&ts->tile_rt[rt], ts->tile_ox + (coord & 0xffff), ts->tile_oy + (coord >> 16), in);
}

void pd_quad_enable(int on) {
  thread_state()->quad.on = on;
}

// a new quad: nothing recorded, so a missing lane answers zeros
void pd_quad_reset(void) {
  struct pd_quad_state *q = &thread_state()->quad;

  memset(q->prev, 0, sizeof(q->prev));
  memset(q->cur, 0, sizeof(q->cur));
  q->calls = 0;
}

void pd_quad_lane(uint32_t lane, uint32_t pass) {
  struct pd_quad_state *q = &thread_state()->quad;

  q->lane = lane & 3;
  q->pass = pass;
  q->idx = 0;
}

void pd_quad_end_pass(void) {
  struct pd_quad_state *q = &thread_state()->quad;

  memcpy(q->prev, q->cur, sizeof(q->prev));
}

// the most shuffles a lane made in the pass just run
uint32_t pd_quad_calls(void) {
  struct pd_quad_state *q = &thread_state()->quad;
  uint32_t n = q->calls;

  q->calls = 0;
  return n;
}

void pd_quad_shuffle(void *val, uint32_t size, uint32_t mode, uint32_t arg) {
  // a compute group's quads are 4 consecutive threads (fragments use the raster's 2x2 passes)
  if (pd_in_fibers()) {
    static const uint32_t ops[] = { [PD_QUAD_INDEX] = PD_LANES_INDEX, [PD_QUAD_XOR] = PD_LANES_XOR,
                                    [PD_QUAD_UP] = PD_LANES_UP, [PD_QUAD_DOWN] = PD_LANES_DOWN };
    uint8_t tmp[64];
    pd_lanes(ops[mode], 4, PD_LANES_U16, (size + 1) / 2, val, tmp, arg);
    memcpy(val, tmp, size);
    return;
  }

  struct pd_quad_state *q = &thread_state()->quad;
  uint32_t c = q->idx++, src;
  int s;

  if (q->idx > q->calls)
    q->calls = q->idx;

  if (!q->on || size > PD_QUAD_VAL || c >= PD_QUAD_CALLS)
    return;

  memcpy(q->cur[c][q->lane], val, size);

  if (c >= q->pass)
    return;

  switch (mode) {
  case PD_QUAD_XOR:
    src = q->lane ^ (arg & 3);
    break;
  case PD_QUAD_INDEX:
    src = arg & 3;
    break;
  default:
    s = (int)q->lane + (mode == PD_QUAD_UP ? -(int)arg : (int)arg);
    if (s < 0 || s > 3)
      return;
    src = (uint32_t)s;
  }

  memcpy(val, q->prev[c][src], size);
}
