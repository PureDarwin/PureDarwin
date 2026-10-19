A690 on SC8280XP, bring-up spec, version 0


What this is

This spec describes what is known about taking the Adreno 690 in the Qualcomm Snapdragon 8cx Gen 3 (SC8280XP, as in the Lenovo ThinkPad X13s) from power off to executing command buffers, for the PDDRMmsm kernel extension.

Status on 19 October 2026: desk only. Nobody in the project has observed this hardware yet. Everything here was written from the allowed sources listed in README.md, without running anything on an 8cx. Where the allowed sources are silent, this spec says so and moves the question to the observation plan in section 8 or to the open items at the end. Nothing in the body of this spec should be implemented as fact unless it carries a citation. Version 1 of this spec is the one that has been checked against the observations.

Roles for this spec. The observer and spec writer is a language model, which wrote this version from the allowed sources and will turn the observations into version 1. The driver is implemented by a person, TheNightman, who also runs the observation plan on the 8cx. This spec describes behaviour and values, never code, and it contains no driver code.

Every value cites its source with one of these tags:

- [DEV] is Mesa's device table, src/freedreno/common/freedreno_devices.py, and the matching rules in src/freedreno/common/freedreno_dev_info.h and freedreno_dev_info.c (MIT).
- [XML a6xx.xml], [XML a6xx_gmu.xml] and [XML pm4] are the freedreno register database in Mesa's src/freedreno/registers/adreno/ (MIT). [XML pm4] is adreno_pm4.xml.
- [PM4H] is Mesa's src/freedreno/common/freedreno_pm4.h (MIT), the packet header helpers.
- [QRISC] is Mesa's src/freedreno/qrisc/README.rst (MIT), the documentation of the CP microcontroller and its firmware (the tool used to be called afuc).
- [FDDOC] is Mesa's docs/drivers/freedreno.rst (MIT), which documents the SQE, the devcoredump, the rd capture and the decode tools.
- [CRASHDEC] is Mesa's src/freedreno/decode/crashdec.c, crashdec.h and crashdec-hfi.c (MIT), the devcoredump decoder, which knows the layout of the HFI queues.
- [MESA] is Mesa's src/freedreno/drm/msm/*.c (MIT), as in msm-uapi.md.
- [DT] is the mainline device tree, arch/arm64/boot/dts/qcom/sc8280xp.dtsi and sc8280xp-lenovo-thinkpad-x13s.dts. Both files carry "SPDX-License-Identifier: BSD-3-Clause" on their first line, which was checked before they were read.
- [WHENCE] is linux-firmware's WHENCE file, used only for file names, versions and licence lines.
- [ARM-SMMU] is the ARM System MMU architecture specification, version 2 (public). The SMMU-500 implements it.
- [CALC] is arithmetic on cited values, done in this spec. The inputs are cited next to it.


1. Identity [DEV, MESA]

Mesa's device table has one entry for this GPU, listed under two ids:

    GPUId(690)
    GPUId(chip_id=0xffff06090000, name="FD690")    (the default no-speedbin fallback)
    generation        CHIP.A6XX, with the a6xx_base and a6xx_gen4 property sets
    num_ccu           8
    num_vsc_pipes     32
    highest_bank_bit  16
    tile_align_w/h    64 / 32
    tile_max_w/h      1024 / 1024

The chip id is matched like this [DEV freedreno_dev_info.c]. The chip id is a 64-bit value with core, major, minor and patch in bytes 3, 2, 1 and 0, and a fuse or speed bin id in bits 47:32. A table entry whose bits 47:32 are 0xffff matches any fuse id. So the entry above matches a kernel CHIP_ID whose low 32 bits are 0x06090000 (core 6, major 9, minor 0, patch 0), whatever the upper bits say. Mesa also matches on GPU_ID 690 alone. When the kernel reports no GPU_ID, Mesa derives one from the chip id as core * 100 + major * 10 + minor [DEV freedreno_dev_info.h], which also gives 690.

What pd_msm_info needs, and where each value comes from:

    name          "a690"
    gpu_id        690 [DEV]
    chip_id       0x06090000 is enough for Mesa to match [DEV]. The exact value Linux reports, including
                  the patch byte and bits 47:32, is to be observed (section 8, step 1).
    gmem_size     not in Mesa's table for a6xx. Mesa reads it from the kernel (MSM_PARAM_GMEM_SIZE) [MESA
                  msm_pipe.c]. To be observed.
    gmem_base     to be observed (MSM_PARAM_GMEM_BASE).
    max_freq      690000000 is the highest GPU operating point in the device tree [DT gpu_opp_table].
                  Whether Linux reports that value is to be observed.
    core_clk      none. The GPU node in the device tree has no clocks property. Every clock is listed on
                  the GMU node instead (section 2). How the GPU frequency is set is an open item.
    va_start/end  to be observed (MSM_PARAM_VA_START and MSM_PARAM_VA_SIZE). The a6xx is a 64-bit GPU
                  (Mesa treats generation 5 and up as 64-bit [DEV fd_dev_64b]).
    funcs         a new a6xx generation, described in sections 4 to 6.

