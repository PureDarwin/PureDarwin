#ifndef AIRJITD_RUNTIME_H
#define AIRJITD_RUNTIME_H

#include <stdint.h>

#include "texture.h"

// derivatives: the fragment runs at (x+1, y) and (x, y+1) first to record each call's argument,
// then at (x, y) a call returns the difference, valid while the quad takes one path
enum {
  PD_DERIV_USE = 0,
  PD_DERIV_RECORD_X = 1,
  PD_DERIV_RECORD_Y = 2,
  PD_DERIV_ZERO = 3,
};

enum {
  PD_DERIV_DX = 0,
  PD_DERIV_DY = 1,
  PD_DERIV_FWIDTH = 2,
};

#define PD_DERIV_MAX 256

void pd_deriv_mode(int mode);
float pd_deriv(int32_t kind, float v);

void pd_discard(void);
void pd_discard_reset(void);
int pd_discarded(void);

// imageblocks: one tile per threadgroup, threads-per-group sized, cleared per group
#define PD_IB_BYTES (1u << 20)

void pd_ib_begin_group(uint32_t w, uint32_t h);
void *pd_ib_data(uint32_t coord, uint32_t size, uint32_t extra);
void pd_ib_write(const struct PDTex *t, const uint8_t *slice, uint32_t f1, uint32_t c0, uint32_t c1, uint32_t c2,
                 uint32_t lod, uint32_t f2, uint32_t n);

#endif
