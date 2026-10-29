#include "texture.h"

#include "airjitd.h"
#include "runtime.h"

#include <math.h>
#include <string.h>

float pd_clamp(float x, float lo, float hi) {
  return x < lo ? lo : x > hi ? hi : x;
}

// v / 255.0f for every byte, folded by the compiler: a load needs no division
#define U8(i) ((float)(i) / 255.0f)
#define U8x4(i) U8(i), U8(i + 1), U8(i + 2), U8(i + 3)
#define U8x16(i) U8x4(i), U8x4(i + 4), U8x4(i + 8), U8x4(i + 12)
#define U8x64(i) U8x16(i), U8x16(i + 16), U8x16(i + 32), U8x16(i + 48)
static const float unorm8[256] = { U8x64(0), U8x64(64), U8x64(128), U8x64(192) };

static uint8_t pd_unorm8(float f) {
  return (uint8_t)(pd_clamp(f, 0, 1) * 255.0f + 0.5f);
}

// bytes a pixel takes
unsigned tex_bpp(unsigned fmt) {
  static const uint8_t bpp[PD_FMT_COUNT] = {
    [PD_FMT_BGRA8] = 4,     [PD_FMT_RGBA16F] = 8,   [PD_FMT_A8] = 1,       [PD_FMT_R8] = 1,
    [PD_FMT_RGBA8] = 4,     [PD_FMT_RG8] = 2,       [PD_FMT_RG16UI] = 4,   [PD_FMT_R32UI] = 4,
    [PD_FMT_RG32UI] = 8,    [PD_FMT_RGBA16UI] = 8,  [PD_FMT_R16UI] = 2,    [PD_FMT_RGBA8UI] = 4,
    [PD_FMT_R16F] = 2,      [PD_FMT_RG16F] = 4,     [PD_FMT_R32F] = 4,     [PD_FMT_RG32F] = 8,
    [PD_FMT_RGBA32F] = 16,  [PD_FMT_RGBA8_SRGB] = 4, [PD_FMT_BGRA8_SRGB] = 4, [PD_FMT_RGB10A2] = 4,
    [PD_FMT_BGR10A2] = 4,   [PD_FMT_R16] = 2,       [PD_FMT_RG16] = 4,     [PD_FMT_RGBA16] = 8,
    [PD_FMT_DEPTH32F] = 4,  [PD_FMT_DEPTH16] = 2,   [PD_FMT_DEPTH32F_S8] = 8, [PD_FMT_DEPTH24_S8] = 4,
    [PD_FMT_STENCIL8] = 1,
    [PD_FMT_BC1] = 8,       [PD_FMT_BC1_SRGB] = 8,  [PD_FMT_BC2] = 16,     [PD_FMT_BC2_SRGB] = 16,
    [PD_FMT_BC3] = 16,      [PD_FMT_BC3_SRGB] = 16, [PD_FMT_BC4] = 8,      [PD_FMT_BC4_SNORM] = 8,
    [PD_FMT_BC5] = 16,      [PD_FMT_BC5_SNORM] = 16, [PD_FMT_BC6H_SF] = 16, [PD_FMT_BC6H_UF] = 16,
    [PD_FMT_BC7] = 16,      [PD_FMT_BC7_SRGB] = 16,
    [PD_FMT_R8UI] = 1,      [PD_FMT_RG8UI] = 2,     [PD_FMT_RGBA32UI] = 16, [PD_FMT_R8I] = 1,
    [PD_FMT_RG8I] = 2,      [PD_FMT_RGBA8I] = 4,    [PD_FMT_R16I] = 2,     [PD_FMT_RG16I] = 4,
    [PD_FMT_RGBA16I] = 8,   [PD_FMT_R32I] = 4,      [PD_FMT_RG32I] = 8,    [PD_FMT_RGBA32I] = 16,
  };

  return fmt < PD_FMT_COUNT ? bpp[fmt] : 4;
}

uint8_t *pd_texel_addr(const uint8_t *data, uint32_t bpr, unsigned fmt, uint32_t x, uint32_t y) {
  return (uint8_t *)data + (uint64_t)y * bpr + (uint64_t)x * tex_bpp(fmt);
}

static float srgb_to_linear(float c) {
  return c <= 0.04045f ? c / 12.92f : powf((c + 0.055f) / 1.055f, 2.4f);
}

static float linear_to_srgb(float c) {
  c = pd_clamp(c, 0, 1);
  return c <= 0.0031308f ? c * 12.92f : 1.055f * powf(c, 1 / 2.4f) - 0.055f;
}

static uint32_t to_uint(float v, uint32_t max) {
  if (!(v > 0))
    return 0;

  return v >= (float)max ? max : (uint32_t)(v + 0.5f);
}

// lanes of n-byte unsigned integers (n 1, 2 or 4), scaled to [0,1] when norm is the maximum
static void load_lanes(const uint8_t *px, unsigned lanes, unsigned n, float norm, float out[4]) {
  unsigned c;

  out[0] = out[1] = out[2] = 0, out[3] = 1;
  for (c = 0; c < lanes; c++) {
    uint32_t v;

    if (n == 1) {
      v = px[c];
      if (norm == 255) {
        out[c] = unorm8[v];
        continue;
      }
    } else if (n == 2) {
      uint16_t h;
      memcpy(&h, px + c * 2, 2);
      v = h;
    } else {
      memcpy(&v, px + c * 4, 4);
    }

    out[c] = norm ? v / norm : (float)v;
  }
}

static void store_lanes(uint8_t *px, unsigned lanes, unsigned n, float norm, const float in[4]) {
  uint32_t max = n == 4 ? 0xffffffffu : (1u << (n * 8)) - 1;
  unsigned c;

  for (c = 0; c < lanes; c++) {
    uint32_t v = norm ? to_uint(pd_clamp(in[c], 0, 1) * norm, max) : to_uint(in[c], max);

    if (n == 1) {
      px[c] = (uint8_t)v;
    } else if (n == 2) {
      uint16_t h = (uint16_t)v;
      memcpy(px + c * 2, &h, 2);
    } else {
      memcpy(px + c * 4, &v, 4);
    }
  }
}