msm-uapi.md was written for the A306. The parameters this GPU answers differently (for example VA_START, VA_SIZE, UCHE_TRAP_BASE, HIGHEST_BANK_BIT and the UBWC parameters, which return EINVAL on the A306) are to be recorded with msmparams in section 8, step 1, and added to msm-uapi.md as an a690 column.


2. Resources from the device tree [DT sc8280xp.dtsi]

The GPU node, gpu@3d00000:

    compatible        "qcom,adreno-690.0", "qcom,adreno"
    reg               0x03d00000 size 0x40000  "kgsl_3d0_reg_memory"   (the GPU registers)
                      0x03d9e000 size 0x1000   "cx_mem"
                      0x03d61000 size 0x800    "cx_dbgc"
    interrupts        GIC SPI 300, level high
    iommus            <&gpu_smmu 0 0xc00>, <&gpu_smmu 1 0xc00>   (stream id 0 and 1, mask 0xc00)
    qcom,gmu          <&gmu>
    interconnects     gem_noc MASTER_GFX3D to mc_virt SLAVE_EBI1, named "gfx-mem"
    zap-shader        memory-region pil_gpu_mem: 0x8bf00000 size 0x2000, no-map
    clocks            none
    power-domains     none

The GPU operating points, each with its RPMh level and the peak memory bandwidth vote:

    270 MHz   LOW_SVS    451000 kBps
    410 MHz   SVS        1555000 kBps
    500 MHz   SVS_L1     1555000 kBps
    547 MHz   SVS_L2     1555000 kBps
    606 MHz   NOM        2736000 kBps
    640 MHz   NOM_L1     2736000 kBps
    655 MHz   TURBO      2736000 kBps
    690 MHz   TURBO_L1   2736000 kBps

The GMU node, gmu@3d6a000. The GMU is the graphics management unit, a small processor next to the GPU that owns its power sequencing.

    compatible        "qcom,adreno-gmu-690.0", "qcom,adreno-gmu"
    reg               0x03d6a000 size 0x34000  "gmu"
                      0x03de0000 size 0x10000  "rscc"
                      0x0b290000 size 0x10000  "gmu_pdc"
    interrupts        GIC SPI 304 "hfi", GIC SPI 305 "gmu", both level high
    clocks            gpucc GPU_CC_CX_GMU_CLK              "gmu"
                      gpucc GPU_CC_CXO_CLK                 "cxo"
                      gcc   GCC_DDRSS_GPU_AXI_CLK          "axi"
                      gcc   GCC_GPU_MEMNOC_GFX_CLK         "memnoc"
                      gpucc GPU_CC_AHB_CLK                 "ahb"
                      gpucc GPU_CC_HUB_CX_INT_CLK          "hub"
                      gpucc GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK "smmu_vote"
    power-domains     gpucc GPU_CC_CX_GDSC "cx", gpucc GPU_CC_GX_GDSC "gx"
    iommus            <&gpu_smmu 5 0xc00>
    GMU operating points  200 MHz at MIN_SVS, 500 MHz at SVS

The GPU clock controller, clock-controller@3d90000:

    compatible        "qcom,sc8280xp-gpucc"
    reg               0x03d90000 size 0x9000
    parent clocks     rpmhcc RPMH_CXO_CLK "bi_tcxo", gcc GCC_GPU_GPLL0_CLK_SRC, gcc GCC_GPU_GPLL0_DIV_CLK_SRC
    power-domains     rpmhpd SC8280XP_GFX
    provides          clocks, resets and power domains

The GPU SMMU, iommu@3da0000:

    compatible        "qcom,sc8280xp-smmu-500", "qcom,adreno-smmu", "qcom,smmu-500", "arm,mmu-500"
    reg               0x03da0000 size 0x20000
    #iommu-cells      2   (stream id, mask)
    #global-interrupts 2
    interrupts        GIC SPI 672 and 673 (global), then SPI 678 to 689 (12 context interrupts), all level high
    clocks            gcc GCC_GPU_MEMNOC_GFX_CLK, gcc GCC_GPU_SNOC_DVM_GFX_CLK, gpucc GPU_CC_AHB_CLK,
                      gpucc GPU_CC_HLOS1_VOTE_GPU_SMMU_CLK, gpucc GPU_CC_CX_GMU_CLK,
                      gpucc GPU_CC_HUB_CX_INT_CLK, gpucc GPU_CC_HUB_AON_CLK
    power-domains     gpucc GPU_CC_CX_GDSC
    dma-coherent

For PDDRMmsm this means the IOMMU type for this GPU is chosen by one of the compatible strings above (README.md, Adding a generation). The MSM8916's qcom,msm-iommu-v1 does not apply.

The clock and power domain names come from the device tree. Their register offsets inside gpucc and gcc, and the sequence that turns them on, are not in any allowed source. They are to be observed (section 8, steps 3 and 4).


3. Register address spaces [XML a6xx.xml, XML a6xx_gmu.xml, CRASHDEC, CALC]

Register offsets in the XML are in 32-bit words. A GPU register at offset N is at byte address 0x03d00000 + 4 * N [CALC from DT].

