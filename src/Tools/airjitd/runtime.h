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
// the recorded neighbours are left of and above this pixel (quad pairs): differences change sign
void pd_deriv_flip(int x, int y);
float pd_deriv(int32_t kind, float v);

void pd_discard(void);

// a threadgroup barrier: back to the scheduler of the fiber running this thread, nothing outside one
void pd_barrier(void);

// runs fn(arg, i) for i < n as fibers on this OS thread: a barrier in one switches to the next, so all
// reach barrier k before any passes it. 0 on success
int pd_fibers_run(unsigned n, void (*fn)(void *, unsigned), void *arg, const uint32_t *lane);
void pd_discard_reset(void);
int pd_discarded(void);

// imageblocks: one tile per threadgroup, threads-per-group sized, cleared per group
#define PD_IB_BYTES (1u << 20)

void pd_ib_begin_group(uint32_t w, uint32_t h);
void *pd_ib_data(uint32_t coord, uint32_t size, uint32_t extra);
void pd_ib_write(const struct PDTex *t, const uint8_t *slice, uint32_t f1, uint32_t c0, uint32_t c1, uint32_t c2,
                 uint32_t lod, uint32_t f2, uint32_t n);

// side effects (texture and imageblock writes) are dropped while set: quad passes before the last
void pd_set_dry(int dry);
int pd_is_dry(void);

// this thread's threadgroup memory, PD_TG_SLOTS of PD_TG_BYTES
#define PD_TG_SLOTS 4
#define PD_TG_BYTES 0x10000
uint8_t *pd_tg_mem(unsigned slot);

// tile kernels: the pass's colour attachments behind the implicit imageblock, and the tile's origin
void pd_tile_begin(const struct PDTex *rt, uint32_t ox, uint32_t oy);
void pd_ib_load(uint32_t rt, uint32_t coord, float *out);
void pd_ib_store(uint32_t rt, uint32_t coord, const float *in);

// quads: the four lanes run one after another, each pass answering a shuffle with the partner's
// argument of the same call in the pass before, so pass n gets the first n shuffles right
enum {
  PD_QUAD_XOR = 0,
  PD_QUAD_INDEX = 1,
  PD_QUAD_UP = 2,
  PD_QUAD_DOWN = 3,
};

void pd_quad_enable(int on);
void pd_quad_reset(void);
void pd_quad_lane(uint32_t lane, uint32_t pass);
void pd_quad_end_pass(void);
uint32_t pd_quad_calls(void);
void pd_quad_shuffle(void *val, uint32_t size, uint32_t mode, uint32_t arg);

// lane operations across a quad (width 4) or simdgroup (width 32) of a compute group running as fibers:
// lanes are consecutive thread indices in the group. Outside fibers a thread is alone in its group
enum {
  PD_LANES_INDEX, PD_LANES_XOR, PD_LANES_UP, PD_LANES_DOWN, PD_LANES_FIRST, PD_LANES_SUM, PD_LANES_PRODUCT,
  PD_LANES_MIN, PD_LANES_MAX, PD_LANES_AND, PD_LANES_OR, PD_LANES_XOR_ALL, PD_LANES_PREFIX_EX_SUM,
  PD_LANES_PREFIX_IN_SUM, PD_LANES_PREFIX_EX_PRODUCT, PD_LANES_PREFIX_IN_PRODUCT, PD_LANES_ALL, PD_LANES_ANY,
};
enum { PD_LANES_U32, PD_LANES_S32, PD_LANES_F32, PD_LANES_F16, PD_LANES_U16, PD_LANES_S16, PD_LANES_BOOL };
void pd_lanes(uint32_t op, uint32_t width, uint32_t kind, uint32_t count, const void *in, void *out, uint32_t arg);
// whether the calling thread runs as one of a group's fibers
int pd_in_fibers(void);

#endif