// 10-bit lanes in a 32-bit word, a 2-bit alpha on top, bgr swaps red and blue
static void load_1010102(const uint8_t *px, int bgr, float out[4]) {
  uint32_t v;
  float c[3];
  unsigned k;

  memcpy(&v, px, 4);
  for (k = 0; k < 3; k++) {
    c[k] = ((v >> (10 * k)) & 0x3ff) / 1023.0f;
  }
  out[0] = bgr ? c[2] : c[0], out[1] = c[1], out[2] = bgr ? c[0] : c[2];
  out[3] = (v >> 30) / 3.0f;
}

static void store_1010102(uint8_t *px, int bgr, const float in[4]) {
  float r = bgr ? in[2] : in[0], b = bgr ? in[0] : in[2];
  uint32_t v = to_uint(pd_clamp(r, 0, 1) * 1023, 1023) | to_uint(pd_clamp(in[1], 0, 1) * 1023, 1023) << 10 |
               to_uint(pd_clamp(b, 0, 1) * 1023, 1023) << 20 | to_uint(pd_clamp(in[3], 0, 1) * 3, 3) << 30;

  memcpy(px, &v, 4);
}

// halves or floats, n lanes
static void load_floats(const uint8_t *px, unsigned lanes, int half, float out[4]) {
  unsigned c;

  out[0] = out[1] = out[2] = 0, out[3] = 1;
  for (c = 0; c < lanes; c++) {
    if (half) {
      uint16_t h;
      memcpy(&h, px + c * 2, 2);
      out[c] = half_to_float(h);
    } else {
      memcpy(&out[c], px + c * 4, 4);
    }
  }
}

static void store_floats(uint8_t *px, unsigned lanes, int half, const float in[4]) {
  unsigned c;

  for (c = 0; c < lanes; c++) {
    if (half) {
      uint16_t h = float_to_half(in[c]);
      memcpy(px + c * 2, &h, 2);
    } else {
      memcpy(px + c * 4, &in[c], 4);
    }
  }
}

// A8 reads as (0,0,0,a), R8 as (r,0,0,1) and RG8 as (r,g,0,1)
void pd_texel_load(unsigned fmt, const uint8_t *px, float out[4]) {
  unsigned c;

  switch (fmt) {
  case PD_FMT_RGBA16F:
    load_floats(px, 4, 1, out);
    break;
  case PD_FMT_A8:
    out[0] = out[1] = out[2] = 0;
    out[3] = unorm8[px[0]];
    break;
  case PD_FMT_R8:
    load_lanes(px, 1, 1, 255, out);
    break;
  case PD_FMT_RG8:
    load_lanes(px, 2, 1, 255, out);
    break;
  case PD_FMT_RGBA8:
    load_lanes(px, 4, 1, 255, out);
    break;
  case PD_FMT_RG16UI:
    load_lanes(px, 2, 2, 0, out);
    break;
  case PD_FMT_R32UI:
    load_lanes(px, 1, 4, 0, out);
    break;
  case PD_FMT_RG32UI:
    load_lanes(px, 2, 4, 0, out);
    break;
  case PD_FMT_RGBA16UI:
    load_lanes(px, 4, 2, 0, out);
    break;
  case PD_FMT_R16UI:
    load_lanes(px, 1, 2, 0, out);
    break;
  case PD_FMT_RGBA8UI:
    load_lanes(px, 4, 1, 0, out);
    break;
  // the CPU path reads the newer integer formats' lanes unsigned, narrow signed ones are not sign extended
  case PD_FMT_R8UI: case PD_FMT_R8I:
    load_lanes(px, 1, 1, 0, out);
    break;
  case PD_FMT_RG8UI: case PD_FMT_RG8I:
    load_lanes(px, 2, 1, 0, out);
    break;
  case PD_FMT_RGBA8I:
    load_lanes(px, 4, 1, 0, out);
    break;
  case PD_FMT_R16I:
    load_lanes(px, 1, 2, 0, out);
    break;
  case PD_FMT_RG16I:
    load_lanes(px, 2, 2, 0, out);
    break;
  case PD_FMT_RGBA16I:
    load_lanes(px, 4, 2, 0, out);
    break;
  case PD_FMT_R32I:
    load_lanes(px, 1, 4, 0, out);
    break;
  case PD_FMT_RG32I:
    load_lanes(px, 2, 4, 0, out);
    break;
  case PD_FMT_RGBA32UI: case PD_FMT_RGBA32I:
    load_lanes(px, 4, 4, 0, out);
    break;
  case PD_FMT_R16F:
    load_floats(px, 1, 1, out);
    break;
  case PD_FMT_RG16F:
    load_floats(px, 2, 1, out);
    break;
  case PD_FMT_R32F:
    load_floats(px, 1, 0, out);
    break;
  case PD_FMT_RG32F:
    load_floats(px, 2, 0, out);
    break;
  case PD_FMT_RGBA32F:
    load_floats(px, 4, 0, out);
    break;
  case PD_FMT_RGBA8_SRGB:
    load_lanes(px, 4, 1, 255, out);
    for (c = 0; c < 3; c++) {
      out[c] = srgb_to_linear(out[c]);
    }
    break;
  case PD_FMT_RGB10A2:
  case PD_FMT_BGR10A2:
    load_1010102(px, fmt == PD_FMT_BGR10A2, out);
    break;
  case PD_FMT_R16:
    load_lanes(px, 1, 2, 65535, out);
    break;
  case PD_FMT_RG16:
    load_lanes(px, 2, 2, 65535, out);
    break;
  case PD_FMT_RGBA16:
    load_lanes(px, 4, 2, 65535, out);
    break;
  case PD_FMT_DEPTH32F:
  case PD_FMT_DEPTH16:
  case PD_FMT_DEPTH32F_S8:
  case PD_FMT_DEPTH24_S8:
    out[0] = pd_depth_load(fmt, px), out[1] = out[2] = 0, out[3] = 1;
    break;
  case PD_FMT_STENCIL8:
    out[0] = px[0], out[1] = out[2] = 0, out[3] = 1;
    break;
  default:
    out[0] = unorm8[px[2]];
    out[1] = unorm8[px[1]];
    out[2] = unorm8[px[0]];
    out[3] = unorm8[px[3]];
    if (fmt == PD_FMT_BGRA8_SRGB) {
      for (c = 0; c < 3; c++) {
        out[c] = srgb_to_linear(out[c]);
      }
    }
    break;
  }
}