a6xx_gmu.xml gives the GMU registers in the same word space as the GPU registers, starting at 0x1a800 and up. The CPU address of a GMU register at offset N is again 0x03d00000 + 4 * N [CALC]. For example:

    GMU_CM3_SYSRESET   0x1f800   0x03d7e000
    GMU_HFI_CTRL_INIT  0x1f986   0x03d7e618
    GMU_AO_INTERRUPT_EN 0x23b03  0x03d8ec0c
    GPU_CC_GX_GDSCR    0x24403   0x03d9100c

The first three fall inside the DT "gmu" region (0x03d6a000 to 0x03d9e000), whose start is GPU word offset 0x1a800 [CALC]. crashdec agrees: it adds 0x1a800 to GMU register offsets taken from devcoredumps of kernels older than 6.19, which dumped them relative to the GMU region [CRASHDEC crashdec.c]. GPU_CC_GX_GDSCR falls inside the gpucc region at 0x03d90000.

The RSCC registers in a6xx_gmu.xml are relative offsets (the XML says they start "at offset 0x8c00 on most gpus"). On this chip the DT gives the RSCC its own region at 0x03de0000. Which base those offsets are relative to here is to be observed.

cx_mem, cx_dbgc and gmu_pdc are named by the DT, but no allowed source describes their contents.


4. The a6xx command processor [XML a6xx.xml, XML pm4, PM4H, QRISC]

4.1 What changes from a3xx

On a3xx the command processor was a prefetch parser (PFP) and a micro engine (ME), each with microcode written word by word through a data port (a306 spec, section 4). From a6xx on, both are replaced by a single microcontroller called the SQE [FDDOC, QRISC]. Starting with Adreno 660, the SQE firmware also carries the firmware for a second, cut-down processor called LPAC (low priority asynchronous compute), and the SQE boots LPAC [QRISC]. The a690 has LPAC (has_lpac in the a6xx_gen4 property set [DEV]).

The SQE runs its microcode from memory. CP_SQE_INSTR_BASE is a 64-bit register that holds an address [XML a6xx.xml], so the firmware is placed in a buffer that the GPU can read and its address is written there, instead of being written through a port.

The firmware layout [QRISC]: the instruction set is QRisc. A QRisc NOP has 0x01 in its high 8 bits on a6xx, and its low 24 bits are ignored. The first two instructions of a firmware typically hold the firmware id and version, and the offset of the packet handling table, encoded as NOPs. The bootstrap routine at the start of the firmware reads them as data and builds its jump table. Whether the file has a header word ahead of the first instruction (like word 0 of a300_pm4.fw), and so which byte of the file CP_SQE_INSTR_BASE must point at, is to be observed (section 8, step 6).

4.2 Registers for the ring and the SQE

The A6XX offsets, from the entries marked A6XX or A6XX-A7XX (entries marked A8XX- are a different layout and do not apply):

    CP_RB_BASE           0x0800  64-bit
    CP_RB_CNTL           0x0802
    CP_RB_RPTR_ADDR      0x0804  64-bit
    CP_RB_RPTR           0x0806
    CP_RB_WPTR           0x0807
    CP_SQE_CNTL          0x0808
    CP_CP2GMU_STATUS     0x0812  bit 0 IFPC
    CP_HW_FAULT          0x0821
    CP_INTERRUPT_STATUS  0x0823
    CP_PROTECT_STATUS    0x0824  bits 17:0 ADDR, bit 20 READ, bit 21 CP_HALTED, bit 22 ACCESS_VIOLATION
    CP_STATUS_1          0x0825
    CP_SQE_INSTR_BASE    0x0830  64-bit
    CP_MISC_CNTL         0x0840
    CP_CHICKEN_DBG       0x0841
    CP_ADDR_MODE_CNTL    0x0842
    CP_DBG_ECO_CNTL      0x0843
    CP_APRIV_CNTL        0x0844  bit 0 ICACHE, 1 RBFETCH, 2 RBPRIVLEVEL, 3 RBRPWB, 4 SCRATCHWT, 5 CDREAD, 6 CDWRITE
    CP_PROTECT_CNTL      0x084f  bit 0 ACCESS_PROT_EN, bit 1 ACCESS_FAULT_ON_VIOL_EN, bit 3 LAST_SPAN_INF_RANGE
    CP_PROTECT[0..31]    0x0850
    CP_SCRATCH[0..7]     0x0883
    CP_CONTEXT_SWITCH_CNTL 0x08a0
    CP_ROQ_THRESHOLDS_1  0x08c1
    CP_ROQ_THRESHOLDS_2  0x08c2
    CP_MEM_POOL_SIZE     0x08c3
    CP_ALWAYS_ON_COUNTER 0x0980  64-bit
    RBBM_GBIF_HALT       0x0016
    RBBM_GBIF_HALT_ACK   0x0017
    RBBM_INT_CLEAR_CMD   0x0037
    RBBM_INT_0_MASK      0x0038
    RBBM_SW_RESET_CMD    0x0043
    RBBM_CLOCK_CNTL      0x00ae
    RBBM_INT_0_STATUS    0x0201
    RBBM_STATUS          0x0210
    RBBM_PERFCTR_CNTL    0x0500
    UCHE_TRAP_BASE       0x0e09  64-bit
    UCHE_GMEM_RANGE_MIN  0x0e0b  64-bit
    UCHE_GMEM_RANGE_MAX  0x0e0d  64-bit
    RBBM_SECVID_TRUST_CNTL       0xf400
    RBBM_SECVID_TSB_TRUSTED_BASE 0xf800  64-bit
    RBBM_SECVID_TSB_TRUSTED_SIZE 0xf802
    RBBM_SECVID_TSB_CNTL         0xf803
    RBBM_SECVID_TSB_ADDR_MODE_CNTL 0xf810

