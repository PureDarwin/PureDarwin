#include "blend.h"

static float blend_factor(unsigned f, const float *s, const float *d, int c) {
  switch (f) {
  case PD_BF_ZERO:
    return 0;
  case PD_BF_ONE:
    return 1;
  case PD_BF_SRC_COLOR:
    return s[c];
  case PD_BF_ONE_MINUS_SRC_COLOR:
    return 1 - s[c];
  case PD_BF_SRC_ALPHA:
    return s[3];
  case PD_BF_ONE_MINUS_SRC_ALPHA:
    return 1 - s[3];
  case PD_BF_DST_COLOR:
    return d[c];
  case PD_BF_ONE_MINUS_DST_COLOR:
    return 1 - d[c];
  case PD_BF_DST_ALPHA:
    return d[3];
  case PD_BF_ONE_MINUS_DST_ALPHA:
    return 1 - d[3];
  case PD_BF_SRC_ALPHA_SATURATED:
    return c == 3 ? 1 : (s[3] < 1 - d[3] ? s[3] : 1 - d[3]);
  default:
    return 1;
  }
}

static float blend_op(unsigned op, float a, float b, float sv, float dv) {
  switch (op) {
  case PD_BO_SUBTRACT:
    return a - b;
  case PD_BO_REVERSE_SUBTRACT:
    return b - a;
  case PD_BO_MIN:
    return sv < dv ? sv : dv;
  case PD_BO_MAX:
    return sv > dv ? sv : dv;
  default:
    return a + b;
  }
}

static void blend_fixed(uint32_t st, float *s, const float *d) {
  unsigned srgb = PD_BLEND_SRC_RGB(st), drgb = PD_BLEND_DST_RGB(st);
  unsigned sa = PD_BLEND_SRC_A(st), da = PD_BLEND_DST_A(st);
  unsigned c;
  float o[4];

  for (c = 0; c < 3; c++) {
    o[c] = blend_op(PD_BLEND_OP_RGB(st), s[c] * blend_factor(srgb, s, d, c), d[c] * blend_factor(drgb, s, d, c), s[c],
                    d[c]);
  }

  o[3] = blend_op(PD_BLEND_OP_A(st), s[3] * blend_factor(sa, s, d, 3), d[3] * blend_factor(da, s, d, 3), s[3], d[3]);

  for (c = 0; c < 4; c++) {
    s[c] = o[c];
  }
}

int pd_blend_needs_dest(uint32_t st, const float *col) {
  if (!(st & PD_BLEND_KNOWN))
    return col[3] < 1;

  return (st & PD_BLEND_ENABLED) || PD_BLEND_MASK(st) != PD_MASK_ALL;
}

void pd_blend_pixel(uint32_t st, float *col, const float *d) {
  unsigned k;

  // unknown state: premultiplied source over, what the compositor's layers expect
  if (!(st & PD_BLEND_KNOWN)) {
    float ia = 1 - col[3];
    for (k = 0; k < 4; k++) {
      col[k] += d[k] * ia;
    }
    return;
  }

  if (st & PD_BLEND_ENABLED)
    blend_fixed(st, col, d);

  for (k = 0; k < 4; k++) {
    if (!(PD_BLEND_MASK(st) & (PD_MASK_RED >> k)))
      col[k] = d[k];
  }
}
