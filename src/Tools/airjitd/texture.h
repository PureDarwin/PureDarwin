#ifndef AIRJITD_TEXTURE_H
#define AIRJITD_TEXTURE_H

#include <stdint.h>
#include "bridge.h"

// pixel formats as the kext numbers them
enum {
  PD_FMT_BGRA8 = 0,
  PD_FMT_RGBA16F = 1,
  PD_FMT_A8 = 2,
  PD_FMT_R8 = 3,
  PD_FMT_RGBA8 = 4,
  PD_FMT_RG8 = 5,
};

// what a texture or sampler pointer parameter points at inside the daemon
struct PDTex {
  const uint8_t *data;
  uint32_t w, h, bpr, fmt;
};

// a bound sampler; constexpr samplers reach pd_sample as the shader's own constant instead,
// so only a sampler carrying the magic is trusted
#define PD_SAMPLER_MAGIC 0x53414d50u
struct PDSampler {
  uint32_t magic, flags;
};

float half_to_float(uint16_t h);
uint16_t float_to_half(float f);
float pd_clamp(float x, float lo, float hi);

unsigned tex_bpp(unsigned fmt);
uint8_t *pd_texel_addr(const uint8_t *data, uint32_t bpr, unsigned fmt, uint32_t x, uint32_t y);
void pd_texel_load(unsigned fmt, const uint8_t *px, float out[4]);
void pd_texel_store(unsigned fmt, uint8_t *px, const float in[4]);
void pd_tex_bind(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc);

// called from shaders
void pd_sample(const struct PDTex *t, const struct PDSampler *smp, float u, float v, float *out);
void pd_read_tex(const struct PDTex *t, uint32_t x, uint32_t y, float *out);
void pd_write_tex(const struct PDTex *t, uint32_t x, uint32_t y, const float *in);
uint32_t pd_tex_width(const struct PDTex *t);
uint32_t pd_tex_height(const struct PDTex *t);

#endif
