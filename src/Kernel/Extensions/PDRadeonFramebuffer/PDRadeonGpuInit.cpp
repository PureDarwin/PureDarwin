/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */

#include "PDRadeonCard.h"
#include "RadeonR700Regs.h"

static uint32_t remapBackend(uint32_t tiling, uint32_t maximum, uint32_t total, uint32_t disabled)
{
    uint32_t unavailable = disabled | ((0xffu << maximum) & 0xff);
    if ((unavailable & 0xff) != 0xff)
        disabled = unavailable;
    uint32_t pipes = 1u << tiling;
    uint32_t count = total - __builtin_popcount(disabled);
    if (!count || count > pipes)
        return 0;
    uint32_t ratio = pipes / count, remain = pipes % count, data = 0;
    for (unsigned i = maximum; i-- > 0;) {
        if (disabled & (1u << i))
            continue;
        for (unsigned j = 0; j < ratio; j++)
            data = (data << 2) | i;
        if (remain) {
            data = (data << 2) | i;
            remain--;
        }
    }
    return data;
}

void RadeonCard::gpuInit(void)
{
    enum { CHIP_RV770, CHIP_RV730, CHIP_RV710, CHIP_RV740 };
    const int family = CHIP_RV730;
    struct { uint32_t active_simds, backend_map, max_backends, max_gprs, max_gs_threads, max_hw_contexts, max_pipes, max_simds, max_stack_entries, max_threads, max_tile_pipes, sc_earlyz_tile_fifo_fize, sc_hiz_tile_fifo_size, sc_prim_fifo_size, sq_num_cf_insts, sx_max_export_pos_size, sx_max_export_size, sx_max_export_smx_size, sx_num_of_sets, tile_config, tiling_group_size, tiling_nbanks, tiling_npipes; } spec = {};
	int i, j, num_qd_pipes;
	uint32_t ta_aux_cntl;
	uint32_t sx_debug_1;
	uint32_t smx_dc_ctl0;
	uint32_t db_debug3;
	uint32_t num_gs_verts_per_thread;
	uint32_t vgt_gs_per_es;
	uint32_t gs_prim_buffer_depth = 0;
	uint32_t sq_ms_fifo_sizes;
	uint32_t sq_config;
	uint32_t sq_thread_resource_mgmt;
	uint32_t hdp_host_path_cntl;
	uint32_t sq_dyn_gpr_size_simd_ab_0;
	uint32_t gb_tiling_config = 0;
	uint32_t cc_gc_shader_pipe_config = 0;
	uint32_t mc_arb_ramcfg;
	uint32_t db_debug4, tmp;
	uint32_t inactive_pipes, shader_pipe_config;
	uint32_t disabled_rb_mask;
	unsigned active_number;

	/* setup chip specs */
	spec.tiling_group_size = 256;
	switch (family) {
	case CHIP_RV770:
		spec.max_pipes = 4;
		spec.max_tile_pipes = 8;
		spec.max_simds = 10;
		spec.max_backends = 4;
		spec.max_gprs = 256;
		spec.max_threads = 248;
		spec.max_stack_entries = 512;
		spec.max_hw_contexts = 8;
		spec.max_gs_threads = 16 * 2;
		spec.sx_max_export_size = 128;
		spec.sx_max_export_pos_size = 16;
		spec.sx_max_export_smx_size = 112;
		spec.sq_num_cf_insts = 2;

		spec.sx_num_of_sets = 7;
		spec.sc_prim_fifo_size = 0xF9;
		spec.sc_hiz_tile_fifo_size = 0x30;
		spec.sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV730:
		spec.max_pipes = 2;
		spec.max_tile_pipes = 4;
		spec.max_simds = 8;
		spec.max_backends = 2;
		spec.max_gprs = 128;
		spec.max_threads = 248;
		spec.max_stack_entries = 256;
		spec.max_hw_contexts = 8;
		spec.max_gs_threads = 16 * 2;
		spec.sx_max_export_size = 256;
		spec.sx_max_export_pos_size = 32;
		spec.sx_max_export_smx_size = 224;
		spec.sq_num_cf_insts = 2;

		spec.sx_num_of_sets = 7;
		spec.sc_prim_fifo_size = 0xf9;
		spec.sc_hiz_tile_fifo_size = 0x30;
		spec.sc_earlyz_tile_fifo_fize = 0x130;
		if (spec.sx_max_export_pos_size > 16) {
			spec.sx_max_export_pos_size -= 16;
			spec.sx_max_export_smx_size += 16;
		}
		break;
	case CHIP_RV710:
		spec.max_pipes = 2;
		spec.max_tile_pipes = 2;
		spec.max_simds = 2;
		spec.max_backends = 1;
		spec.max_gprs = 256;
		spec.max_threads = 192;
		spec.max_stack_entries = 256;
		spec.max_hw_contexts = 4;
		spec.max_gs_threads = 8 * 2;
		spec.sx_max_export_size = 128;
		spec.sx_max_export_pos_size = 16;
		spec.sx_max_export_smx_size = 112;
		spec.sq_num_cf_insts = 1;

		spec.sx_num_of_sets = 7;
		spec.sc_prim_fifo_size = 0x40;
		spec.sc_hiz_tile_fifo_size = 0x30;
		spec.sc_earlyz_tile_fifo_fize = 0x130;
		break;
	case CHIP_RV740:
		spec.max_pipes = 4;
		spec.max_tile_pipes = 4;
		spec.max_simds = 8;
		spec.max_backends = 4;
		spec.max_gprs = 256;
		spec.max_threads = 248;
		spec.max_stack_entries = 512;
		spec.max_hw_contexts = 8;
		spec.max_gs_threads = 16 * 2;
		spec.sx_max_export_size = 256;
		spec.sx_max_export_pos_size = 32;
		spec.sx_max_export_smx_size = 224;
		spec.sq_num_cf_insts = 2;

		spec.sx_num_of_sets = 7;
		spec.sc_prim_fifo_size = 0x100;
		spec.sc_hiz_tile_fifo_size = 0x30;
		spec.sc_earlyz_tile_fifo_fize = 0x130;

		if (spec.sx_max_export_pos_size > 16) {
			spec.sx_max_export_pos_size -= 16;
			spec.sx_max_export_smx_size += 16;
		}
		break;
	default:
		break;
	}

	/* Initialize HDP */
	j = 0;
	for (i = 0; i < 32; i++) {
		wreg((0x2c14 + j), 0x00000000);
		wreg((0x2c18 + j), 0x00000000);
		wreg((0x2c1c + j), 0x00000000);
		wreg((0x2c20 + j), 0x00000000);
		wreg((0x2c24 + j), 0x00000000);
		j += 0x18;
	}

	wreg(GRBM_CNTL, GRBM_READ_TIMEOUT(0xff));

	/* setup tiling, simd, pipe config */
	mc_arb_ramcfg = rreg(MC_ARB_RAMCFG);

	shader_pipe_config = rreg(CC_GC_SHADER_PIPE_CONFIG);
	inactive_pipes = (shader_pipe_config & INACTIVE_QD_PIPES_MASK) >> INACTIVE_QD_PIPES_SHIFT;
	for (i = 0, tmp = 1, active_number = 0; i < R7XX_MAX_PIPES; i++) {
		if (!(inactive_pipes & tmp)) {
			active_number++;
		}
		tmp <<= 1;
	}
	if (active_number == 1) {
		wreg(SPI_CONFIG_CNTL, DISABLE_INTERP_1);
	} else {
		wreg(SPI_CONFIG_CNTL, 0);
	}

	cc_gc_shader_pipe_config = rreg(CC_GC_SHADER_PIPE_CONFIG) & 0xffffff00;
	tmp = spec.max_simds -
		__builtin_popcount((cc_gc_shader_pipe_config >> 16) & ((1u << spec.max_simds) - 1));
	spec.active_simds = tmp;

	switch (spec.max_tile_pipes) {
	case 1:
	default:
		gb_tiling_config = PIPE_TILING(0);
		break;
	case 2:
		gb_tiling_config = PIPE_TILING(1);
		break;
	case 4:
		gb_tiling_config = PIPE_TILING(2);
		break;
	case 8:
		gb_tiling_config = PIPE_TILING(3);
		break;
	}
	spec.tiling_npipes = spec.max_tile_pipes;

	disabled_rb_mask = (rreg(CC_RB_BACKEND_DISABLE) >> 16) & R7XX_MAX_BACKENDS_MASK;
	tmp = 0;
	for (i = 0; i < spec.max_backends; i++)
		tmp |= (1 << i);
	/* if all the backends are disabled, fix it up here */
	if ((disabled_rb_mask & tmp) == tmp) {
		for (i = 0; i < spec.max_backends; i++)
			disabled_rb_mask &= ~(1 << i);
	}
	tmp = (gb_tiling_config & PIPE_TILING__MASK) >> PIPE_TILING__SHIFT;
	tmp = remapBackend( tmp, spec.max_backends,
					R7XX_MAX_BACKENDS, disabled_rb_mask);
	gb_tiling_config |= tmp << 16;
	spec.backend_map = tmp;

	if (family == CHIP_RV770)
		gb_tiling_config |= BANK_TILING(1);
	else {
		if ((mc_arb_ramcfg & NOOFBANK_MASK) >> NOOFBANK_SHIFT)
			gb_tiling_config |= BANK_TILING(1);
		else
			gb_tiling_config |= BANK_TILING(0);
	}
	spec.tiling_nbanks = 4 << ((gb_tiling_config >> 4) & 0x3);
	gb_tiling_config |= GROUP_SIZE((mc_arb_ramcfg & BURSTLENGTH_MASK) >> BURSTLENGTH_SHIFT);
	if (((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT) > 3) {
		gb_tiling_config |= ROW_TILING(3);
		gb_tiling_config |= SAMPLE_SPLIT(3);
	} else {
		gb_tiling_config |=
			ROW_TILING(((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT));
		gb_tiling_config |=
			SAMPLE_SPLIT(((mc_arb_ramcfg & NOOFROWS_MASK) >> NOOFROWS_SHIFT));
	}

	gb_tiling_config |= BANK_SWAPS(1);
	spec.tile_config = gb_tiling_config;

	wreg(GB_TILING_CONFIG, gb_tiling_config);
	wreg(DCP_TILING_CONFIG, (gb_tiling_config & 0xffff));
	wreg(HDP_TILING_CONFIG, (gb_tiling_config & 0xffff));
	wreg(DMA_TILING_CONFIG, (gb_tiling_config & 0xffff));
	wreg(DMA_TILING_CONFIG2, (gb_tiling_config & 0xffff));
	if (family == CHIP_RV730) {
		wreg(UVD_UDEC_DB_TILING_CONFIG, (gb_tiling_config & 0xffff));
		wreg(UVD_UDEC_DBW_TILING_CONFIG, (gb_tiling_config & 0xffff));
		wreg(UVD_UDEC_TILING_CONFIG, (gb_tiling_config & 0xffff));
	}

	wreg(CGTS_SYS_TCC_DISABLE, 0);
	wreg(CGTS_TCC_DISABLE, 0);
	wreg(CGTS_USER_SYS_TCC_DISABLE, 0);
	wreg(CGTS_USER_TCC_DISABLE, 0);


	num_qd_pipes = R7XX_MAX_PIPES - __builtin_popcount((cc_gc_shader_pipe_config & INACTIVE_QD_PIPES_MASK) >> 8);
	wreg(VGT_OUT_DEALLOC_CNTL, (num_qd_pipes * 4) & DEALLOC_DIST_MASK);
	wreg(VGT_VERTEX_REUSE_BLOCK_CNTL, ((num_qd_pipes * 4) - 2) & VTX_REUSE_DEPTH_MASK);

	/* set HW defaults for 3D engine */
	wreg(CP_QUEUE_THRESHOLDS, (ROQ_IB1_START(0x16) |
				     ROQ_IB2_START(0x2b)));

	wreg(CP_MEQ_THRESHOLDS, STQ_SPLIT(0x30));

	ta_aux_cntl = rreg(TA_CNTL_AUX);
	wreg(TA_CNTL_AUX, ta_aux_cntl | DISABLE_CUBE_ANISO);

	sx_debug_1 = rreg(SX_DEBUG_1);
	sx_debug_1 |= ENABLE_NEW_SMX_ADDRESS;
	wreg(SX_DEBUG_1, sx_debug_1);

	smx_dc_ctl0 = rreg(SMX_DC_CTL0);
	smx_dc_ctl0 &= ~CACHE_DEPTH(0x1ff);
	smx_dc_ctl0 |= CACHE_DEPTH((spec.sx_num_of_sets * 64) - 1);
	wreg(SMX_DC_CTL0, smx_dc_ctl0);

	if (family != CHIP_RV740)
		wreg(SMX_EVENT_CTL, (ES_FLUSH_CTL(4) |
				       GS_FLUSH_CTL(4) |
				       ACK_FLUSH_CTL(3) |
				       SYNC_FLUSH_CTL));

	if (family != CHIP_RV770)
		wreg(SMX_SAR_CTL0, 0x00003f3f);

	db_debug3 = rreg(DB_DEBUG3);
	db_debug3 &= ~DB_CLK_OFF_DELAY(0x1f);
	switch (family) {
	case CHIP_RV770:
	case CHIP_RV740:
		db_debug3 |= DB_CLK_OFF_DELAY(0x1f);
		break;
	case CHIP_RV710:
	case CHIP_RV730:
	default:
		db_debug3 |= DB_CLK_OFF_DELAY(2);
		break;
	}
	wreg(DB_DEBUG3, db_debug3);

	if (family != CHIP_RV770) {
		db_debug4 = rreg(DB_DEBUG4);
		db_debug4 |= DISABLE_TILE_COVERED_FOR_PS_ITER;
		wreg(DB_DEBUG4, db_debug4);
	}

	wreg(SX_EXPORT_BUFFER_SIZES, (COLOR_BUFFER_SIZE((spec.sx_max_export_size / 4) - 1) |
					POSITION_BUFFER_SIZE((spec.sx_max_export_pos_size / 4) - 1) |
					SMX_BUFFER_SIZE((spec.sx_max_export_smx_size / 4) - 1)));

	wreg(PA_SC_FIFO_SIZE, (SC_PRIM_FIFO_SIZE(spec.sc_prim_fifo_size) |
				 SC_HIZ_TILE_FIFO_SIZE(spec.sc_hiz_tile_fifo_size) |
				 SC_EARLYZ_TILE_FIFO_SIZE(spec.sc_earlyz_tile_fifo_fize)));

	wreg(PA_SC_MULTI_CHIP_CNTL, 0);

	wreg(VGT_NUM_INSTANCES, 1);

	wreg(SPI_CONFIG_CNTL_1, VTX_DONE_DELAY(4));

	wreg(CP_PERFMON_CNTL, 0);

	sq_ms_fifo_sizes = (CACHE_FIFO_SIZE(16 * spec.sq_num_cf_insts) |
			    DONE_FIFO_HIWATER(0xe0) |
			    ALU_UPDATE_FIFO_HIWATER(0x8));
	switch (family) {
	case CHIP_RV770:
	case CHIP_RV730:
	case CHIP_RV710:
		sq_ms_fifo_sizes |= FETCH_FIFO_HIWATER(0x1);
		break;
	case CHIP_RV740:
	default:
		sq_ms_fifo_sizes |= FETCH_FIFO_HIWATER(0x4);
		break;
	}
	wreg(SQ_MS_FIFO_SIZES, sq_ms_fifo_sizes);

	/* SQ_CONFIG, SQ_GPR_RESOURCE_MGMT, SQ_THREAD_RESOURCE_MGMT, SQ_STACK_RESOURCE_MGMT
	 * should be adjusted as needed by the 2D/3D drivers.  This just sets default values
	 */
	sq_config = rreg(SQ_CONFIG);
	sq_config &= ~(PS_PRIO(3) |
		       VS_PRIO(3) |
		       GS_PRIO(3) |
		       ES_PRIO(3));
	sq_config |= (DX9_CONSTS |
		      VC_ENABLE |
		      EXPORT_SRC_C |
		      PS_PRIO(0) |
		      VS_PRIO(1) |
		      GS_PRIO(2) |
		      ES_PRIO(3));
	if (family == CHIP_RV710)
		/* no vertex cache */
		sq_config &= ~VC_ENABLE;

	wreg(SQ_CONFIG, sq_config);

	wreg(SQ_GPR_RESOURCE_MGMT_1,  (NUM_PS_GPRS((spec.max_gprs * 24)/64) |
					 NUM_VS_GPRS((spec.max_gprs * 24)/64) |
					 NUM_CLAUSE_TEMP_GPRS(((spec.max_gprs * 24)/64)/2)));

	wreg(SQ_GPR_RESOURCE_MGMT_2,  (NUM_GS_GPRS((spec.max_gprs * 7)/64) |
					 NUM_ES_GPRS((spec.max_gprs * 7)/64)));

	sq_thread_resource_mgmt = (NUM_PS_THREADS((spec.max_threads * 4)/8) |
				   NUM_VS_THREADS((spec.max_threads * 2)/8) |
				   NUM_ES_THREADS((spec.max_threads * 1)/8));
	if (((spec.max_threads * 1) / 8) > spec.max_gs_threads)
		sq_thread_resource_mgmt |= NUM_GS_THREADS(spec.max_gs_threads);
	else
		sq_thread_resource_mgmt |= NUM_GS_THREADS((spec.max_gs_threads * 1)/8);
	wreg(SQ_THREAD_RESOURCE_MGMT, sq_thread_resource_mgmt);

	wreg(SQ_STACK_RESOURCE_MGMT_1, (NUM_PS_STACK_ENTRIES((spec.max_stack_entries * 1)/4) |
						     NUM_VS_STACK_ENTRIES((spec.max_stack_entries * 1)/4)));

	wreg(SQ_STACK_RESOURCE_MGMT_2, (NUM_GS_STACK_ENTRIES((spec.max_stack_entries * 1)/4) |
						     NUM_ES_STACK_ENTRIES((spec.max_stack_entries * 1)/4)));

	sq_dyn_gpr_size_simd_ab_0 = (SIMDA_RING0((spec.max_gprs * 38)/64) |
				     SIMDA_RING1((spec.max_gprs * 38)/64) |
				     SIMDB_RING0((spec.max_gprs * 38)/64) |
				     SIMDB_RING1((spec.max_gprs * 38)/64));

	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_0, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_1, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_2, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_3, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_4, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_5, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_6, sq_dyn_gpr_size_simd_ab_0);
	wreg(SQ_DYN_GPR_SIZE_SIMD_AB_7, sq_dyn_gpr_size_simd_ab_0);

	wreg(PA_SC_FORCE_EOV_MAX_CNTS, (FORCE_EOV_MAX_CLK_CNT(4095) |
					  FORCE_EOV_MAX_REZ_CNT(255)));

	if (family == CHIP_RV710)
		wreg(VGT_CACHE_INVALIDATION, (CACHE_INVALIDATION(TC_ONLY) |
						AUTO_INVLD_EN(ES_AND_GS_AUTO)));
	else
		wreg(VGT_CACHE_INVALIDATION, (CACHE_INVALIDATION(VC_AND_TC) |
						AUTO_INVLD_EN(ES_AND_GS_AUTO)));

	switch (family) {
	case CHIP_RV770:
	case CHIP_RV730:
	case CHIP_RV740:
		gs_prim_buffer_depth = 384;
		break;
	case CHIP_RV710:
		gs_prim_buffer_depth = 128;
		break;
	default:
		break;
	}

	num_gs_verts_per_thread = spec.max_pipes * 16;
	vgt_gs_per_es = gs_prim_buffer_depth + num_gs_verts_per_thread;
	/* Max value for this is 256 */
	if (vgt_gs_per_es > 256)
		vgt_gs_per_es = 256;

	wreg(VGT_ES_PER_GS, 128);
	wreg(VGT_GS_PER_ES, vgt_gs_per_es);
	wreg(VGT_GS_PER_VS, 2);

	/* more default values. 2D/3D driver should adjust as needed */
	wreg(VGT_GS_VERTEX_REUSE, 16);
	wreg(PA_SC_LINE_STIPPLE_STATE, 0);
	wreg(VGT_STRMOUT_EN, 0);
	wreg(SX_MISC, 0);
	wreg(PA_SC_MODE_CNTL, 0);
	wreg(PA_SC_EDGERULE, 0xaaaaaaaa);
	wreg(PA_SC_AA_CONFIG, 0);
	wreg(PA_SC_CLIPRECT_RULE, 0xffff);
	wreg(PA_SC_LINE_STIPPLE, 0);
	wreg(SPI_INPUT_Z, 0);
	wreg(SPI_PS_IN_CONTROL_0, NUM_INTERP(2));
	wreg(CB_COLOR7_FRAG, 0);

	/* clear render buffer base addresses */
	wreg(CB_COLOR0_BASE, 0);
	wreg(CB_COLOR1_BASE, 0);
	wreg(CB_COLOR2_BASE, 0);
	wreg(CB_COLOR3_BASE, 0);
	wreg(CB_COLOR4_BASE, 0);
	wreg(CB_COLOR5_BASE, 0);
	wreg(CB_COLOR6_BASE, 0);
	wreg(CB_COLOR7_BASE, 0);

	wreg(TCP_CNTL, 0);

	hdp_host_path_cntl = rreg(HDP_HOST_PATH_CNTL);
	wreg(HDP_HOST_PATH_CNTL, hdp_host_path_cntl);

	wreg(PA_SC_MULTI_CHIP_CNTL, 0);

	wreg(PA_CL_ENHANCE, (CLIP_VTX_REORDER_ENA |
					  NUM_CLIP_SEQ(3)));
	wreg(VC_ENHANCE, 0);

    gpuTiling = spec.tile_config;
    gpuBackendMap = spec.backend_map;
    gpuActiveSimds = spec.active_simds;
    LOG("GPU initialized: tiling=0x%08x backend map=0x%x simds=%u shader config=0x%08x\n", gpuTiling, gpuBackendMap, gpuActiveSimds, rreg(CC_GC_SHADER_PIPE_CONFIG));
}