The interrupt bits of RBBM_INT_0_MASK, RBBM_INT_0_STATUS and RBBM_INT_CLEAR_CMD that apply to A6XX:

    0 RBBM_GPU_IDLE       1 CP_AHB_ERROR        6 RBBM_ATB_ASYNCFIFO_OVERFLOW  7 RBBM_GPC_ERROR
    8 CP_SW               9 CP_HW_ERROR        10 CP_CCU_FLUSH_DEPTH_TS       11 CP_CCU_FLUSH_COLOR_TS
    12 CP_CCU_RESOLVE_TS  13 CP_IB2            14 CP_IB1                      15 CP_RB
    17 CP_RB_DONE_TS      18 CP_WT_DONE_TS     20 CP_CACHE_FLUSH_TS           22 RBBM_ATB_BUS_OVERFLOW
    23 RBBM_HANG_DETECT   24 UCHE_OOB_ACCESS   25 UCHE_TRAP_INTR              26 DEBBUS_INTR_0
    27 DEBBUS_INTR_1      30 ISDB_CPU_IRQ

The register init sequence (which of these are written, in what order and with what values), the CP_RB_CNTL encoding, the CP_PROTECT table and the values for the "magic" registers are not given by the allowed sources as a sequence. Mesa's device table does list the values userspace writes to a set of raw registers on this GPU (raw_magic_regs [DEV]), for example PC_POWER_CNTL 7 and VFD_POWER_CNTL 7, but those are written from the command stream by userspace, not by the kernel. The kernel's sequence is to be observed (section 8, step 5).

4.3 Packet formats [XML pm4, PM4H]

a6xx uses type 4 and type 7 packets. Type 0 and type 3 are not used on this generation.

    type 4 (register write):  0x40000000 | count | (odd_parity(count) << 7)
                              | ((register & 0x3ffff) << 8) | (odd_parity(register) << 27)
                              count is the number of payload words, 1 to 0x7e
    type 7 (opcode):          0x70000000 | count | (odd_parity(count) << 15)
                              | ((opcode & 0x7f) << 16) | (odd_parity(opcode) << 23)

odd_parity(v) is the bit that makes the number of set bits odd. Mesa computes it as (0x9669 >> (0xf & (v ^ v>>4 ^ v>>8 ^ v>>12 ^ v>>16 ^ v>>20 ^ v>>24 ^ v>>28))) & 1 [PM4H]. The decoder checks the type 7 opcode in bits 22:16 and the size in bits 13:0, and the type 4 register in bits 26:8 and the size in bits 6:0 [PM4H].

The opcodes the a3xx ring code used, and their a6xx form [XML pm4]:

    CP_NOP                 0x10
    CP_REG_RMW             0x21
    CP_WAIT_FOR_IDLE       0x26
    CP_MEM_WRITE           0x3d   payload: 64-bit address, then the words to write
    CP_REG_TO_MEM          0x3e
    CP_INDIRECT_BUFFER     0x3f   payload: IB_BASE (64-bit), then IB_SIZE in bits 19:0 of word 2
    CP_EVENT_WRITE         0x46   word 0: EVENT in bits 7:0, TIMESTAMP bit 30, IRQ bit 31, then ADDR (64-bit)
    CP_ME_INIT             0x48
    CP_SMMU_TABLE_UPDATE   0x53   TTBR0 low, then TTBR0_HI bits 15:0 and ASID bits 31:16, CONTEXTIDR, CONTEXTBANK
    CP_SET_PSEUDO_REG      0x56
    CP_SET_PROTECTED_MODE  0x5f
    CP_SET_SECURE_MODE     0x66

Events [XML pm4]: CACHE_FLUSH_TS is 0x04 and RB_DONE_TS is 0x16.

The CP_ME_INIT payload on a6xx, the fence packet the kernel puts after each IB, and whether CP_SET_SECURE_MODE has to leave secure mode before the first submission are not in the allowed sources. They are to be observed from the ring (section 8, step 7). CP_SMMU_TABLE_UPDATE exists so the CP can switch page tables itself (per-process page tables). A first driver with one shared page table does not need it.


5. The GMU and HFI [XML a6xx_gmu.xml, CRASHDEC]

5.1 What is known

On a6xx the GPU does not power itself up from the kernel's register writes alone. The GMU, a Cortex-M3 class processor (its registers are named GMU_CM3_*), runs its own firmware and owns the GX power domain, the GPU frequency and the power state votes to RPMh. RPMh is the SoC's resource power manager, reached through the RSCC and PDC regions. The kernel talks to the GMU through HFI, a set of message queues in shared memory, and through out-of-band (OOB) request bits.

