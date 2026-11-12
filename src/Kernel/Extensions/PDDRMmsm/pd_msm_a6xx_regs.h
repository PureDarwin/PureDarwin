// pd_msm_a6xx_regs.h - the registers the a690 bring-up programs, read back by the pdmsmregs dump
// and by tools/a690-capture on linux, so both sides print the same list
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
//
// REG(window, offset, name): offset is in bytes from the window's start. GMU offsets are from
// gpu word GMU_WINDOW_WORD (0x1a800). SMMU_CB0 and SMMU_CB1 are the first two context banks,
// wherever IDR1 puts them. GX registers need the gpu powered, see the tool's --gx.

#ifndef PD_MSM_A6XX_REGS_H
#define PD_MSM_A6XX_REGS_H

enum a6xx_reg_window {
    W_GPU, W_GMU, W_RSCC, W_PDC, W_SMMU_GR0, W_SMMU_GR1, W_SMMU_CB0, W_SMMU_CB1, W_GPUCC, W_GCC,
    W_COUNT
};

#define A6XX_REG_WINDOW_NAMES \
    "gpu", "gmu", "rscc", "pdc", "smmu_gr0", "smmu_gr1", "smmu_cb0", "smmu_cb1", "gpucc", "gcc"

#define GW(word)    ((word) * 4)
#define MW(word)    (((word) - 0x1a800) * 4)

