#ifndef AIRJITD_TEXTURE_H
#define AIRJITD_TEXTURE_H

#include <stdint.h>
#include <string.h>
#include "bridge.h"

// pixel formats as the kext numbers them
enum {
  PD_FMT_BGRA8 = 0,
  PD_FMT_RGBA16F = 1,
  PD_FMT_A8 = 2,
  PD_FMT_R8 = 3,
  PD_FMT_RGBA8 = 4,
  PD_FMT_RG8 = 5,
  // integer formats hold whole numbers: a load answers them as floats, a store rounds and clamps
  PD_FMT_RG16UI = 6,
  PD_FMT_R32UI = 7,
  PD_FMT_RG32UI = 8,
  PD_FMT_RGBA16UI = 9,
  PD_FMT_R16UI = 10,
  PD_FMT_RGBA8UI = 11,
  PD_FMT_R16F = 12,
  PD_FMT_RG16F = 13,
  PD_FMT_R32F = 14,
  PD_FMT_RG32F = 15,
  PD_FMT_RGBA32F = 16,
  // sRGB-encoded 8 bit: loads decode to linear, stores encode
  PD_FMT_RGBA8_SRGB = 17,
  PD_FMT_BGRA8_SRGB = 18,
  PD_FMT_RGB10A2 = 19,
  PD_FMT_BGR10A2 = 20,
  PD_FMT_R16 = 21,
  PD_FMT_RG16 = 22,
  PD_FMT_RGBA16 = 23,
  // depth and stencil: float depth, unorm16 depth, float depth then the stencil byte and 3 unused (8 bytes),
  // depth as 24 bit unorm in the low bits of a word with the stencil byte on top, stencil only
  PD_FMT_DEPTH32F = 24,
  PD_FMT_DEPTH16 = 25,
  PD_FMT_DEPTH32F_S8 = 26,
  PD_FMT_DEPTH24_S8 = 27,
  PD_FMT_STENCIL8 = 28,
  // block compressed, 4x4 texels a block: BC1/BC4 8 bytes, the others 16
  PD_FMT_BC1 = 29,
  PD_FMT_BC1_SRGB = 30,
  PD_FMT_BC2 = 31,
  PD_FMT_BC2_SRGB = 32,
  PD_FMT_BC3 = 33,
  PD_FMT_BC3_SRGB = 34,
  PD_FMT_BC4 = 35,
  PD_FMT_BC4_SNORM = 36,
  PD_FMT_BC5 = 37,
  PD_FMT_BC5_SNORM = 38,
  PD_FMT_BC6H_SF = 39,
  PD_FMT_BC6H_UF = 40,
  PD_FMT_BC7 = 41,
  PD_FMT_BC7_SRGB = 42,
  PD_FMT_COUNT
};

// the MTLTextureType values a PDTexLayout names
enum {
  PD_TEXTYPE_2D = 2,
  PD_TEXTYPE_2D_ARRAY = 3,
  PD_TEXTYPE_CUBE = 5,
  PD_TEXTYPE_CUBE_ARRAY = 6,
  PD_TEXTYPE_3D = 7,
};

// what a texture or sampler pointer parameter points at inside the daemon
struct PDTex {
  const uint8_t *data;
  uint32_t w, h, bpr, fmt;
  // from the request's PDTexLayout, type 0 when it sent none: level 0 of a 2D texture
  uint32_t type, levels, slices, depth, sliceBytes;
  struct PDTexLevel level[PD_TEX_LEVELS];
};

// a bound sampler; constexpr samplers reach pd_sample as the shader's own constant instead,
// so only a sampler carrying the magic is trusted
#define PD_SAMPLER_MAGIC 0x53414d50u
struct PDSampler {
  uint32_t magic, flags;
};

// half conversions run per lane of every pixel: inline. A float narrows by truncating its mantissa
static inline float half_to_float(uint16_t h) {
  uint32_t sgn = (h >> 15) & 1, exp = (h >> 10) & 0x1f, man = h & 0x3ff, bits;
  float f;

  // normal numbers: the exponent rebiased by 127 - 15, the mantissa widened
  if (exp != 0 && exp != 31) {
    bits = (sgn << 31) | (((uint32_t)h & 0x7fff) << 13) + ((127 - 15) << 23);
    memcpy(&f, &bits, 4);
    return f;
  }

  if (exp == 0) {
    if (!man) {
      bits = sgn << 31;
    } else {
      int e = -1;
      do {
        man <<= 1;
        e++;
      } while (!(man & 0x400));
      bits = (sgn << 31) | ((127 - 15 - e) << 23) | ((man & 0x3ff) << 13);
    }
  } else if (exp == 31) {
    bits = (sgn << 31) | 0x7f800000 | (man << 13);
  } else {
    bits = (sgn << 31) | ((exp + 127 - 15) << 23) | (man << 13);
  }

  memcpy(&f, &bits, 4);
  return f;
}