The GMU registers known from a6xx_gmu.xml, with their word offsets:

    GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL 0x1a880
    GMU_GX_SPTPRAC_POWER_CONTROL     0x1a881
    GMU_CM3_ITCM_START               0x1b400   start of the GMU instruction memory window
    GMU_CM3_DTCM_START               0x1c400   start of the GMU data memory window
    GMU_NMI_CONTROL_STATUS           0x1cbf0
    GMU_BOOT_SLUMBER_OPTION          0x1cbf8
    GMU_GX_VOTE_IDX                  0x1cbf9
    GMU_MX_VOTE_IDX                  0x1cbfa
    GMU_DCVS_ACK_OPTION              0x1cbfc
    GMU_DCVS_PERF_SETTING            0x1cbfd
    GMU_DCVS_BW_SETTING              0x1cbfe
    GMU_DCVS_RETURN                  0x1cbff
    GMU_CORE_FW_VERSION              0x1d3f8   MAJOR 31:28, MINOR 27:16, STEP 15:0
    GMU_ICACHE_CONFIG                0x1f400
    GMU_DCACHE_CONFIG                0x1f401
    GMU_SYS_BUS_CONFIG               0x1f40f
    GMU_CM3_SYSRESET                 0x1f800
    GMU_CM3_BOOT_CONFIG              0x1f801
    GMU_CM3_FW_BUSY                  0x1f81a
    GMU_CM3_FW_INIT_RESULT           0x1f81c
    GMU_CM3_CFG                      0x1f82d
    GMU_PWR_COL_INTER_FRAME_CTRL     0x1f8c0   bit 0 IFPC_ENABLE, bit 1 HM_POWER_COLLAPSE_ENABLE, bit 2 SPTPRAC_POWER_CONTROL_ENABLE
    GMU_PWR_COL_INTER_FRAME_HYST     0x1f8c1
    GMU_SPTPRAC_PWR_CLK_STATUS       0x1f8d0   A6XX: bit 5 GMU_UP_POWER_STATE, bit 6 GX_HM_GDSC_POWER_OFF, bit 7 GX_HM_CLK_OFF
    GMU_GPU_NAP_CTRL                 0x1f8e4
    GMU_RPMH_CTRL                    0x1f8e8   bit 0 RPMH_INTERFACE_ENABLE, 4 LLC, 8 DDR, 9 MX, 10 CX, 11 GFX vote enables
    GPU_GMU_CX_GMU_RPMH_POWER_STATE  0x1f8ec
    GMU_ALWAYS_ON_COUNTER_L/H        0x1f888 / 0x1f889
    GMU_HFI_CTRL_STATUS              0x1f980
    GMU_HFI_VERSION_INFO             0x1f981
    GMU_HFI_SFR_ADDR                 0x1f982
    GMU_HFI_MMAP_ADDR                0x1f983
    GMU_HFI_QTBL_INFO                0x1f984
    GMU_HFI_QTBL_ADDR                0x1f985
    GMU_HFI_CTRL_INIT                0x1f986
    GMU_GMU2HOST_INTR_SET/CLR/INFO/MASK  0x1f990 to 0x1f993   INFO: bit 0 MSGQ, bit 23 CM3_FAULT
    GMU_HOST2GMU_INTR_SET            0x1f994
    GMU_HOST2GMU_INTR_CLR            0x1f995
    GMU_HOST2GMU_INTR_RAW_INFO       0x1f996
    GMU_GENERAL_0/1/6/7              0x1f9c5 / 0x1f9c6 / 0x1f9cb / 0x1f9cc
    GMU_AO_INTERRUPT_EN              0x23b03
    GMU_AO_HOST_INTERRUPT_CLR        0x23b04
    GMU_AO_HOST_INTERRUPT_STATUS     0x23b05   bit 0 WDOG_BITE, 1 RSCC_COMP, 2 VDROOP, 3 FENCE_ERR, 4 DBD_WAKEUP, 5 HOST_AHB_BUS_ERROR
    GMU_AO_HOST_INTERRUPT_MASK       0x23b06
    GPU_GMU_AO_GPU_CX_BUSY_STATUS    0x23b0c
    GMU_RSCC_CONTROL_REQ/ACK         0x23b07 / 0x23b08
    GMU_AO_AHB_FENCE_CTRL            0x23b10
    GMU_AHB_FENCE_STATUS             0x23b13
    GPU_CC_GX_GDSCR                  0x24403
    GPU_CC_GX_DOMAIN_MISC            0x24542

The bit layouts a6xx_gmu.xml gives for the OOB requests and the HFI interrupt (the register they apply to is not named in the XML, which is itself something to observe):

    A6XX_GMU_OOB      GPU: set bit 18, check and clear bit 26
                      PERFCNTR: set 17, check and clear 25
                      BOOT_SLUMBER: set 22, check and clear 30
                      DCVS: set 23, check and clear 31
    A6XX_HFI_IRQ      bit 0 MSGQ, 1 DSGQ, 2 BLOCKED_MSG, 16-22 GMU_ERR, 23 CM3_FAULT, 24-31 OOB
    A6XX_HFI_H2F      bit 0 IRQ
    A6XX_GMU_GPU_IDLE_STATUS  bit 23 BUSY_IGN_AHB, bit 30 CX_GX_CPU_BUSY_IGN_AHB

