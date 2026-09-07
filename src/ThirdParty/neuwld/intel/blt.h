/* wld: intel/blt.h
 *
 * Copyright (c) 2013, 2014 Michael Forney
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef WLD_INTEL_BLT_H
#define WLD_INTEL_BLT_H

#include <i915_drm.h>
#include <intel_bufmgr.h>

#define INTEL_CLIENT_BLT 0x2

enum blt_op {
	BLT_OP_XY_SETUP_BLT = 0x01,
	BLT_OP_XY_TEXT_BLT = 0x26,
	BLT_OP_XY_TEXT_IMMEDIATE_BLT = 0x31,
	BLT_OP_XY_COLOR_BLT = 0x50,
	BLT_OP_XY_SRC_COPY_BLT = 0x53
};

enum blt_32bpp_mask {
	BLT_32BPP_MASK_ALPHA = (1 << 0),
	BLT_32BPP_MASK_RGB = (1 << 1)
};

enum blt_packing {
	BLT_PACKING_BIT = 0,
	BLT_PACKING_BYTE = 1
};

enum blt_color_depth {
	BLT_COLOR_DEPTH_8BIT = 0x0,
	BLT_COLOR_DEPTH_16BIT_565 = 0x1,
	BLT_COLOR_DEPTH_16BIT_1555 = 0x2,
	BLT_COLOR_DEPTH_32BIT = 0x3
};

enum blt_raster_operation {
	BLT_RASTER_OPERATION_SRC = 0xcc,
	BLT_RASTER_OPERATION_PAT = 0xf0
};

/* BR00 : BLT Opcode & Control */
#define BLT_BR00_CLIENT(x) ((x) << 29)            /* 31:29 */
#define BLT_BR00_OP(x) ((x) << 22)                /* 28:22 */
#define BLT_BR00_32BPP_MASK(x) ((x) << 20)        /* 21:20 */
                                                  /* 19:17 */
#define BLT_BR00_PACKING(x) ((x) << 16)           /* 16 */
#define BLT_BR00_SRC_TILING_ENABLE(x) ((x) << 15) /* 15 */
                                                  /* 14:12 */
#define BLT_BR00_DST_TILING_ENABLE(x) ((x) << 11) /* 11 */
#define BLT_BR00_DWORD_LENGTH(x) ((x) << 0)       /* 7:0 */

/* BR01 : Setup BLT Raster OP, Control, and Destination Offset */
#define BLT_BR01_SOLID_PATTERN(x) ((x) << 31)         /* 31 */
#define BLT_BR01_CLIPPING_ENABLE(x) ((x) << 30)       /* 30 */
#define BLT_BR01_MONO_SRC_TRANSPARENCY(x) ((x) << 29) /* 29 */
#define BLT_BR01_MONO_PAT_TRANSPARENCY(x) ((x) << 28) /* 28 */
#define BLT_BR01_COLOR_DEPTH(x) ((x) << 24)           /* 25:24 */
#define BLT_BR01_RASTER_OPERATION(x) ((x) << 16)      /* 23:16 */
#define BLT_BR01_DST_PITCH(x) ((x) << 0)              /* 15:0 */

/* BR05 : Setup Expansion Background Color */
#define BLT_BR05_BACKGROUND_COLOR(x) ((x) << 0) /* 31:0 */

/* BR06 : Setup Expansion Foreground Color */
#define BLT_BR06_FOREGROUND_COLOR(x) ((x) << 0) /* 31:0 */

/* BR07 : Setup Blit Color Pattern Address Low Bits */
/* 31:29 */
#define BLT_BR07_PAT_ADDRESS(x) ((x) << 6) /* 28:6 */
                                           /* 5:0 */

/* BR09 : Destination Address Low Bits */
/* 31:29 */
#define BLT_BR09_DST_ADDRESS(x) ((x) << 0) /* 28:0 */

/* BR11 : Source Pitch */
/* 31:16 */
#define BLT_BR11_SRC_PITCH(x) ((x) << 0) /* 15:0 */

/* BR12 : Source Address Low Bits */
/* 31:29 */
#define BLT_BR12_SRC_ADDRESS(x) ((x) << 0) /* 28:0 */