void pd_texel_store(unsigned fmt, uint8_t *px, const float in[4]) {
  float s[4];
  unsigned c;

  switch (fmt) {
  case PD_FMT_RGBA16F:
    store_floats(px, 4, 1, in);
    break;
  case PD_FMT_A8:
    px[0] = pd_unorm8(in[3]);
    break;
  case PD_FMT_R8:
    store_lanes(px, 1, 1, 255, in);
    break;
  case PD_FMT_RG8:
    store_lanes(px, 2, 1, 255, in);
    break;
  case PD_FMT_RGBA8:
    store_lanes(px, 4, 1, 255, in);
    break;
  case PD_FMT_RG16UI:
    store_lanes(px, 2, 2, 0, in);
    break;
  case PD_FMT_R32UI:
    store_lanes(px, 1, 4, 0, in);
    break;
  case PD_FMT_RG32UI:
    store_lanes(px, 2, 4, 0, in);
    break;
  case PD_FMT_RGBA16UI:
    store_lanes(px, 4, 2, 0, in);
    break;
  case PD_FMT_R16UI:
    store_lanes(px, 1, 2, 0, in);
    break;
  case PD_FMT_RGBA8UI:
    store_lanes(px, 4, 1, 0, in);
    break;
  case PD_FMT_R8UI: case PD_FMT_R8I:
    store_lanes(px, 1, 1, 0, in);
    break;
  case PD_FMT_RG8UI: case PD_FMT_RG8I:
    store_lanes(px, 2, 1, 0, in);
    break;
  case PD_FMT_RGBA8I:
    store_lanes(px, 4, 1, 0, in);
    break;
  case PD_FMT_R16I:
    store_lanes(px, 1, 2, 0, in);
    break;
  case PD_FMT_RG16I:
    store_lanes(px, 2, 2, 0, in);
    break;
  case PD_FMT_RGBA16I:
    store_lanes(px, 4, 2, 0, in);
    break;
  case PD_FMT_R32I:
    store_lanes(px, 1, 4, 0, in);
    break;
  case PD_FMT_RG32I:
    store_lanes(px, 2, 4, 0, in);
    break;
  case PD_FMT_RGBA32UI: case PD_FMT_RGBA32I:
    store_lanes(px, 4, 4, 0, in);
    break;
  case PD_FMT_R16F:
    store_floats(px, 1, 1, in);
    break;
  case PD_FMT_RG16F:
    store_floats(px, 2, 1, in);
    break;
  case PD_FMT_R32F:
    store_floats(px, 1, 0, in);
    break;
  case PD_FMT_RG32F:
    store_floats(px, 2, 0, in);
    break;
  case PD_FMT_RGBA32F:
    store_floats(px, 4, 0, in);
    break;
  case PD_FMT_RGBA8_SRGB:
    for (c = 0; c < 3; c++) {
      s[c] = linear_to_srgb(in[c]);
    }
    s[3] = in[3];
    store_lanes(px, 4, 1, 255, s);
    break;
  case PD_FMT_RGB10A2:
  case PD_FMT_BGR10A2:
    store_1010102(px, fmt == PD_FMT_BGR10A2, in);
    break;
  case PD_FMT_R16:
    store_lanes(px, 1, 2, 65535, in);
    break;
  case PD_FMT_RG16:
    store_lanes(px, 2, 2, 65535, in);
    break;
  case PD_FMT_RGBA16:
    store_lanes(px, 4, 2, 65535, in);
    break;
  case PD_FMT_DEPTH32F:
  case PD_FMT_DEPTH16:
  case PD_FMT_DEPTH32F_S8:
  case PD_FMT_DEPTH24_S8:
    pd_depth_store(fmt, px, in[0]);
    break;
  case PD_FMT_STENCIL8:
    px[0] = (uint8_t)pd_clamp(in[0], 0, 255);
    break;
  default:
    memcpy(s, in, sizeof(s));
    if (fmt == PD_FMT_BGRA8_SRGB) {
      for (c = 0; c < 3; c++) {
        s[c] = linear_to_srgb(in[c]);
      }
    }
    px[0] = pd_unorm8(s[2]);
    px[1] = pd_unorm8(s[1]);
    px[2] = pd_unorm8(s[0]);
    px[3] = pd_unorm8(s[3]);
    break;
  }
}

// integer formats take no blending
int pd_fmt_is_int(unsigned fmt) {
  return (fmt >= PD_FMT_RG16UI && fmt <= PD_FMT_RGBA8UI) || fmt == PD_FMT_STENCIL8;
}

int pd_fmt_is_compressed(unsigned fmt) {
  return fmt >= PD_FMT_BC1 && fmt <= PD_FMT_BC7_SRGB;
}

int pd_fmt_cpu_decodes(unsigned fmt) {
  return fmt >= PD_FMT_BC1 && fmt <= PD_FMT_BC5_SNORM;
}

// BC1's colour block: two RGB565 endpoints and 2 bit indices, alpha from the punch-through mode unless bc1 is 0
static void bc_colour(const uint8_t *b, unsigned i, int bc1, float out[4]) {
  uint16_t c0 = (uint16_t)(b[0] | b[1] << 8), c1 = (uint16_t)(b[2] | b[3] << 8);
  uint32_t bits = (uint32_t)b[4] | (uint32_t)b[5] << 8 | (uint32_t)b[6] << 16 | (uint32_t)b[7] << 24;
  unsigned s = bits >> (2 * i) & 3;
  float e[2][3] = { { (c0 >> 11) / 31.0f, (c0 >> 5 & 63) / 63.0f, (c0 & 31) / 31.0f },
                    { (c1 >> 11) / 31.0f, (c1 >> 5 & 63) / 63.0f, (c1 & 31) / 31.0f } };

  out[3] = 1;
  for (unsigned c = 0; c < 3; c++) {
    if (s < 2)
      out[c] = e[s][c];
    else if (c0 > c1 || !bc1)
      out[c] = s == 2 ? (2 * e[0][c] + e[1][c]) / 3 : (e[0][c] + 2 * e[1][c]) / 3;
    else
      out[c] = s == 2 ? (e[0][c] + e[1][c]) / 2 : 0;
  }
  if (bc1 && c0 <= c1 && s == 3)
    out[3] = 0;
}