5.2 HFI queue layout [CRASHDEC crashdec-hfi.c]

crashdec decodes the HFI memory out of a devcoredump. From its structures:

The queue table starts with a header of six words: version, size (of the whole table, in words), qhdr0_offset (where the first queue header is), qhdr_size, num_queues and active_queues. The queue headers follow, twelve words each: status, iova, type, size, msg_size, dropped, rx_watermark, tx_watermark, rx_request, tx_request, read_index and write_index. A queue's messages are at its iova. read_index and write_index count words and wrap at size.

Each message starts with a header word:

    bits 7:0     message id
    bits 15:8    size in words, including the header
    bits 19:16   type: 0 command, 1 ack, 2 ack (v1)
    bits 31:20   sequence number

The message ids crashdec knows, with their payload words after the header:

    0   H2F_MSG_INIT             seg_id, dbg_buffer_addr, dbg_buffer_size, boot_state
    1   H2F_MSG_FW_VERSION       supported_version
    3   H2F_MSG_BW_TABLE         bw_level_num, cnoc_cmds_num, ddr_cmds_num, cnoc_wait_bitmask,
                                 ddr_wait_bitmask, cnoc_cmds_addrs[6], cnoc_cmds_data[2][6],
                                 ddr_cmds_addrs[8], ddr_cmds_data[16][8]
    4   H2F_MSG_PERF_TABLE       num_gpu_levels, num_gmu_levels, gx_votes[16] of {vote, acd, freq}
                                 (or {vote, freq} on the legacy GMU), cx_votes[4] of {vote, freq}
    5   H2F_MSG_TEST             no payload
    10  H2F_MSG_START            no payload
    14  H2F_MSG_CORE_FW_START    handle
    30  H2F_MSG_GX_BW_PERF_VOTE  ack_type, freq, bw
    33  H2F_MSG_PREPARE_SLUMBER  bw, freq
    100 F2H_MSG_ERROR            code, two more words
    126 F2H_MSG_ACK              ret_header (the header of the message acknowledged), error, payload[16]

5.3 What is not known

None of the allowed sources gives:

- the GMU boot sequence: how the GMU firmware image is laid out and where its pieces go (ITCM, DTCM or memory the GMU reaches through the SMMU at stream id 5), what is written to GMU_CM3_SYSRESET and GMU_CM3_BOOT_CONFIG, and what GMU_CM3_FW_INIT_RESULT reads when the boot worked.
- the RSCC and PDC programming, and whether the bootloader has already done it.
- how the HFI queue table is handed over (the values of GMU_HFI_QTBL_ADDR, GMU_HFI_QTBL_INFO and GMU_HFI_CTRL_INIT), how many queues there are and which is which.
- the message sequence at start-up, and the contents of the perf and bandwidth tables for this chip. The votes are RPMh command words that come from the command database, which is in turn not described by any allowed source.
- which register the OOB bits go in, and the order of the GPU OOB request against the GX power-up.
- whether the legacy or the newer perf table format applies to the a690 firmware.
- what the GPU core clock is and how the kernel requests a frequency, since the GPU node has no clocks.

All of it goes to the observation plan. Section 8, steps 3, 4 and 8, are designed to capture it.


6. GPU SMMU context bank programming [ARM-SMMU, DT]

The GPU SMMU is an ARM SMMU-500 [DT compatible], which implements the SMMUv2 architecture. This section gives the register model from the ARM specification, in the same shape as section 3.2 of the A306 spec. The values the Linux driver writes, the context bank numbers it uses, and whether the global registers may be written by the operating system at all on this chip are to be observed (section 8, step 2). Unlike the MSM8916, where a secure call set up the secure page table and the banks, nothing in the DT says what the secure world or the hypervisor owns here.

The address map [ARM-SMMU]. The SMMU is split into a global space and a translation context space of equal size. Each is NUMPAGE pages of PAGESIZE bytes, where NUMPAGE is 2^(NUMPAGENDXB + 1), and both NUMPAGENDXB and PAGESIZE come from SMMU_IDR1. Global register space 0 is page 0 and global register space 1 is page 1. Context bank n starts at base + NUMPAGE * PAGESIZE + n * PAGESIZE. With the 0x20000 region in the DT and 4 KB pages, context bank 0 would be at 0x03db0000 [CALC]. Read IDR1 to confirm. The number of context banks is in SMMU_IDR1 NUMCB, and the number of stream match registers in SMMU_IDR0 NUMSMRG.

