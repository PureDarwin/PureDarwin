#ifndef AIRJITD_BLEND_H
#define AIRJITD_BLEND_H

#include <stdint.h>

// the kext packs a pipeline's blend state into PDDrawRequest.blend
#define PD_BLEND_KNOWN (1u << 0)
#define PD_BLEND_ENABLED (1u << 1)
#define PD_BLEND_SRC_RGB(s) (((s) >> 2) & 31)
#define PD_BLEND_DST_RGB(s) (((s) >> 7) & 31)
#define PD_BLEND_SRC_A(s) (((s) >> 12) & 31)
#define PD_BLEND_DST_A(s) (((s) >> 17) & 31)
#define PD_BLEND_OP_RGB(s) (((s) >> 22) & 7)
#define PD_BLEND_OP_A(s) (((s) >> 25) & 7)
#define PD_BLEND_MASK(s) ((s) >> 28)

// write mask bits
#define PD_MASK_RED 8u
#define PD_MASK_GREEN 4u
#define PD_MASK_BLUE 2u
#define PD_MASK_ALPHA 1u
#define PD_MASK_ALL 15u

// MTLBlendFactor
enum {
  PD_BF_ZERO = 0,
  PD_BF_ONE = 1,
  PD_BF_SRC_COLOR = 2,
  PD_BF_ONE_MINUS_SRC_COLOR = 3,
  PD_BF_SRC_ALPHA = 4,
  PD_BF_ONE_MINUS_SRC_ALPHA = 5,
  PD_BF_DST_COLOR = 6,
  PD_BF_ONE_MINUS_DST_COLOR = 7,
  PD_BF_DST_ALPHA = 8,
  PD_BF_ONE_MINUS_DST_ALPHA = 9,
  PD_BF_SRC_ALPHA_SATURATED = 10,
};

// MTLBlendOperation
enum {
  PD_BO_ADD = 0,
  PD_BO_SUBTRACT = 1,
  PD_BO_REVERSE_SUBTRACT = 2,
  PD_BO_MIN = 3,
  PD_BO_MAX = 4,
};

// whether writing col needs the destination: blending, a partial write mask, or the
// premultiplied source-over used when the kext did not find the pipeline's state
int pd_blend_needs_dest(uint32_t st, const float *col);

// col = what lands in the target given the destination d
void pd_blend_pixel(uint32_t st, float *col, const float *d);

#endif