// BC4's channel block (BC3 alpha, BC5 each channel): two endpoints and 3 bit indices
static float bc_channel(const uint8_t *b, unsigned i, int snorm) {
  float e0 = snorm ? ((int8_t)b[0] < -127 ? -127 : (int8_t)b[0]) / 127.0f : b[0] / 255.0f;
  float e1 = snorm ? ((int8_t)b[1] < -127 ? -127 : (int8_t)b[1]) / 127.0f : b[1] / 255.0f;
  int gt = snorm ? (int8_t)b[0] > (int8_t)b[1] : b[0] > b[1];
  uint64_t bits = 0;
  unsigned s;

  for (unsigned k = 0; k < 6; k++)
    bits |= (uint64_t)b[2 + k] << (8 * k);
  s = (unsigned)(bits >> (3 * i) & 7);
  if (s == 0)
    return e0;
  if (s == 1)
    return e1;
  if (gt)
    return ((8 - s) * e0 + (s - 1) * e1) / 7;
  if (s == 6)
    return snorm ? -1 : 0;
  if (s == 7)
    return 1;
  return ((6 - s) * e0 + (s - 1) * e1) / 5;
}

// texel (x, y) of a BC1-BC5 texture with bpr bytes per row of blocks
void pd_bc_texel(unsigned fmt, const uint8_t *data, uint32_t bpr, uint32_t x, uint32_t y, float out[4]) {
  const uint8_t *b = data + (uint64_t)(y / 4) * bpr + (uint64_t)(x / 4) * tex_bpp(fmt);
  unsigned i = (y % 4) * 4 + x % 4;
  int srgb = fmt == PD_FMT_BC1_SRGB || fmt == PD_FMT_BC2_SRGB || fmt == PD_FMT_BC3_SRGB;

  switch (fmt) {
  case PD_FMT_BC1:
  case PD_FMT_BC1_SRGB:
    bc_colour(b, i, 1, out);
    break;
  case PD_FMT_BC2:
  case PD_FMT_BC2_SRGB:
    bc_colour(b + 8, i, 0, out);
    out[3] = (b[i / 2] >> (4 * (i & 1)) & 15) / 15.0f;
    break;
  case PD_FMT_BC3:
  case PD_FMT_BC3_SRGB:
    bc_colour(b + 8, i, 0, out);
    out[3] = bc_channel(b, i, 0);
    break;
  case PD_FMT_BC4:
  case PD_FMT_BC4_SNORM:
    out[0] = bc_channel(b, i, fmt == PD_FMT_BC4_SNORM), out[1] = out[2] = 0, out[3] = 1;
    break;
  case PD_FMT_BC5:
  case PD_FMT_BC5_SNORM:
    out[0] = bc_channel(b, i, fmt == PD_FMT_BC5_SNORM), out[1] = bc_channel(b + 8, i, fmt == PD_FMT_BC5_SNORM);
    out[2] = 0, out[3] = 1;
    break;
  default:
    out[0] = out[1] = out[2] = 0, out[3] = 1;
    return;
  }
  if (srgb) {
    for (unsigned c = 0; c < 3; c++)
      out[c] = srgb_to_linear(out[c]);
  }
}

int pd_fmt_is_depth(unsigned fmt) {
  return fmt >= PD_FMT_DEPTH32F && fmt <= PD_FMT_DEPTH24_S8;
}

int pd_fmt_has_stencil(unsigned fmt) {
  return fmt == PD_FMT_DEPTH32F_S8 || fmt == PD_FMT_DEPTH24_S8 || fmt == PD_FMT_STENCIL8;
}

float pd_depth_load(unsigned fmt, const uint8_t *px) {
  float f;
  uint32_t w;
  uint16_t h;

  switch (fmt) {
  case PD_FMT_DEPTH16:
    memcpy(&h, px, 2);
    return h / 65535.0f;
  case PD_FMT_DEPTH24_S8:
    memcpy(&w, px, 4);
    return (w & 0xffffff) / 16777215.0f;
  case PD_FMT_DEPTH32F:
  case PD_FMT_DEPTH32F_S8:
    memcpy(&f, px, 4);
    return f;
  default:
    return 0;
  }
}

void pd_depth_store(unsigned fmt, uint8_t *px, float d) {
  uint32_t w;
  uint16_t h;

  switch (fmt) {
  case PD_FMT_DEPTH16:
    h = (uint16_t)(pd_clamp(d, 0, 1) * 65535.0f + 0.5f);
    memcpy(px, &h, 2);
    break;
  case PD_FMT_DEPTH24_S8:
    memcpy(&w, px, 4);
    w = (w & 0xff000000u) | (uint32_t)(pd_clamp(d, 0, 1) * 16777215.0f + 0.5f);
    memcpy(px, &w, 4);
    break;
  case PD_FMT_DEPTH32F:
  case PD_FMT_DEPTH32F_S8:
    memcpy(px, &d, 4);
    break;
  }
}

uint8_t pd_stencil_load(unsigned fmt, const uint8_t *px) {
  return fmt == PD_FMT_DEPTH32F_S8 ? px[4] : fmt == PD_FMT_DEPTH24_S8 ? px[3] : fmt == PD_FMT_STENCIL8 ? px[0] : 0;
}

void pd_stencil_store(unsigned fmt, uint8_t *px, uint8_t s) {
  if (fmt == PD_FMT_DEPTH32F_S8)
    px[4] = s;
  else if (fmt == PD_FMT_DEPTH24_S8)
    px[3] = s;
  else if (fmt == PD_FMT_STENCIL8)
    px[0] = s;
}

void pd_tex_bind(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc) {
  t->data = desc->w ? base + desc->off : 0;
  t->w = desc->w, t->h = desc->h;
  t->bpr = desc->bpr, t->fmt = desc->fmt;
  t->type = t->levels = t->slices = t->depth = t->sliceBytes = 0;
}