Global registers used to route the GPU's stream ids to a context bank [ARM-SMMU]:

    SMMU_sCR0        GR0 0x000   global control, including client port enable and unmatched-stream behaviour
    SMMU_IDR0        GR0 0x020
    SMMU_IDR1        GR0 0x024
    SMMU_sGFSR       GR0 0x048   global fault status
    SMMU_SMRn        GR0 0x800 + 4n   VALID bit 31, MASK, ID
    SMMU_S2CRn       GR0 0xc00 + 4n   TYPE (translate, bypass or fault), CBNDX (the context bank)
    SMMU_CBARn       GR1 0x000 + 4n   context bank type (stage 1 with stage 2 bypass for the GPU)
    SMMU_CBA2Rn      GR1 0x800 + 4n   VA64 for the AArch64 page table format

The stream ids to route are 0 and 1 (the GPU) and 5 (the GMU), each with mask 0xc00 [DT iommus].

Registers in a context bank, with their offset in the bank [ARM-SMMU]:

    SCTLR           0x000   M (enable) and fault reporting bits
    TCR2            0x010
    TTBR0 (64-bit)  0x020   ASID in bits 63:48, table address below
    TTBR1 (64-bit)  0x028
    TCR             0x030
    CONTEXTIDR      0x034
    MAIR0           0x038
    MAIR1           0x03c
    FSR             0x058   write 1 to clear
    FAR (64-bit)    0x060
    FSYNR0          0x068
    TLBIVA          0x600
    TLBIASID        0x610
    TLBIALL         0x618
    TLBIVAL         0x620
    TLBSYNC         0x7f0
    TLBSTATUS       0x7f4

The programming order is the one already used for the A306 [a306 spec 3.2]: disable SCTLR, clear FAR and FSR, write TTBR0, TTBR1, TCR2, TCR, MAIR0 and MAIR1, then set SCTLR. The page table is the ARMv8 stage 1 format with 4 KB granule. CP_SMMU_TABLE_UPDATE uses the AArch64 format only (from a5xx on) [XML pm4], which is consistent with this. TLB maintenance after an unmap is TLBIVA or TLBIVAL, then TLBSYNC, then poll TLBSTATUS until it is clear.

The exact TCR, TCR2, MAIR and SCTLR values, which bank holds the GPU and which holds the GMU, and the ASIDs are to be observed. The SMMU is dma-coherent [DT], so the page table walk attributes in TCR may differ from the A306's non-coherent ones. That too is to be observed.


7. Firmware [WHENCE, DT]

    SQE microcode    linux-firmware has qcom/a660_sqe.fw (version v1.12). There is no a690-named SQE file.
                     Observed: the a690 loads qcom/a660_sqe.fw [OBS TheNightman 2026-10-19].
    GMU firmware     linux-firmware has no a690_gmu.bin. The a6xx GMU images it has are a623_gmu.bin,
                     a630_gmu.bin, a640_gmu.bin, a650_gmu.bin, a660_gmu.bin (v3.01.0B) and a663_gmu.bin.
                     Observed: the a690 loads qcom/a660_gmu.bin [OBS TheNightman 2026-10-19].
    Zap shader       qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn [DT x13s.dts firmware-name, WHENCE]
                     The DT gives it a no-map carve-out, pil_gpu_mem at 0x8bf00000, size 0x2000.

Licences [WHENCE]: the Adreno GPU firmware block and the Lenovo zap shader entry are both "Redistributable. See LICENSE.qcom and NOTICE.qcom for details". These files are not stored in the PureDarwin tree. Like the A306 microcode, PureDarwin loads them at first open from:

    /Library/Firmware/qcom/a660_sqe.fw
    /Library/Firmware/qcom/a660_gmu.bin
    /Library/Firmware/qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn

LICENSE.qcom allows the binaries to be redistributed only in binary form, only for platforms with Qualcomm chipsets, with the licence text included and the notices kept intact. It also forbids reverse engineering, disassembling or decompiling them. So nobody on this project looks inside these files, and that includes running Mesa's microcode disassembler on them. The spec only describes how the driver places, loads and talks to the firmware, as seen from outside.

The zap shader is a signed image for the secure world. How it is handed to the secure world (which secure calls, with what arguments) and whether the GPU can run without it are not in the allowed sources. If the observation shows that the GPU powers up in a secure state that only the zap shader leaves, the secure call sequence is captured the same way the A306's SMC calls were (section 8, step 2).


8. Observation plan for TheNightman on the 8cx

This is what turns version 0 into a verified spec. It mirrors how the A306 was observed: the Linux msm driver runs as test equipment on the X13s, and only what the hardware sees is recorded. Nobody opens the Linux driver sources while doing this. Keep every raw capture in an observations/ folder outside PureDarwin, named by run, as was done for the MSM8916, and note the kernel version, the firmware file versions and the X13s firmware version with each run.

Tools, all from allowed sources or generic Linux: ftrace with the rwmmio tracepoints (CONFIG_TRACE_MMIO_ACCESS) for register reads and writes, an SMC tracepoint or a kprobe on the SMCCC call for secure calls, the msmparams tool used for the A306, Mesa's rd capture through /sys/kernel/debug/dri/0/rd and cffdump [FDDOC], the devcoredump decoded with crashdec [FDDOC], and the clk, genpd and interconnect summaries in debugfs.

