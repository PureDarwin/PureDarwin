#include "runtime.h"

#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "airjitd.h"

// derivative and discard state is per thread: raster workers shade pixels in parallel
struct pd_thread_state {
  int deriv_mode, deriv_idx;
  float deriv_rec[2][PD_DERIV_MAX];
  volatile int discarded;
};

static pthread_key_t state_key;
static pthread_once_t state_once = PTHREAD_ONCE_INIT;

static void state_key_init(void) {
  pthread_key_create(&state_key, free);
}

static struct pd_thread_state *thread_state(void) {
  struct pd_thread_state *ts;

  pthread_once(&state_once, state_key_init);
  ts = pthread_getspecific(state_key);

  if (!ts) {
    ts = calloc(1, sizeof(*ts));
    pthread_setspecific(state_key, ts);
  }

  return ts;
}

static uint8_t ib[PD_IB_BYTES];
static uint32_t ib_w = 1, ib_h = 1, ib_stride = 16, ib_logs;

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
  uint64_t used;

  ib_w = w, ib_h = h;
  used = (uint64_t)ib_w * ib_h * ib_stride;
  memset(ib, 0, used < PD_IB_BYTES ? used : PD_IB_BYTES);
}

// this thread's element in the group's tile
void *pd_ib_data(uint32_t coord, uint32_t size, uint32_t extra) {
  uint32_t x = coord & 0xffff, y = coord >> 16;
  uint64_t at;

  if (size)
    ib_stride = size;

  if (ib_logs < 6) {
    ib_logs++;
    logmsg("AIRJITD: imageblock_data coord %u,%u size %u extra %u tile %ux%u", x, y, size, extra, ib_w, ib_h);
  }

  at = ((uint64_t)y * ib_w + x) * ib_stride;
  return at + ib_stride <= PD_IB_BYTES ? ib + at : ib;
}

// c0 is the region's origin in the tile, c1 its size, c2 where it lands in the texture
void pd_ib_write(const struct PDTex *t, const uint8_t *slice, uint32_t f1, uint32_t c0, uint32_t c1, uint32_t c2,
                 uint32_t lod, uint32_t f2, uint32_t n) {
  uint32_t ox = c0 & 0xffff, oy = c0 >> 16, x, y;
  uint32_t rw = c1 & 0xffff, rh = c1 >> 16, dx = c2 & 0xffff, dy = c2 >> 16;

  if (ib_logs < 12) {
    ib_logs++;
    logmsg("AIRJITD: imageblock write slice +%ld f1 %u c0 %u,%u c1 %u,%u c2 %u,%u lod %u f2 %u n %u tile %ux%u stride "
           "%u tex %ux%u",
           (long)(slice - ib), f1, ox, oy, rw, rh, dx, dy, lod, f2, n, ib_w, ib_h, ib_stride, t ? t->w : 0,
           t ? t->h : 0);
  }

  if (slice < ib || slice >= ib + PD_IB_BYTES || lod != 0)
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