void pd_tex_bind_layout(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc, const struct PDTexLayout *l) {
  pd_tex_bind(t, base, desc);
  if (!l || !(l->flags & PD_TEXLAYOUT_SET) || !desc->w)
    return;
  t->type = l->type ? l->type : PD_TEXTYPE_2D;
  t->levels = l->levels ? (l->levels < PD_TEX_LEVELS ? l->levels : PD_TEX_LEVELS) : 1;
  t->slices = t->type == PD_TEXTYPE_3D ? 1 : l->slices ? l->slices : 1;
  t->depth = t->type == PD_TEXTYPE_3D ? (l->slices ? l->slices : 1) : 1;
  t->sliceBytes = l->sliceBytes;
  memcpy(t->level, l->level, sizeof(t->level));
  t->level[0].off = 0, t->level[0].bpr = desc->bpr;
}

// sampler flags (host message type 3): bit 0 min and bit 2 mag linear, s/t address modes in
// bits 8-11 and 12-15 (Metal's: 0 clamp to edge, 2 repeat, 3 mirror repeat, 4 clamp to zero),
// bit 31 normalized coordinates. 0 means unknown: linear, clamped, normalized
#define PD_SAMP_LINEAR 0x5u
#define PD_SAMP_NORMALIZED 0x80000000u
#define PD_SAMP_ADDR_S(f) (((f) >> 8) & 0xf)
#define PD_SAMP_ADDR_T(f) (((f) >> 12) & 0xf)

enum { PD_ADDR_CLAMP_EDGE = 0, PD_ADDR_REPEAT = 2, PD_ADDR_MIRROR_REPEAT = 3, PD_ADDR_CLAMP_ZERO = 4 };

// a texel index through an address mode: -1 when it falls outside a clamp-to-zero edge
static int addr_texel(int i, int n, unsigned mode) {
  switch (mode) {
  case PD_ADDR_REPEAT:
    i %= n;
    return i < 0 ? i + n : i;
  case PD_ADDR_MIRROR_REPEAT: {
    int p = 2 * n;

    i %= p;
    if (i < 0)
      i += p;

    return i < n ? i : p - 1 - i;
  }
  case PD_ADDR_CLAMP_ZERO:
    return i < 0 || i >= n ? -1 : i;
  default:
    return i < 0 ? 0 : i >= n ? n - 1 : i;
  }
}

static void fetch_texel(const struct PDTex *t, int x, int y, unsigned ms, unsigned mt, float *out) {
  x = addr_texel(x, (int)t->w, ms);
  y = addr_texel(y, (int)t->h, mt);

  if (x < 0 || y < 0) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }

  if (pd_fmt_is_compressed(t->fmt))
    pd_bc_texel(t->fmt, t->data, t->bpr, (uint32_t)x, (uint32_t)y, out);
  else
    pd_texel_load(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), out);
}

// what the shaders hand the sampler beyond a coordinate, logged the first few times it shows up
static void note_sample(const struct PDSampler *smp, int32_t ox, int32_t oy, uint32_t lod_given, float lod) {
  static uint64_t seen[16];
  static unsigned nseen, nargs;
  uint64_t raw;
  unsigned i;

  if (smp && smp->magic != PD_SAMPLER_MAGIC && nseen < PD_ARRAY_LEN(seen)) {
    memcpy(&raw, smp, 8);
    for (i = 0; i < nseen && seen[i] != raw; i++) {
    }
    if (i == nseen) {
      seen[nseen++] = raw;
      logmsg("AIRJITD: shader constant sampler 0x%016llx", (unsigned long long)raw);
    }
  }

  if ((ox || oy || lod_given || lod != 0) && nargs < 24) {
    nargs++;
    logmsg("AIRJITD: sample offset %d,%d lod given %u value %g", ox, oy, lod_given, lod);
  }
}

// pd_texel_load with the two formats compositing samples most spelled out
static inline void load_quad_texel(unsigned fmt, const uint8_t *px, float out[4]) {
  unsigned c;

  if (fmt == PD_FMT_RGBA16F) {
    uint16_t h[4];

    memcpy(h, px, 8);
    for (c = 0; c < 4; c++) {
      out[c] = half_to_float(h[c]);
    }
  } else if (fmt == PD_FMT_BGRA8) {
    out[0] = unorm8[px[2]], out[1] = unorm8[px[1]], out[2] = unorm8[px[0]], out[3] = unorm8[px[3]];
  } else {
    pd_texel_load(fmt, px, out);
  }
}

void pd_sample(const struct PDTex *t, const struct PDSampler *smp, float u, float v, float *out) {
  pd_sample_ex(t, smp, u, v, 0, 0, 0, 0, out);
}

void pd_sample_ex(const struct PDTex *t, const struct PDSampler *smp, float u, float v, int32_t ox, int32_t oy,
                  uint32_t lod_given, float lod, float *out) {
  uint32_t fl = smp && smp->magic == PD_SAMPLER_MAGIC ? smp->flags : 0;
  unsigned ms = fl ? PD_SAMP_ADDR_S(fl) : PD_ADDR_CLAMP_EDGE, mt = fl ? PD_SAMP_ADDR_T(fl) : PD_ADDR_CLAMP_EDGE;
  float fx, fy, p[4][4];
  int x0, y0;
  unsigned c, k;

  if (!t || !t->data || !t->w || !t->h) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }

  note_sample(smp, ox, oy, lod_given, lod);

  // texel coordinates (a pixel sampler's, or with unknown state anything far outside 0-1) as they are:
  // dividing and multiplying back by the size can land a hair below a texel edge
  if (fl ? !(fl & PD_SAMP_NORMALIZED) : (u > 2 || u < -2 || v > 2 || v < -2))
    fx = u + (float)ox, fy = v + (float)oy;
  else
    fx = u * t->w + (float)ox, fy = v * t->h + (float)oy;

  // nearest: the texel the coordinate falls in
  if (fl && !(fl & PD_SAMP_LINEAR)) {
    fetch_texel(t, (int)(fx < 0 ? fx - 1 : fx), (int)(fy < 0 ? fy - 1 : fy), ms, mt, out);
    return;
  }

  fx -= 0.5f, fy -= 0.5f;
  x0 = (int)(fx < 0 ? fx - 1 : fx), y0 = (int)(fy < 0 ? fy - 1 : fy);
  fx -= x0, fy -= y0;

  // weights at 8 bits of sub-texel precision like the GPU's: a float's leftover never leaks a neighbour in
  fx = (float)(int)(fx * 256 + 0.5f) / 256, fy = (float)(int)(fy * 256 + 0.5f) / 256;

  // all four texels inside: every address mode maps them to themselves
  if (x0 >= 0 && y0 >= 0 && (uint32_t)x0 + 1 < t->w && (uint32_t)y0 + 1 < t->h && !pd_fmt_is_compressed(t->fmt)) {
    unsigned bpp = tex_bpp(t->fmt);
    const uint8_t *q = t->data + (uint64_t)(uint32_t)y0 * t->bpr + (uint64_t)(uint32_t)x0 * bpp;
    const uint8_t *at[4] = { q, q + bpp, q + t->bpr, q + t->bpr + bpp };

    for (k = 0; k < 4; k++) {
      load_quad_texel(t->fmt, at[k], p[k]);
    }
  } else {
    for (k = 0; k < 4; k++) {
      fetch_texel(t, x0 + (int)(k & 1), y0 + (int)(k >> 1), ms, mt, p[k]);
    }
  }

  for (c = 0; c < 4; c++) {
    out[c] = (p[0][c] * (1 - fx) + p[1][c] * fx) * (1 - fy) + (p[2][c] * (1 - fx) + p[3][c] * fx) * fy;
  }
}