/* BR13 : BLT Raster OP, Control, and Destination Pitch */
#define BLT_BR13_SOLID_PATTERN(x) ((x) << 31)        /* 31 */
#define BLT_BR13_CLIPPING_ENABLE(x) ((x) << 30)      /* 30 */
#define BLT_BR13_MONO_SRC_TRANSPARENT(x) ((x) << 29) /* 29 */
#define BLT_BR13_MONO_PAT_TRANSPARENT(x) ((x) << 28) /* 28 */
#define BLT_BR13_COLOR_DEPTH(x) ((x) << 24)          /* 25:24 */
#define BLT_BR13_RASTER_OPERATION(x) ((x) << 16)     /* 23:16 */
#define BLT_BR13_DST_PITCH(x) ((x) << 0)             /* 15:0 */

/* BR16 : Pattern Expansion Background & Solid Pattern Color */
#define BLT_BR16_COLOR(x) ((x) << 0) /* 31 : 0 */

/* BR22 : Destination Top Left */
#define BLT_BR22_DST_Y1(x) ((x) << 16) /* 31:16 */
#define BLT_BR22_DST_X1(x) ((x) << 0)  /* 16:0 */

/* BR23 : Destination Bottom Right */
#define BLT_BR23_DST_Y2(x) ((x) << 16) /* 31:16 */
#define BLT_BR23_DST_X2(x) ((x) << 0)  /* 16:0 */

/* BR24 : Clip Rectangle Top Left */
/* 31 */
#define BLT_BR24_CLP_Y1(x) ((x) << 16) /* 30:16 */
                                       /* 15 */
#define BLT_BR24_CLP_X1(x) ((x) << 0)  /* 14:0 */

/* BR25 : Clip Rectangle Bottom Right */
/* 31 */
#define BLT_BR25_CLP_Y2(x) ((x) << 16) /* 30:16 */
                                       /* 15 */
#define BLT_BR25_CLP_X2(x) ((x) << 0)  /* 14:0 */

/* BR26 : Source Top Left */
#define BLT_BR26_SRC_Y1(x) ((x) << 16) /* 31:16 */
#define BLT_BR26_SRC_X1(x) ((x) << 0)  /* 15:0 */

/* BR27 : Destination Address High Bits */
/* 31:16 */
#define BLT_BR27_DST_ADDRESS_HI(x) ((x) << 0) /* 15:0 */

/* BR28 : Source Address High Bits */
/* 31:16 */
#define BLT_BR28_SRC_ADDRESS_HI(x) ((x) << 0) /* 15:0 */

/* BR30 : Setup Blit Color Pattern Address High Bits */
/* 31:16 */
#define BLT_BR30_PAT_ADDRESS_HI(x) ((x) << 0) /* 15:0 */

static inline void
xy_setup_blt(struct intel_batch *batch,
             bool monochrome_source_transparency,
             uint8_t raster_operation,
             uint32_t background_color,
             uint32_t foreground_color,
             drm_intel_bo *dst, uint16_t dst_pitch)
{
	uint32_t tiling_mode, swizzle_mode;

	intel_batch_ensure_space(batch, GEN(batch, 8) ? 10 : 8);

	drm_intel_bo_get_tiling(dst, &tiling_mode, &swizzle_mode);
	drm_intel_bo_emit_reloc_fence(batch->bo, intel_batch_offset(batch, 4), dst, 0,
	                              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);

	intel_batch_add_dwords(batch, 4,
	                       BLT_BR00_CLIENT(INTEL_CLIENT_BLT)
	                           | BLT_BR00_OP(BLT_OP_XY_SETUP_BLT)
	                           | BLT_BR00_32BPP_MASK(BLT_32BPP_MASK_ALPHA | BLT_32BPP_MASK_RGB)
	                           | BLT_BR00_DST_TILING_ENABLE(tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DWORD_LENGTH(GEN(batch, 8) ? 8 : 6),

	                       BLT_BR01_CLIPPING_ENABLE(false)
	                           | BLT_BR01_MONO_SRC_TRANSPARENCY(monochrome_source_transparency)
	                           | BLT_BR01_COLOR_DEPTH(BLT_COLOR_DEPTH_32BIT)
	                           | BLT_BR01_RASTER_OPERATION(raster_operation)
	                           | BLT_BR01_DST_PITCH(tiling_mode == I915_TILING_NONE
	                                                    ? dst_pitch
	                                                    : dst_pitch >> 2),

	                       /* XXX: No clipping yet */
	                       BLT_BR24_CLP_Y1(0)
	                           | BLT_BR24_CLP_X1(0),

	                       BLT_BR25_CLP_Y2(0)
	                           | BLT_BR25_CLP_X2(0));

	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR09_DST_ADDRESS(dst->offset64),
	                       /* if gen8 */
	                       BLT_BR27_DST_ADDRESS_HI(dst->offset64 >> 32));

	intel_batch_add_dwords(batch, 2,
	                       BLT_BR05_BACKGROUND_COLOR(background_color),
	                       BLT_BR06_FOREGROUND_COLOR(foreground_color));

	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR07_PAT_ADDRESS(0),
	                       /* if gen8 */
	                       BLT_BR30_PAT_ADDRESS_HI(0));
}