1. Identity. Run msmparams against the msm driver and record every parameter, including the ones that answer EINVAL. This gives CHIP_ID with its upper bits, GMEM_SIZE, GMEM_BASE, MAX_FREQ, VA_START, VA_SIZE, PRIORITIES and the rest. The firmware files are already known: TheNightman's notes from an Ubuntu install on the X13s show a660_sqe.fw, a660_gmu.bin and qcdxkmsuc8280.mbn (section 7).
2. SMMU and secure calls. Trace MMIO on 0x03da0000 to 0x03dc0000 and every SMC from boot until the first submission. Record SMMU_IDR0, IDR1 and sCR0, the SMR, S2CR, CBAR and CBA2R writes (or note that there are none, which means something else owns them), and each context bank write as in A306 section 3.2. Record the secure calls around zap shader loading, with function id and arguments, as was done for the A306 secure page table.
3. Clocks and power domains. Before and after the GPU powers up, save /sys/kernel/debug/clk/clk_summary, /sys/kernel/debug/pm_genpd/pm_genpd_summary and /sys/kernel/debug/interconnect/interconnect_summary. Trace MMIO on gpucc (0x03d90000 to 0x03d99000) and on the GCC clocks the DT names for the GPU and GMU. As on the MSM8916, map each register write to a clock or GDSC name by matching it against the clk tracepoints (clk_enable, clk_set_rate) and the genpd events in the same window.
4. GMU boot. Trace MMIO on the gmu region (0x03d6a000 to 0x03d9e000), on rscc (0x03de0000 to 0x03df0000) and on gmu_pdc (0x0b290000 to 0x0b2a0000) from module load to the first GPU register write. Name every access with a6xx_gmu.xml. This should show the order of the CM3 reset, the firmware placement, the HFI table handover, the OOB requests and the GX power-up. Also record what the GMU's own memory windows (GMU_CM3_ITCM_START, GMU_CM3_DTCM_START) receive, as offsets and sizes only.
5. GPU register init. Trace MMIO on 0x03d00000 to 0x03d40000 from the GMU being up to the first CP_RB_WPTR write, collapse it and name every register with a6xx.xml, like gpu-mmio-collapsed.txt for the A306. This gives the init sequence, CP_RB_CNTL, the CP_PROTECT table, CP_SQE_INSTR_BASE, CP_RB_RPTR_ADDR and the interrupt mask.
6. SQE firmware placement. From the trace in step 5, take the value written to CP_SQE_INSTR_BASE. Then force a GPU hang (for example with a submission that waits on memory that never changes) and take the devcoredump. crashdec shows the SQE state and the buffers it knows. Compare what is at the instruction base with the firmware file to tell whether the file has a header word that is skipped.
7. Ring contents. Capture /sys/kernel/debug/dri/0/rd while running the same gltri workload used on the A306, and decode it with cffdump. Record the first packets in the ring after start (the a6xx form of CP_ME_INIT and anything that leaves secure mode), and the packets the kernel wraps around each user IB (the fence write and its interrupt). Also record which RBBM_INT_0_STATUS bit the fence raises and how it is acknowledged.
8. HFI traffic. The devcoredump from step 6 includes the GMU log and the HFI queues, and crashdec decodes the messages and the queue history [CRASHDEC]. Take one dump soon after power-up and one after a frequency change. This records the INIT, FW_VERSION, PERF_TABLE, BW_TABLE and START messages with their payloads, and the GX_BW_PERF_VOTE sent when the frequency changes. Trace MMIO on GMU_HOST2GMU_INTR_SET and GMU_GMU2HOST_INTR_INFO in the same window to see how the queues are doorbelled.
9. Idle and power off. Let the GPU go idle, trace what happens (slumber, the PREPARE_SLUMBER message, GDSC collapse), and record it. A first PureDarwin driver can keep the GPU up, but the order matters for a clean shutdown.

When each step is done, its results go into this spec with an [OBS run1 ...] tag, and the matching open item below is closed.


Open items

- GMEM size and base, the exact CHIP_ID, the VA range and MAX_FREQ as Linux reports them (step 1).
- The SQE firmware header, if any (step 6).
- Clock and GDSC register offsets in gpucc and gcc, and the power-on order. No allowed source has them (step 3).
- The GPU core clock and how a frequency is chosen, given that the GPU node has no clocks and the GMU has a PERF_TABLE message and a GX_BW_PERF_VOTE message (steps 3 and 8).
- Whether the interconnect vote ("gfx-mem") has to be made by PureDarwin, or whether the bootloader's default bandwidth is enough to bring the GPU up (step 3).
- The whole GMU boot, the RSCC and PDC programming, and the HFI start-up messages (steps 4 and 8).
- Whether the OS may program the SMMU global registers, which context banks and ASIDs are used, and the TCR and MAIR values (step 2).
- The zap shader: whether it is required, and the secure call sequence that loads it (step 2).
- The kernel's ring start packets and its per-submission fence packets (step 7).
- What cx_mem and cx_dbgc hold. They may not be needed for a first bring-up.
- Hang recovery, preemption, LPAC, IFPC (inter-frame power collapse) and performance counters. Out of scope for version 1.