static inline uint16_t float_to_half(float f) {
  uint32_t bits, sgn, exp, man;

  memcpy(&bits, &f, 4);
  sgn = (bits >> 16) & 0x8000;
  exp = (bits >> 23) & 0xff;
  man = bits & 0x7fffff;

  // a normal half: the mantissa truncated, as below
  if (exp - (127 - 15 + 1) < 30)
    return (uint16_t)(sgn | ((exp - 127 + 15) << 10) | (man >> 13));

  if (exp == 0xff)
    return (uint16_t)(sgn | 0x7c00 | (man ? 0x200 : 0));

  if ((int)exp - 127 + 15 >= 31)
    return (uint16_t)(sgn | 0x7c00);

  if ((int)exp - 127 + 15 <= 0) {
    // a half denormal counts units of 2^-24: the float's 24 bit mantissa shifted by 126 - exp
    int sh = 126 - (int)exp;

    if ((int)exp - 127 + 15 < -10)
      return (uint16_t)sgn;

    man |= 0x800000;
    return (uint16_t)(sgn | (man >> sh));
  }

  return (uint16_t)(sgn | (((exp - 127 + 15) << 10) | (man >> 13)));
}

float pd_clamp(float x, float lo, float hi);

unsigned tex_bpp(unsigned fmt);
int pd_fmt_is_int(unsigned fmt);
uint8_t *pd_texel_addr(const uint8_t *data, uint32_t bpr, unsigned fmt, uint32_t x, uint32_t y);
void pd_texel_load(unsigned fmt, const uint8_t *px, float out[4]);
void pd_texel_store(unsigned fmt, uint8_t *px, const float in[4]);
void pd_tex_bind(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc);
// with the levels and slices a layout describes (NULL or unset: level 0 only)
void pd_tex_bind_layout(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc, const struct PDTexLayout *l);

// depth and stencil attachment access by pixel
float pd_depth_load(unsigned fmt, const uint8_t *px);
void pd_depth_store(unsigned fmt, uint8_t *px, float d);
uint8_t pd_stencil_load(unsigned fmt, const uint8_t *px);
void pd_stencil_store(unsigned fmt, uint8_t *px, uint8_t s);
int pd_fmt_has_stencil(unsigned fmt);
int pd_fmt_is_depth(unsigned fmt);
// block compressed (tex_bpp is then bytes per 4x4 block), _cpu: one the CPU path can decode (BC1-BC5)
int pd_fmt_is_compressed(unsigned fmt);
int pd_fmt_cpu_decodes(unsigned fmt);
void pd_bc_texel(unsigned fmt, const uint8_t *data, uint32_t bpr, uint32_t x, uint32_t y, float out[4]);

// called from shaders
void pd_sample(const struct PDTex *t, const struct PDSampler *smp, float u, float v, float *out);
// with a texel offset and the shader's lod argument (no mip levels here: logged and left unapplied)
void pd_sample_ex(const struct PDTex *t, const struct PDSampler *smp, float u, float v, int32_t ox, int32_t oy,
                  uint32_t lod_given, float lod, float *out);

// any texture type: c the coordinate (2 or 3 lanes, a cube's direction), slice an array's index, o the
// texel offset, mode how the level is picked with lod its argument, g the gradients dPdx then dPdy
enum {
  PD_LOD_IMPLICIT = 0,
  PD_LOD_LEVEL = 1,
  PD_LOD_BIAS = 2,
  PD_LOD_GRAD = 3,
  // or'ed in by 3D samples: without a layout the texture is a lookup table that answers the coordinate
  PD_LOD_3D_COMPAT = 0x100,
};
void pd_sample_full(const struct PDTex *t, const struct PDSampler *smp, const float *c, uint32_t slice, const int32_t *o,
                    uint32_t mode, float lod, const float *g, float *out);
// fragments with mipmapped textures run the derivative passes: implicit lods take them then
void pd_lod_active(int on);
// texel access at a level and slice (a 3D texture's z), and sizes at a level
void pd_read_tex_l(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, float *out);
void pd_write_tex_l(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, const float *in);
void pd_read_tex_lu(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, uint32_t *out);
void pd_write_tex_lu(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, const uint32_t *in);
// what: 0 width, 1 height, 2 depth, 3 levels, 4 array length
uint32_t pd_tex_query(const struct PDTex *t, uint32_t what, uint32_t lod);
void pd_read_tex(const struct PDTex *t, uint32_t x, uint32_t y, float *out);
void pd_write_tex(const struct PDTex *t, uint32_t x, uint32_t y, const float *in);
// integer textures by their integer values (a float in between loses bits above 2^24)
void pd_read_tex_u(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t *out);
void pd_write_tex_u(const struct PDTex *t, uint32_t x, uint32_t y, const uint32_t *in);
uint32_t pd_tex_width(const struct PDTex *t);
uint32_t pd_tex_height(const struct PDTex *t);

#endif