static inline int
xy_text_blt(struct intel_batch *batch,
            drm_intel_bo *src, uint32_t src_offset,
            drm_intel_bo *dst,
            int16_t dst_x1, int16_t dst_y1,
            int16_t dst_x2, int16_t dst_y2)
{
	uint32_t tiling_mode, swizzle_mode;

	if (!intel_batch_check_space(batch, GEN(batch, 8) ? 5 : 4))
		return INTEL_BATCH_NO_SPACE;

	drm_intel_bo_get_tiling(dst, &tiling_mode, &swizzle_mode);

	drm_intel_bo_emit_reloc_fence(batch->bo, intel_batch_offset(batch, 3), src, src_offset,
	                              I915_GEM_DOMAIN_RENDER, 0);

	intel_batch_add_dwords(batch, 3,
	                       BLT_BR00_CLIENT(INTEL_CLIENT_BLT)
	                           | BLT_BR00_OP(BLT_OP_XY_TEXT_BLT)
	                           | BLT_BR00_PACKING(BLT_PACKING_BYTE)
	                           | BLT_BR00_DST_TILING_ENABLE(tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DWORD_LENGTH(GEN(batch, 8) ? 3 : 2),

	                       BLT_BR22_DST_Y1(dst_y1) | BLT_BR22_DST_X1(dst_x1),
	                       BLT_BR23_DST_Y2(dst_y2) | BLT_BR23_DST_X2(dst_x2));

	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR12_SRC_ADDRESS(src->offset64 + src_offset),
	                       /* if gen8 */
	                       BLT_BR28_SRC_ADDRESS_HI((src->offset64 + src_offset) >> 32));

	return INTEL_BATCH_SUCCESS;
}

