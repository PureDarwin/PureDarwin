#include "texture.h"

#include <string.h>

float half_to_float(uint16_t h) {
  uint32_t sgn = (h >> 15) & 1, exp = (h >> 10) & 0x1f, man = h & 0x3ff, bits;
  float f;

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

uint16_t float_to_half(float f) {
  uint32_t bits, sgn, exp, man;

  memcpy(&bits, &f, 4);
  sgn = (bits >> 16) & 0x8000;
  exp = (bits >> 23) & 0xff;
  man = bits & 0x7fffff;

  if (exp == 0xff)
    return (uint16_t)(sgn | 0x7c00 | (man ? 0x200 : 0));

  if ((int)exp - 127 + 15 >= 31)
    return (uint16_t)(sgn | 0x7c00);

  if ((int)exp - 127 + 15 <= 0) {
    int sh = 14 - ((int)exp - 127);

    if ((int)exp - 127 + 15 < -10)
      return (uint16_t)sgn;

    man |= 0x800000;
    return (uint16_t)(sgn | (man >> sh));
  }

  return (uint16_t)(sgn | (((exp - 127 + 15) << 10) | (man >> 13)));
}

float pd_clamp(float x, float lo, float hi) {
  return x < lo ? lo : x > hi ? hi : x;
}

static uint8_t pd_unorm8(float f) {
  return (uint8_t)(pd_clamp(f, 0, 1) * 255.0f + 0.5f);
}

// bytes a pixel takes
unsigned tex_bpp(unsigned fmt) {
  return fmt == PD_FMT_RGBA16F ? 8 : (fmt == PD_FMT_A8 || fmt == PD_FMT_R8) ? 1 : fmt == PD_FMT_RG8 ? 2 : 4;
}

uint8_t *pd_texel_addr(const uint8_t *data, uint32_t bpr, unsigned fmt, uint32_t x, uint32_t y) {
  return (uint8_t *)data + (uint64_t)y * bpr + (uint64_t)x * tex_bpp(fmt);
}

// A8 reads as (0,0,0,a), R8 as (r,0,0,1) and RG8 as (r,g,0,1)
void pd_texel_load(unsigned fmt, const uint8_t *px, float out[4]) {
  unsigned c;

  switch (fmt) {
  case PD_FMT_RGBA16F: {
    uint16_t h[4];
    memcpy(h, px, 8);
    for (c = 0; c < 4; c++) {
      out[c] = half_to_float(h[c]);
    }
    break;
  }
  case PD_FMT_A8:
    out[0] = out[1] = out[2] = 0;
    out[3] = px[0] / 255.0f;
    break;
  case PD_FMT_R8:
    out[0] = px[0] / 255.0f;
    out[1] = out[2] = 0;
    out[3] = 1;
    break;
  case PD_FMT_RG8:
    out[0] = px[0] / 255.0f;
    out[1] = px[1] / 255.0f;
    out[2] = 0;
    out[3] = 1;
    break;
  case PD_FMT_RGBA8:
    for (c = 0; c < 4; c++) {
      out[c] = px[c] / 255.0f;
    }
    break;
  default:
    out[0] = px[2] / 255.0f;
    out[1] = px[1] / 255.0f;
    out[2] = px[0] / 255.0f;
    out[3] = px[3] / 255.0f;
    break;
  }
}

void pd_texel_store(unsigned fmt, uint8_t *px, const float in[4]) {
  unsigned c;

  switch (fmt) {
  case PD_FMT_RGBA16F: {
    uint16_t h[4];
    for (c = 0; c < 4; c++) {
      h[c] = float_to_half(in[c]);
    }
    memcpy(px, h, 8);
    break;
  }
  case PD_FMT_A8:
    px[0] = pd_unorm8(in[3]);
    break;
  case PD_FMT_R8:
    px[0] = pd_unorm8(in[0]);
    break;
  case PD_FMT_RG8:
    px[0] = pd_unorm8(in[0]);
    px[1] = pd_unorm8(in[1]);
    break;
  case PD_FMT_RGBA8:
    for (c = 0; c < 4; c++) {
      px[c] = pd_unorm8(in[c]);
    }
    break;
  default:
    px[0] = pd_unorm8(in[2]);
    px[1] = pd_unorm8(in[1]);
    px[2] = pd_unorm8(in[0]);
    px[3] = pd_unorm8(in[3]);
    break;
  }
}

void pd_tex_bind(struct PDTex *t, uint8_t *base, const struct PDDrawTex *desc) {
  t->data = desc->w ? base + desc->off : 0;
  t->w = desc->w, t->h = desc->h;
  t->bpr = desc->bpr, t->fmt = desc->fmt;
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

  pd_texel_load(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), out);
}

void pd_sample(const struct PDTex *t, const struct PDSampler *smp, float u, float v, float *out) {
  uint32_t fl = smp && smp->magic == PD_SAMPLER_MAGIC ? smp->flags : 0;
  unsigned ms = fl ? PD_SAMP_ADDR_S(fl) : PD_ADDR_CLAMP_EDGE, mt = fl ? PD_SAMP_ADDR_T(fl) : PD_ADDR_CLAMP_EDGE;
  float fx, fy, p[4][4];
  int x0, y0;
  unsigned c, k;

  if (!t || !t->data || !t->w || !t->h) {
    out[0] = out[1] = out[2] = out[3] = 0;
    return;
  }

  // texel coordinates: a pixel sampler's own, or, when the state is unknown, anything far outside 0-1
  if (fl ? !(fl & PD_SAMP_NORMALIZED) : (u > 2 || u < -2 || v > 2 || v < -2))
    u /= t->w, v /= t->h;

  fx = u * t->w, fy = v * t->h;

  // nearest: the texel the coordinate falls in
  if (fl && !(fl & PD_SAMP_LINEAR)) {
    fetch_texel(t, (int)(fx < 0 ? fx - 1 : fx), (int)(fy < 0 ? fy - 1 : fy), ms, mt, out);
    return;
  }

  fx -= 0.5f, fy -= 0.5f;
  x0 = (int)(fx < 0 ? fx - 1 : fx), y0 = (int)(fy < 0 ? fy - 1 : fy);
  fx -= x0, fy -= y0;

  for (k = 0; k < 4; k++) {
    fetch_texel(t, x0 + (int)(k & 1), y0 + (int)(k >> 1), ms, mt, p[k]);
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

  pd_texel_load(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), out);
}

void pd_write_tex(const struct PDTex *t, uint32_t x, uint32_t y, const float *in) {
  if (!t || !t->data || x >= t->w || y >= t->h)
    return;

  pd_texel_store(t->fmt, pd_texel_addr(t->data, t->bpr, t->fmt, x, y), in);
}

uint32_t pd_tex_width(const struct PDTex *t) {
  return t ? t->w : 0;
}

uint32_t pd_tex_height(const struct PDTex *t) {
  return t ? t->h : 0;
}
