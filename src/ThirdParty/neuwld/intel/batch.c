/* wld: intel/batch.c
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

#include "batch.h"
#include "mi.h"

#include <i915_drm.h>
#include <stdlib.h>

static const struct intel_device_info device_info_i965 = { .gen = 4 };
static const struct intel_device_info device_info_g4x = { .gen = 4 };
static const struct intel_device_info device_info_ilk = { .gen = 5 };
static const struct intel_device_info device_info_snb_gt1 = { .gen = 6 };
static const struct intel_device_info device_info_snb_gt2 = { .gen = 6 };
static const struct intel_device_info device_info_ivb_gt1 = { .gen = 7 };
static const struct intel_device_info device_info_ivb_gt2 = { .gen = 7 };
static const struct intel_device_info device_info_byt = { .gen = 7 };
static const struct intel_device_info device_info_hsw_gt1 = { .gen = 7 };
static const struct intel_device_info device_info_hsw_gt2 = { .gen = 7 };
static const struct intel_device_info device_info_hsw_gt3 = { .gen = 7 };
static const struct intel_device_info device_info_bdw_gt1 = { .gen = 8 };
static const struct intel_device_info device_info_bdw_gt2 = { .gen = 8 };
static const struct intel_device_info device_info_bdw_gt3 = { .gen = 8 };
static const struct intel_device_info device_info_bdw_rsvd = { .gen = 8 };
static const struct intel_device_info device_info_chv = { .gen = 8 };
static const struct intel_device_info device_info_skl_gt1 = { .gen = 9 };
static const struct intel_device_info device_info_skl_gt2 = { .gen = 9 };
static const struct intel_device_info device_info_skl_gt3 = { .gen = 9 };
static const struct intel_device_info device_info_skl_gt4 = { .gen = 9 };
static const struct intel_device_info device_info_bxt = { .gen = 9 };
static const struct intel_device_info device_info_bxt_2x6 = { .gen = 9 };
static const struct intel_device_info device_info_kbl_gt1 = { .gen = 9 };
static const struct intel_device_info device_info_kbl_gt1_5 = { .gen = 9 };
static const struct intel_device_info device_info_kbl_gt2 = { .gen = 9 };
static const struct intel_device_info device_info_kbl_gt3 = { .gen = 9 };
static const struct intel_device_info device_info_kbl_gt4 = { .gen = 9 };
static const struct intel_device_info device_info_glk = { .gen = 9 };
static const struct intel_device_info device_info_glk_2x6 = { .gen = 9 };
static const struct intel_device_info device_info_cfl_gt1 = { .gen = 9 };
static const struct intel_device_info device_info_cfl_gt2 = { .gen = 9 };
static const struct intel_device_info device_info_cfl_gt3 = { .gen = 9 };
static const struct intel_device_info device_info_cnl_2x8 = { .gen = 10 };
static const struct intel_device_info device_info_cnl_3x8 = { .gen = 10 };
static const struct intel_device_info device_info_cnl_4x8 = { .gen = 10 };
static const struct intel_device_info device_info_cnl_5x8 = { .gen = 10 };
static const struct intel_device_info device_info_cnl = { .gen = 10 };
static const struct intel_device_info device_info_icl = { .gen = 11 };
static const struct intel_device_info device_info_ehl = { .gen = 11 };
static const struct intel_device_info device_info_jpl = { .gen = 11 };
static const struct intel_device_info device_info_gen12 = { .gen = 12 };

static const struct intel_device_info *
device_info(int device_id)
{
	switch (device_id) {
#define CHIPSET(device_id, type, name) \
	case device_id:                \
		return &device_info_##type;
#include "i965_pci_ids.h"
#undef CHIPSET
	default:
		return NULL;
	}
}

bool
intel_batch_initialize(struct intel_batch *batch,
                       drm_intel_bufmgr *bufmgr)
{
	int device_id = drm_intel_bufmgr_gem_get_devid(bufmgr);

	batch->command_count = 0;
	batch->device_info = device_info(device_id);

	if (!batch->device_info)
		return false;

	/* Alignment argument (4096) is not used */
	batch->bo = drm_intel_bo_alloc(bufmgr, "batchbuffer", sizeof batch->commands, 4096);

	if (!batch->bo)
		return false;

	return true;
}

void
intel_batch_finalize(struct intel_batch *batch)
{
	drm_intel_bo_unreference(batch->bo);
}

void
intel_batch_flush(struct intel_batch *batch)
{
	if (batch->command_count == 0)
		return;

	intel_batch_add_dword(batch, MI_BATCH_BUFFER_END);

	/* Pad the batch buffer to the next quad-word. */
	if (batch->command_count & 1)
		intel_batch_add_dword(batch, MI_NOOP);

	drm_intel_bo_subdata(batch->bo, 0, batch->command_count << 2,
	                     batch->commands);
	drm_intel_bo_mrb_exec(batch->bo, batch->command_count << 2, NULL, 0, 0,
	                      GEN(batch, 6) ? I915_EXEC_BLT
	                                    : I915_EXEC_DEFAULT);
	drm_intel_gem_bo_clear_relocs(batch->bo, 0);
	batch->command_count = 0;
}