static inline int
xy_text_immediate_blt(struct intel_batch *batch,
                      drm_intel_bo *dst,
                      int16_t dst_x1, int16_t dst_y1,
                      int16_t dst_x2, int16_t dst_y2,
                      uint16_t count, uint32_t *immediates)
{
	/* Round up to the next even number. */
	uint8_t dwords = (count + 1) & ~1;
	uint32_t index;
	uint32_t tiling_mode, swizzle_mode;

	if (!intel_batch_check_space(batch, 3 + dwords))
		return INTEL_BATCH_NO_SPACE;

	drm_intel_bo_get_tiling(dst, &tiling_mode, &swizzle_mode);

	intel_batch_add_dwords(batch, 3,
	                       BLT_BR00_CLIENT(INTEL_CLIENT_BLT)
	                           | BLT_BR00_OP(BLT_OP_XY_TEXT_IMMEDIATE_BLT)
	                           | BLT_BR00_PACKING(BLT_PACKING_BYTE)
	                           | BLT_BR00_DST_TILING_ENABLE(tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DWORD_LENGTH(1 + dwords),

	                       BLT_BR22_DST_Y1(dst_y1) | BLT_BR22_DST_X1(dst_x1),
	                       BLT_BR23_DST_Y2(dst_y2) | BLT_BR23_DST_X2(dst_x2));

	for (index = 0; index < count; ++index)
		intel_batch_add_dword(batch, *immediates++);

	/* From BLT engine documentation:
	 *
	 * The IMMEDIATE_BLT data MUST transfer an even number of doublewords. The
	 * BLT engine will hang if it does not get an even number of doublewords. */
	if (count & 1)
		intel_batch_add_dword(batch, 0);

	return INTEL_BATCH_SUCCESS;
}

static inline void
xy_src_copy_blt(struct intel_batch *batch,
                drm_intel_bo *src, uint16_t src_pitch,
                uint16_t src_x, uint16_t src_y,
                drm_intel_bo *dst, uint16_t dst_pitch,
                uint16_t dst_x, uint16_t dst_y,
                uint16_t width, uint16_t height)
{
	uint32_t src_tiling_mode, dst_tiling_mode, swizzle;

	intel_batch_ensure_space(batch, GEN(batch, 8) ? 10 : 8);

	drm_intel_bo_get_tiling(dst, &dst_tiling_mode, &swizzle);
	drm_intel_bo_get_tiling(src, &src_tiling_mode, &swizzle);

	drm_intel_bo_emit_reloc_fence(batch->bo, intel_batch_offset(batch, 4), dst, 0,
	                              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);
	drm_intel_bo_emit_reloc_fence(batch->bo, intel_batch_offset(batch, GEN(batch, 8) ? 8 : 7), src, 0,
	                              I915_GEM_DOMAIN_RENDER, 0);

	intel_batch_add_dwords(batch, 4,
	                       BLT_BR00_CLIENT(INTEL_CLIENT_BLT)
	                           | BLT_BR00_OP(BLT_OP_XY_SRC_COPY_BLT)
	                           | BLT_BR00_32BPP_MASK(BLT_32BPP_MASK_ALPHA | BLT_32BPP_MASK_RGB)
	                           | BLT_BR00_SRC_TILING_ENABLE(src_tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DST_TILING_ENABLE(dst_tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DWORD_LENGTH(GEN(batch, 8) ? 8 : 6),

	                       BLT_BR13_CLIPPING_ENABLE(false)
	                           | BLT_BR13_COLOR_DEPTH(BLT_COLOR_DEPTH_32BIT)
	                           | BLT_BR13_RASTER_OPERATION(BLT_RASTER_OPERATION_SRC)
	                           | BLT_BR13_DST_PITCH(dst_tiling_mode == I915_TILING_NONE
	                                                    ? dst_pitch
	                                                    : dst_pitch >> 2),

	                       BLT_BR22_DST_Y1(dst_y) | BLT_BR22_DST_X1(dst_x),

	                       BLT_BR23_DST_Y2(dst_y + height)
	                           | BLT_BR23_DST_X2(dst_x + width));
	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR09_DST_ADDRESS(dst->offset64),
	                       BLT_BR27_DST_ADDRESS_HI(dst->offset64 >> 32));
	intel_batch_add_dwords(batch, 2,
	                       BLT_BR26_SRC_Y1(src_y) | BLT_BR26_SRC_X1(src_x),
	                       BLT_BR11_SRC_PITCH(src_tiling_mode == I915_TILING_NONE
	                                              ? src_pitch
	                                              : src_pitch >> 2));
	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR12_SRC_ADDRESS(src->offset64),
	                       BLT_BR28_SRC_ADDRESS_HI(src->offset64 >> 32));
}

static inline void
xy_color_blt(struct intel_batch *batch,
             drm_intel_bo *dst, uint16_t dst_pitch,
             uint16_t dst_x1, uint16_t dst_y1,
             uint16_t dst_x2, uint16_t dst_y2,
             uint32_t color)
{
	uint32_t tiling_mode, swizzle_mode;

	intel_batch_ensure_space(batch, GEN(batch, 8) ? 7 : 6);

	drm_intel_bo_get_tiling(dst, &tiling_mode, &swizzle_mode);

	drm_intel_bo_emit_reloc_fence(batch->bo, intel_batch_offset(batch, 4), dst, 0,
	                              I915_GEM_DOMAIN_RENDER, I915_GEM_DOMAIN_RENDER);

	intel_batch_add_dwords(batch, 4,
	                       BLT_BR00_CLIENT(INTEL_CLIENT_BLT)
	                           | BLT_BR00_OP(BLT_OP_XY_COLOR_BLT)
	                           | BLT_BR00_32BPP_MASK(BLT_32BPP_MASK_ALPHA | BLT_32BPP_MASK_RGB)
	                           | BLT_BR00_DST_TILING_ENABLE(tiling_mode != I915_TILING_NONE)
	                           | BLT_BR00_DWORD_LENGTH(GEN(batch, 8) ? 5 : 4),

	                       BLT_BR13_CLIPPING_ENABLE(false)
	                           | BLT_BR13_COLOR_DEPTH(BLT_COLOR_DEPTH_32BIT)
	                           | BLT_BR13_RASTER_OPERATION(BLT_RASTER_OPERATION_PAT)
	                           | BLT_BR13_DST_PITCH(tiling_mode == I915_TILING_NONE
	                                                    ? dst_pitch
	                                                    : dst_pitch >> 2),

	                       BLT_BR22_DST_Y1(dst_y1) | BLT_BR22_DST_X1(dst_x1),
	                       BLT_BR23_DST_Y2(dst_y2) | BLT_BR23_DST_X2(dst_x2));
	intel_batch_add_dwords(batch, GEN(batch, 8) ? 2 : 1,
	                       BLT_BR09_DST_ADDRESS(dst->offset64),
	                       BLT_BR27_DST_ADDRESS_HI(dst->offset64 >> 32));
	intel_batch_add_dword(batch,
	                      BLT_BR16_COLOR(color));
}

#endif