// compute texture access by integer pixel
void pd_read_tex(const struct PDTex *t, uint32_t x, uint32_t y, float *out) {
  if (!t || !t->data || x >= t->w || y >= t->h) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }

  if (pd_fmt_is_compressed(t->fmt))
    pd_bc_texel(t->fmt, t->data, t->bpr, x, y, out);
  else
    pd_texel_load(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), out);
}

void pd_write_tex(const struct PDTex *t, uint32_t x, uint32_t y, const float *in) {
  if (!t || !t->data || x >= t->w || y >= t->h || pd_is_dry())
    return;

  pd_texel_store(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), in);
}

// an integer format's lanes and bytes per lane, 0 for the others
static unsigned int_lanes(unsigned fmt, unsigned *bytes) {
  static const struct {
    unsigned fmt, lanes, bytes;
  } f[] = { { PD_FMT_RG16UI, 2, 2 }, { PD_FMT_R32UI, 1, 4 }, { PD_FMT_RG32UI, 2, 4 },
            { PD_FMT_RGBA16UI, 4, 2 }, { PD_FMT_R16UI, 1, 2 }, { PD_FMT_RGBA8UI, 4, 1 } };

  for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
    if (f[i].fmt == fmt) {
      *bytes = f[i].bytes;
      return f[i].lanes;
    }
  }
  return 0;
}

void pd_read_tex_u(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t *out) {
  unsigned bytes, n, c;
  const uint8_t *p;
  float f[4];

  out[0] = out[1] = out[2] = 0, out[3] = 1;
  if (!t || !t->data || x >= t->w || y >= t->h)
    return;
  p = pd_texel_addr(t->data, t->bpr, t->fmt, x, y);
  if (!(n = int_lanes(t->fmt, &bytes))) {
    pd_texel_load(t->fmt, p, f);
    for (c = 0; c < 4; c++) {
      out[c] = (uint32_t)f[c];
    }
    return;
  }
  for (c = 0; c < n; c++) {
    uint32_t v = 0;
    memcpy(&v, p + c * bytes, bytes);
    out[c] = v;
  }
}

void pd_write_tex_u(const struct PDTex *t, uint32_t x, uint32_t y, const uint32_t *in) {
  unsigned bytes, n, c;
  uint8_t *p;
  float f[4];

  if (!t || !t->data || x >= t->w || y >= t->h || pd_is_dry())
    return;
  p = pd_texel_addr(t->data, t->bpr, t->fmt, x, y);
  if (!(n = int_lanes(t->fmt, &bytes))) {
    for (c = 0; c < 4; c++) {
      f[c] = (float)in[c];
    }
    pd_texel_store(t->fmt, p, f);
    return;
  }
  // a value too wide for its lane is cut to its low bits, as Metal stores it
  for (c = 0; c < n; c++) {
    memcpy(p + c * bytes, &in[c], bytes);
  }
}

uint32_t pd_tex_width(const struct PDTex *t) {
  return t ? t->w : 0;
}

uint32_t pd_tex_height(const struct PDTex *t) {
  return t ? t->h : 0;
}

static __thread int lod_on;

void pd_lod_active(int on) {
  lod_on = on;
}

static uint32_t lvl_dim(uint32_t n, unsigned k) {
  n >>= k;
  return n ? n : 1;
}

static unsigned tex_nlevels(const struct PDTex *t) {
  return t->levels ? t->levels : 1;
}

// level k of slice s (a 3D texture's z): its first byte, size and row pitch, NULL outside the texture
static const uint8_t *tex_at(const struct PDTex *t, unsigned k, unsigned s, uint32_t *w, uint32_t *h, uint32_t *bpr) {
  const uint8_t *p;

  if (!t || !t->data || k >= tex_nlevels(t))
    return 0;
  *w = lvl_dim(t->w, k), *h = lvl_dim(t->h, k);
  *bpr = k ? t->level[k].bpr : t->bpr;
  p = t->data + (k ? t->level[k].off : 0);
  if (t->type == PD_TEXTYPE_3D)
    return s < lvl_dim(t->depth, k) ? p + (uint64_t)s * *bpr * *h : 0;
  return s < (t->slices ? t->slices : 1) ? p + (uint64_t)s * t->sliceBytes : 0;
}

static unsigned cube_uv(const float *d, int face, float *u, float *v);

// a texel just past a cube face's edge is on the neighbouring face: its centre's direction picks it
static void cube_wrap(const struct PDTex *t, unsigned k, int *x, int *y, unsigned *slice) {
  unsigned n = lvl_dim(t->w, k), f = *slice % 6;
  float sc = ((float)*x + 0.5f) / (float)n * 2 - 1, tc = ((float)*y + 0.5f) / (float)n * 2 - 1, d[3], u, v;

  if (*x >= 0 && *y >= 0 && *x < (int)n && *y < (int)n)
    return;
  switch (f) {
  case 0: d[0] = 1, d[1] = -tc, d[2] = -sc; break;
  case 1: d[0] = -1, d[1] = -tc, d[2] = sc; break;
  case 2: d[0] = sc, d[1] = 1, d[2] = tc; break;
  case 3: d[0] = sc, d[1] = -1, d[2] = -tc; break;
  case 4: d[0] = sc, d[1] = -tc, d[2] = 1; break;
  default: d[0] = -sc, d[1] = -tc, d[2] = -1; break;
  }
  *slice = *slice - f + cube_uv(d, -1, &u, &v);
  *x = (int)floorf(u * (float)n), *y = (int)floorf(v * (float)n);
}