#define A6XX_REGS(REG) \
    /* gx: rbbm, cp, uche, gbif */ \
    REG(W_GPU, GW(0x0011), "rbbm_secvid_tsb_cntl") \
    REG(W_GPU, GW(0x0016), "rbbm_gbif_halt") \
    REG(W_GPU, GW(0x001f), "rbbm_interface_hang_int_cntl") \
    REG(W_GPU, GW(0x0038), "rbbm_int_0_mask") \
    REG(W_GPU, GW(0x0042), "rbbm_0x0042") \
    REG(W_GPU, GW(0x0044), "rbbm_0x0044") \
    REG(W_GPU, GW(0x00ae), "rbbm_clock_cntl") \
    REG(W_GPU, GW(0x00b0), "hwcg_0x00b0") REG(W_GPU, GW(0x00b4), "hwcg_0x00b4") \
    REG(W_GPU, GW(0x00b8), "hwcg_0x00b8") REG(W_GPU, GW(0x00bc), "hwcg_0x00bc") \
    REG(W_GPU, GW(0x00c0), "hwcg_0x00c0") REG(W_GPU, GW(0x00c4), "hwcg_0x00c4") \
    REG(W_GPU, GW(0x00c8), "hwcg_0x00c8") REG(W_GPU, GW(0x00cc), "hwcg_0x00cc") \
    REG(W_GPU, GW(0x00d0), "hwcg_0x00d0") REG(W_GPU, GW(0x00d4), "hwcg_0x00d4") \
    REG(W_GPU, GW(0x00d8), "hwcg_0x00d8") REG(W_GPU, GW(0x00dc), "hwcg_0x00dc") \
    REG(W_GPU, GW(0x00e0), "hwcg_0x00e0") REG(W_GPU, GW(0x00e4), "hwcg_0x00e4") \
    REG(W_GPU, GW(0x00e8), "hwcg_0x00e8") REG(W_GPU, GW(0x00ec), "hwcg_0x00ec") \
    REG(W_GPU, GW(0x00f0), "hwcg_0x00f0") REG(W_GPU, GW(0x00f4), "hwcg_0x00f4") \
    REG(W_GPU, GW(0x00f8), "hwcg_0x00f8") REG(W_GPU, GW(0x0100), "hwcg_0x0100") \
    REG(W_GPU, GW(0x0104), "hwcg_0x0104") REG(W_GPU, GW(0x0105), "hwcg_0x0105") \
    REG(W_GPU, GW(0x0106), "hwcg_0x0106") REG(W_GPU, GW(0x0107), "hwcg_0x0107") \
    REG(W_GPU, GW(0x0108), "hwcg_0x0108") REG(W_GPU, GW(0x0109), "hwcg_0x0109") \
    REG(W_GPU, GW(0x010a), "hwcg_0x010a") REG(W_GPU, GW(0x010b), "hwcg_0x010b") \
    REG(W_GPU, GW(0x010f), "hwcg_0x010f") REG(W_GPU, GW(0x0110), "hwcg_0x0110") \
    REG(W_GPU, GW(0x0111), "hwcg_0x0111") REG(W_GPU, GW(0x0112), "hwcg_0x0112") \
    REG(W_GPU, GW(0x0113), "hwcg_0x0113") REG(W_GPU, GW(0x0114), "hwcg_0x0114") \
    REG(W_GPU, GW(0x0115), "hwcg_0x0115") REG(W_GPU, GW(0x0116), "hwcg_0x0116") \
    REG(W_GPU, GW(0x0117), "hwcg_0x0117") REG(W_GPU, GW(0x0118), "hwcg_0x0118") \
    REG(W_GPU, GW(0x0119), "hwcg_0x0119") REG(W_GPU, GW(0x011a), "hwcg_0x011a") \
    REG(W_GPU, GW(0x011b), "hwcg_0x011b") REG(W_GPU, GW(0x011c), "hwcg_0x011c") \
    REG(W_GPU, GW(0x011d), "hwcg_0x011d") REG(W_GPU, GW(0x0120), "hwcg_0x0120") \
    REG(W_GPU, GW(0x0121), "hwcg_0x0121") REG(W_GPU, GW(0x0122), "hwcg_0x0122") \
    REG(W_GPU, GW(0x0210), "rbbm_status") \
    REG(W_GPU, GW(0x0500), "rbbm_0x0500") \
    REG(W_GPU, GW(0x050b), "rbbm_0x050b") \
    REG(W_GPU, GW(0x0533), "rbbm_0x0533") \
    REG(W_GPU, GW(0x0534), "rbbm_0x0534") \
    REG(W_GPU, GW(0x0802), "cp_rb_cntl") \
    REG(W_GPU, GW(0x0808), "cp_sqe_cntl") \
    REG(W_GPU, GW(0x0841), "cp_0x0841") \
    REG(W_GPU, GW(0x0842), "addr_mode_0x0842") \
    REG(W_GPU, GW(0x0844), "cp_0x0844") \
    REG(W_GPU, GW(0x084f), "cp_protect_cntl") \
    REG(W_GPU, GW(0x0850), "cp_protect_0")  REG(W_GPU, GW(0x0851), "cp_protect_1") \
    REG(W_GPU, GW(0x0852), "cp_protect_2")  REG(W_GPU, GW(0x0853), "cp_protect_3") \
    REG(W_GPU, GW(0x0854), "cp_protect_4")  REG(W_GPU, GW(0x0855), "cp_protect_5") \
    REG(W_GPU, GW(0x0856), "cp_protect_6")  REG(W_GPU, GW(0x0857), "cp_protect_7") \
    REG(W_GPU, GW(0x0858), "cp_protect_8")  REG(W_GPU, GW(0x0859), "cp_protect_9") \
    REG(W_GPU, GW(0x085a), "cp_protect_10") REG(W_GPU, GW(0x085b), "cp_protect_11") \
    REG(W_GPU, GW(0x085c), "cp_protect_12") REG(W_GPU, GW(0x085d), "cp_protect_13") \
    REG(W_GPU, GW(0x085e), "cp_protect_14") REG(W_GPU, GW(0x085f), "cp_protect_15") \
    REG(W_GPU, GW(0x0860), "cp_protect_16") REG(W_GPU, GW(0x0861), "cp_protect_17") \
    REG(W_GPU, GW(0x0862), "cp_protect_18") REG(W_GPU, GW(0x0863), "cp_protect_19") \
    REG(W_GPU, GW(0x0864), "cp_protect_20") REG(W_GPU, GW(0x0865), "cp_protect_21") \
    REG(W_GPU, GW(0x0866), "cp_protect_22") REG(W_GPU, GW(0x0867), "cp_protect_23") \
    REG(W_GPU, GW(0x0868), "cp_protect_24") REG(W_GPU, GW(0x0869), "cp_protect_25") \
    REG(W_GPU, GW(0x086a), "cp_protect_26") REG(W_GPU, GW(0x086b), "cp_protect_27") \
    REG(W_GPU, GW(0x086c), "cp_protect_28") REG(W_GPU, GW(0x086d), "cp_protect_29") \
    REG(W_GPU, GW(0x086e), "cp_protect_30") REG(W_GPU, GW(0x086f), "cp_protect_31") \
    REG(W_GPU, GW(0x0870), "cp_protect_32") REG(W_GPU, GW(0x0871), "cp_protect_33") \
    REG(W_GPU, GW(0x087f), "cp_protect_last") \
    REG(W_GPU, GW(0x08c1), "cp_0x08c1") REG(W_GPU, GW(0x08c2), "cp_0x08c2") \
    REG(W_GPU, GW(0x08c3), "cp_0x08c3") REG(W_GPU, GW(0x08d0), "cp_0x08d0") \
    REG(W_GPU, GW(0x098d), "vsc_0x098d") \
    REG(W_GPU, GW(0x0b34), "0x0b34") \
    REG(W_GPU, GW(0x0c01), "addr_mode_0x0c01") \
    REG(W_GPU, GW(0x0e00), "addr_mode_0x0e00") \
    REG(W_GPU, GW(0x0e01), "uche_0x0e01") \
    REG(W_GPU, GW(0x0e05), "uche_0x0e05_lo") REG(W_GPU, GW(0x0e06), "uche_0x0e05_hi") \
    REG(W_GPU, GW(0x0e07), "uche_0x0e07_lo") REG(W_GPU, GW(0x0e08), "uche_0x0e07_hi") \
    REG(W_GPU, GW(0x0e09), "uche_0x0e09_lo") REG(W_GPU, GW(0x0e0a), "uche_0x0e09_hi") \
    REG(W_GPU, GW(0x0e17), "uche_0x0e17") REG(W_GPU, GW(0x0e18), "uche_0x0e18") \
    REG(W_GPU, GW(0x0e19), "uche_0x0e19") REG(W_GPU, GW(0x0e3c), "uche_0x0e3c") \
    REG(W_GPU, GW(0x3c01), "gbif_scache_cntl0") REG(W_GPU, GW(0x3c02), "gbif_scache_cntl1") \
    REG(W_GPU, GW(0x3c03), "gbif_qsb_side0") REG(W_GPU, GW(0x3c04), "gbif_qsb_side1") \
    REG(W_GPU, GW(0x3c05), "gbif_qsb_side2") REG(W_GPU, GW(0x3c06), "gbif_qsb_side3") \
    REG(W_GPU, GW(0x3c45), "gbif_halt") \
    REG(W_GPU, GW(0x8601), "addr_mode_0x8601") REG(W_GPU, GW(0x8e05), "addr_mode_0x8e05") \
    REG(W_GPU, GW(0x8e08), "0x8e08") \
    REG(W_GPU, GW(0x9601), "addr_mode_0x9601") REG(W_GPU, GW(0x9e00), "0x9e00") \
    REG(W_GPU, GW(0x9e01), "addr_mode_0x9e01") \
    REG(W_GPU, GW(0xa601), "addr_mode_0xa601") REG(W_GPU, GW(0xae01), "addr_mode_0xae01") \
    REG(W_GPU, GW(0xae02), "0xae02") \
    REG(W_GPU, GW(0xb601), "addr_mode_0xb601") REG(W_GPU, GW(0xb604), "0xb604") \
    REG(W_GPU, GW(0xb608), "bicubic_0") REG(W_GPU, GW(0xb609), "bicubic_1") \
    REG(W_GPU, GW(0xb60a), "bicubic_2") REG(W_GPU, GW(0xb60b), "bicubic_3") \
    REG(W_GPU, GW(0xb60c), "bicubic_4") \
    REG(W_GPU, GW(0xbe05), "addr_mode_0xbe05") \
    REG(W_GPU, GW(0xf800), "0xf800") REG(W_GPU, GW(0xf801), "0xf801") \
    REG(W_GPU, GW(0xf802), "0xf802") REG(W_GPU, GW(0xf803), "0xf803") \
    REG(W_GPU, GW(0xf810), "addr_mode_0xf810") \
    /* cx: gmu */ \
    REG(W_GMU, MW(0x1a880), "gmu_gx_sptprac_clock_control") \
    REG(W_GMU, MW(0x1d3f8), "gmu_core_fw_version") \
    REG(W_GMU, MW(0x1f400), "gmu_icache_config") \
    REG(W_GMU, MW(0x1f401), "gmu_dcache_config") \
    REG(W_GMU, MW(0x1f40f), "gmu_sys_bus_config") \
    REG(W_GMU, MW(0x1f801), "gmu_cm3_boot_config") \
    REG(W_GMU, MW(0x1f81c), "gmu_cm3_fw_init_result") \
    REG(W_GMU, MW(0x1f82d), "gmu_cm3_cfg") \
    REG(W_GMU, MW(0x1f840), "gmu_cx_power_counter_enable") \
    REG(W_GMU, MW(0x1f841), "gmu_cx_power_counter_select_0") \
    REG(W_GMU, MW(0x1f8c0), "gmu_pwr_col_inter_frame_ctrl") \
    REG(W_GMU, MW(0x1f8e8), "gmu_rpmh_ctrl") \
    REG(W_GMU, MW(0x1f8f0), "gmu_cx_fal_intf") \
    REG(W_GMU, MW(0x1f8f1), "gmu_cx_falnext_intf") \
    REG(W_GMU, MW(0x1f900), "gmu_pwr_col_cp_msg") \
    REG(W_GMU, MW(0x1f980), "gmu_hfi_ctrl_status") \
    REG(W_GMU, MW(0x1f982), "gmu_hfi_sfr_addr") \
    REG(W_GMU, MW(0x1f984), "gmu_hfi_qtbl_info") \
    REG(W_GMU, MW(0x1f985), "gmu_hfi_qtbl_addr") \
    REG(W_GMU, MW(0x1f993), "gmu_gmu2host_intr_mask") \
    REG(W_GMU, MW(0x1f9cc), "gmu_general_7") \
    REG(W_GMU, MW(0x23b06), "gmu_ao_host_interrupt_mask") \
    REG(W_GMU, MW(0x23b09), "gmu_ao_cgc_mode_cntl") \
    REG(W_GMU, MW(0x23b0a), "gmu_ao_cgc_delay_cntl") \
    REG(W_GMU, MW(0x23b0b), "gmu_ao_cgc_hyst_cntl") \
    REG(W_GMU, MW(0x23b0e), "gmu_ao_gpu_cx_busy_mask") \
    REG(W_GMU, MW(0x23b11), "gmu_ahb_fence_range_0") \
    /* cx: rscc and pdc, as gmu_rscc_init writes them */ \
    REG(W_RSCC, 0x010, "rscc_0x010") REG(W_RSCC, 0x020, "rscc_0x020") \
    REG(W_RSCC, 0x024, "rscc_0x024") REG(W_RSCC, 0x028, "rscc_0x028") \
    REG(W_RSCC, 0x02c, "rscc_0x02c") REG(W_RSCC, 0x034, "rscc_0x034") \
    REG(W_RSCC, 0x038, "rscc_0x038") REG(W_RSCC, 0x03c, "rscc_0x03c") \
    REG(W_RSCC, 0x040, "rscc_0x040") REG(W_RSCC, 0x044, "rscc_0x044") \
    REG(W_RSCC, 0x048, "rscc_0x048") REG(W_RSCC, 0x400, "rscc_0x400") \
    REG(W_RSCC, 0x600, "rscc_seq_0") REG(W_RSCC, 0x604, "rscc_seq_1") \
    REG(W_RSCC, 0x608, "rscc_seq_2") REG(W_RSCC, 0x60c, "rscc_seq_3") \
    REG(W_RSCC, 0x610, "rscc_seq_4") \
    REG(W_PDC, 0x4500, "pdc_0x4500") REG(W_PDC, 0x4520, "pdc_0x4520") \
    /* the adreno smmu: globals, the first four streams, banks 0 and 1 */ \
    REG(W_SMMU_GR0, 0x000, "smmu_scr0") REG(W_SMMU_GR0, 0x010, "smmu_sacr") \
    REG(W_SMMU_GR0, 0x020, "smmu_idr0") REG(W_SMMU_GR0, 0x024, "smmu_idr1") \
    REG(W_SMMU_GR0, 0x028, "smmu_idr2") \
    REG(W_SMMU_GR0, 0x800, "smmu_smr0") REG(W_SMMU_GR0, 0x804, "smmu_smr1") \
    REG(W_SMMU_GR0, 0x808, "smmu_smr2") REG(W_SMMU_GR0, 0x80c, "smmu_smr3") \
    REG(W_SMMU_GR0, 0xc00, "smmu_s2cr0") REG(W_SMMU_GR0, 0xc04, "smmu_s2cr1") \
    REG(W_SMMU_GR0, 0xc08, "smmu_s2cr2") REG(W_SMMU_GR0, 0xc0c, "smmu_s2cr3") \
    REG(W_SMMU_GR1, 0x000, "smmu_cbar0") REG(W_SMMU_GR1, 0x004, "smmu_cbar1") \
    REG(W_SMMU_GR1, 0x800, "smmu_cba2r0") REG(W_SMMU_GR1, 0x804, "smmu_cba2r1") \
    REG(W_SMMU_CB0, 0x000, "cb0_sctlr") REG(W_SMMU_CB0, 0x004, "cb0_actlr") \
    REG(W_SMMU_CB0, 0x010, "cb0_tcr2") REG(W_SMMU_CB0, 0x030, "cb0_tcr") \
    REG(W_SMMU_CB0, 0x038, "cb0_mair0") REG(W_SMMU_CB0, 0x03c, "cb0_mair1") \
    REG(W_SMMU_CB1, 0x000, "cb1_sctlr") REG(W_SMMU_CB1, 0x004, "cb1_actlr") \
    REG(W_SMMU_CB1, 0x010, "cb1_tcr2") REG(W_SMMU_CB1, 0x030, "cb1_tcr") \
    REG(W_SMMU_CB1, 0x038, "cb1_mair0") REG(W_SMMU_CB1, 0x03c, "cb1_mair1") \
    /* clocks and the cx gdsc */ \
    REG(W_GPUCC, 0x106c, "gpucc_cx_gdscr") REG(W_GPUCC, 0x1540, "gpucc_cx_gdsc_status") \
    REG(W_GPUCC, 0x1078, "gpucc_ahb_cbcr") REG(W_GPUCC, 0x1098, "gpucc_cx_gmu_cbcr") \
    REG(W_GPUCC, 0x1120, "gpucc_gmu_rcg_cmd") REG(W_GPUCC, 0x1124, "gpucc_gmu_rcg_cfg") \
    REG(W_GPUCC, 0x1178, "gpucc_hub_aon_cbcr") \
    REG(W_GPUCC, 0x117c, "gpucc_hub_rcg_cmd") REG(W_GPUCC, 0x1180, "gpucc_hub_rcg_cfg") \
    REG(W_GPUCC, 0x1204, "gpucc_hub_cx_int_cbcr") \
    REG(W_GPUCC, 0x5000, "gpucc_hlos1_vote_smmu_cbcr") \
    REG(W_GCC, 0x52000, "gcc_gpll0_vote") \
    REG(W_GCC, 0x71010, "gcc_gpu_memnoc_gfx_cbcr") \
    REG(W_GCC, 0x71020, "gcc_gpu_snoc_dvm_gfx_cbcr") \
    REG(W_GCC, 0x7115c, "gcc_ddrss_gpu_axi_cbcr")

#endif