// one texel of level k through the address modes m (s, t, r), zeros outside a clamp-to-zero edge
static void texel_at(const struct PDTex *t, unsigned k, int x, int y, int z, unsigned slice, const unsigned *m, float *out) {
  uint32_t w, h, bpr;
  const uint8_t *p;

  if (t->type == PD_TEXTYPE_CUBE || t->type == PD_TEXTYPE_CUBE_ARRAY)
    cube_wrap(t, k, &x, &y, &slice);

  if (t->type == PD_TEXTYPE_3D) {
    z = addr_texel(z, (int)lvl_dim(t->depth, k), m[2]);
    slice = z < 0 ? ~0u : (unsigned)z;
  }
  p = tex_at(t, k, slice, &w, &h, &bpr);
  x = p ? addr_texel(x, (int)w, m[0]) : -1;
  y = p ? addr_texel(y, (int)h, m[1]) : -1;
  if (x < 0 || y < 0) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }
  pd_texel_load(t->fmt, p + (uint64_t)y * bpr + (uint64_t)x * tex_bpp(t->fmt), out);
}

static int ifloor(float f) {
  return (int)floorf(f);
}

// a weight at 8 bits of sub-texel precision, as the GPU filters
static float weight8(float f) {
  return (float)(int)(f * 256 + 0.5f) / 256;
}

// level k sampled at normalized c (u, v and a 3D texture's w), nearest or linear
static void sample_level(const struct PDTex *t, unsigned k, unsigned slice, const float *c, int linear, const unsigned *m,
                         const int32_t *o, float *out) {
  int three = t->type == PD_TEXTYPE_3D;
  float fx = c[0] * lvl_dim(t->w, k) + o[0], fy = c[1] * lvl_dim(t->h, k) + o[1];
  float fz = three ? c[2] * lvl_dim(t->depth, k) + o[2] : 0, p[8][4], wx, wy, wz;
  int x0, y0, z0;
  unsigned i, ch;

  if (!linear) {
    texel_at(t, k, ifloor(fx), ifloor(fy), ifloor(fz), slice, m, out);
    return;
  }

  fx -= 0.5f, fy -= 0.5f, fz -= 0.5f;
  x0 = ifloor(fx), y0 = ifloor(fy), z0 = ifloor(fz);
  wx = weight8(fx - x0), wy = weight8(fy - y0), wz = three ? weight8(fz - z0) : 0;
  for (i = 0; i < (three ? 8u : 4u); i++) {
    texel_at(t, k, x0 + (int)(i & 1), y0 + (int)(i >> 1 & 1), z0 + (int)(i >> 2), slice, m, p[i]);
  }
  for (ch = 0; ch < 4; ch++) {
    float a = (p[0][ch] * (1 - wx) + p[1][ch] * wx) * (1 - wy) + (p[2][ch] * (1 - wx) + p[3][ch] * wx) * wy;

    if (three)
      a = a * (1 - wz) + ((p[4][ch] * (1 - wx) + p[5][ch] * wx) * (1 - wy) + (p[6][ch] * (1 - wx) + p[7][ch] * wx) * wy) * wz;
    out[ch] = a;
  }
}

// the cube face a direction points at (+x -x +y -y +z -z) and its coordinates on it, face forces one
static unsigned cube_uv(const float *d, int face, float *u, float *v) {
  float ax = fabsf(d[0]), ay = fabsf(d[1]), az = fabsf(d[2]), ma, sc, tc;
  unsigned f;

  if (face >= 0)
    f = (unsigned)face;
  else if (ax >= ay && ax >= az)
    f = d[0] >= 0 ? 0 : 1;
  else if (ay >= az)
    f = d[1] >= 0 ? 2 : 3;
  else
    f = d[2] >= 0 ? 4 : 5;

  switch (f) {
  case 0: ma = d[0], sc = -d[2], tc = -d[1]; break;
  case 1: ma = -d[0], sc = d[2], tc = -d[1]; break;
  case 2: ma = d[1], sc = d[0], tc = d[2]; break;
  case 3: ma = -d[1], sc = d[0], tc = -d[2]; break;
  case 4: ma = d[2], sc = d[0], tc = -d[1]; break;
  default: ma = -d[2], sc = -d[0], tc = -d[1]; break;
  }
  if (ma == 0)
    ma = 1e-30f;
  *u = (sc / ma + 1) * 0.5f, *v = (tc / ma + 1) * 0.5f;
  return f;
}

// log2 of the larger footprint axis, from the coordinates' derivatives in level 0 texels
static float footprint_lod(const struct PDTex *t, const float *dx, const float *dy, unsigned n) {
  float s[3] = { (float)t->w, (float)t->h, (float)(t->depth ? t->depth : 1) }, px = 0, py = 0, r;
  unsigned i;

  for (i = 0; i < n; i++) {
    px += dx[i] * s[i] * dx[i] * s[i];
    py += dy[i] * s[i] * dy[i] * s[i];
  }
  r = px > py ? px : py;
  return r > 0 ? 0.5f * log2f(r) : -64;
}

void pd_sample_full(const struct PDTex *t, const struct PDSampler *smp, const float *c, uint32_t slice, const int32_t *o,
                    uint32_t mode, float lod, const float *g, float *out) {
  static const int32_t zo[3];
  uint32_t fl = smp && smp->magic == PD_SAMPLER_MAGIC ? smp->flags : 0;
  unsigned m[3], nl, n = 2, k, ch, mip;
  float uvw[3] = { c[0], c[1], 0 }, dx[3] = { 0 }, dy[3] = { 0 }, l = 0, a[4], b[4];
  int cube, face = 0, linear;

  if (!o)
    o = zo;
  if (mode & PD_LOD_3D_COMPAT) {
    mode &= 0xff;
    if (!t || t->type != PD_TEXTYPE_3D) {
      out[0] = c[0], out[1] = c[1], out[2] = c[2], out[3] = 1;
      return;
    }
  }
  // the 2D path textures without a layout always took
  if (!t || !t->type || (fl && !(fl & PD_SAMP_NORMALIZED))) {
    pd_sample_ex(t, smp, c[0], c[1], o[0], o[1], mode == PD_LOD_LEVEL, mode == PD_LOD_LEVEL || mode == PD_LOD_BIAS ? lod : 0, out);
    return;
  }

  cube = t->type == PD_TEXTYPE_CUBE || t->type == PD_TEXTYPE_CUBE_ARRAY;
  if (t->type == PD_TEXTYPE_3D)
    n = 3, uvw[2] = c[2];
  m[0] = fl ? PD_SAMP_ADDR_S(fl) : PD_ADDR_CLAMP_EDGE, m[1] = fl ? PD_SAMP_ADDR_T(fl) : PD_ADDR_CLAMP_EDGE;
  m[2] = fl ? (fl >> 16) & 0xf : PD_ADDR_CLAMP_EDGE;
  if (cube) {
    face = (int)cube_uv(c, -1, &uvw[0], &uvw[1]);
    m[0] = m[1] = PD_ADDR_CLAMP_EDGE;
  }

  // the level of detail: given, from gradients, or from the fragment's derivatives
  if (mode == PD_LOD_LEVEL) {
    l = lod;
  } else {
    unsigned nc = cube ? 3 : n, i;

    if (mode == PD_LOD_GRAD) {
      for (i = 0; i < nc; i++) {
        dx[i] = g[i], dy[i] = g[3 + i];
      }
    } else if (lod_on) {
      for (i = 0; i < nc; i++) {
        dx[i] = pd_deriv(PD_DERIV_DX, c[i]);
        dy[i] = pd_deriv(PD_DERIV_DY, c[i]);
      }
    }
    if (cube) {
      float p[3], q[3], u1, v1, u2, v2;

      for (i = 0; i < 3; i++) {
        p[i] = c[i] + dx[i], q[i] = c[i] + dy[i];
      }
      cube_uv(p, face, &u1, &v1);
      cube_uv(q, face, &u2, &v2);
      dx[0] = u1 - uvw[0], dx[1] = v1 - uvw[1], dy[0] = u2 - uvw[0], dy[1] = v2 - uvw[1];
      nc = 2;
    }
    l = footprint_lod(t, dx, dy, nc) + (mode == PD_LOD_BIAS ? lod : 0);
  }

  // magnifying takes the mag filter, minifying the min filter, unknown samplers are linear
  linear = !fl || (l > 0 ? (fl & 1) : (fl & 4));
  if (t->type == PD_TEXTYPE_2D_ARRAY || t->type == PD_TEXTYPE_CUBE_ARRAY) {
    uint32_t layers = t->slices / (cube ? 6 : 1);
    slice = layers ? (slice < layers ? slice : layers - 1) : 0;
  } else {
    slice = 0;
  }
  if (cube)
    slice = slice * 6 + (unsigned)face;

  nl = tex_nlevels(t);
  mip = fl ? PD_SAMP_MIP(fl) : 0;
  if (l < 0)
    l = 0;
  if (l > nl - 1)
    l = (float)(nl - 1);
  if (nl < 2 || mip == 0) {
    sample_level(t, 0, slice, uvw, linear, m, o, out);
    return;
  }
  if (mip == 1) {
    sample_level(t, (unsigned)floorf(l + 0.5f), slice, uvw, linear, m, o, out);
    return;
  }
  k = (unsigned)floorf(l);
  sample_level(t, k, slice, uvw, linear, m, o, a);
  if (k + 1 >= nl || l == (float)k) {
    memcpy(out, a, sizeof(a));
    return;
  }
  sample_level(t, k + 1, slice, uvw, linear, m, o, b);
  l = weight8(l - (float)k);
  for (ch = 0; ch < 4; ch++) {
    out[ch] = a[ch] * (1 - l) + b[ch] * l;
  }
}

void pd_read_tex_l(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, float *out) {
  uint32_t w, h, bpr;
  const uint8_t *p = tex_at(t, lod, s, &w, &h, &bpr);

  if (!p || x >= w || y >= h) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }
  pd_texel_load(t->fmt, p + (uint64_t)y * bpr + (uint64_t)x * tex_bpp(t->fmt), out);
}

void pd_write_tex_l(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, const float *in) {
  uint32_t w, h, bpr;
  const uint8_t *p = tex_at(t, lod, s, &w, &h, &bpr);

  if (!p || x >= w || y >= h || pd_is_dry())
    return;
  pd_texel_store(t->fmt, (uint8_t *)p + (uint64_t)y * bpr + (uint64_t)x * tex_bpp(t->fmt), in);
}

void pd_read_tex_lu(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, uint32_t *out) {
  struct PDTex at;
  uint32_t w, h, bpr;
  const uint8_t *p = tex_at(t, lod, s, &w, &h, &bpr);

  out[0] = out[1] = out[2] = 0, out[3] = 1;
  if (!p)
    return;
  at = *t, at.data = p, at.w = w, at.h = h, at.bpr = bpr;
  pd_read_tex_u(&at, x, y, out);
}

void pd_write_tex_lu(const struct PDTex *t, uint32_t x, uint32_t y, uint32_t s, uint32_t lod, const uint32_t *in) {
  struct PDTex at;
  uint32_t w, h, bpr;
  const uint8_t *p = tex_at(t, lod, s, &w, &h, &bpr);

  if (!p)
    return;
  at = *t, at.data = p, at.w = w, at.h = h, at.bpr = bpr;
  pd_write_tex_u(&at, x, y, in);
}

uint32_t pd_tex_query(const struct PDTex *t, uint32_t what, uint32_t lod) {
  int cube;

  if (!t || !t->data)
    return 0;
  // without a layout the level argument was never applied
  if (!t->type)
    lod = 0;
  cube = t->type == PD_TEXTYPE_CUBE || t->type == PD_TEXTYPE_CUBE_ARRAY;
  switch (what) {
  case 0: return lvl_dim(t->w, lod);
  case 1: return lvl_dim(t->h, lod);
  case 2: return t->type == PD_TEXTYPE_3D ? lvl_dim(t->depth, lod) : 1;
  case 3: return tex_nlevels(t);
  default: return t->slices ? (cube ? t->slices / 6 : t->slices) : 1;
  }
}
