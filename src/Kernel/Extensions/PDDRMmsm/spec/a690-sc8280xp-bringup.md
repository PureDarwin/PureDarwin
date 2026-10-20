A690 on SC8280XP, bring-up spec, version 1


What this is

This spec describes what is known about taking the Adreno 690 in the Qualcomm Snapdragon 8cx Gen 3 (SC8280XP, as in the Lenovo ThinkPad X13s) from power off to executing command buffers, for the PDDRMmsm kernel extension.

Status on 19 October 2026: checked against hardware. Version 0 was written at a desk from the allowed sources. This version adds what the observation plan (section 8) recorded on an X13s, with the Linux msm driver used as test equipment. Every observed value carries an [OBS run1 ...] tag. Where version 0 guessed and the hardware disagreed, the observation wins and the text says so. Nothing in this spec has been implemented in PDDRMmsm yet. The items still open after run1 are listed at the end, with the capture that would close each.

Roles for this spec. The observer and spec writer is a language model, which wrote version 0 from the allowed sources and turned the run1 captures into this version. The driver is implemented by a person, TheNightman, who also ran the observation plan on the 8cx. This spec describes behaviour and values, never code, and it contains no driver code.

Every value cites its source with one of these tags:

- [OBS run1 <capture> <time>] is a black-box capture on the X13s, described in section 8. [OBS run2 gmumap.txt] is the GMU image comparison of section 5.2a. <capture> is a folder of the run (boot, load, open, submit, rd, hang, freq, idle, or clean/<folder> for the repeat without console tracing). <time> is the kernel timestamp in seconds in that capture's trace. Register names in the captures come from the XML below, applied by address, and the Linux symbol names in the raw traces were dropped before anything was read.
- [DEV] is Mesa's device table, src/freedreno/common/freedreno_devices.py, and the matching rules in src/freedreno/common/freedreno_dev_info.h and freedreno_dev_info.c (MIT).
- [XML a6xx.xml], [XML a6xx_gmu.xml] and [XML pm4] are the freedreno register database in Mesa's src/freedreno/registers/adreno/ (MIT), at Mesa 44fd0fde8187. [XML pm4] is adreno_pm4.xml.
- [PM4H] is Mesa's src/freedreno/common/freedreno_pm4.h (MIT), the packet header helpers.
- [QRISC] is Mesa's src/freedreno/qrisc/README.rst (MIT), the documentation of the CP microcontroller and its firmware.
- [FDDOC] is Mesa's docs/drivers/freedreno.rst (MIT), which documents the SQE, the devcoredump, the rd capture and the decode tools.
- [CRASHDEC] is Mesa's src/freedreno/decode/crashdec.c, crashdec.h and crashdec-hfi.c (MIT), the devcoredump decoder, which knows the layout of the HFI queues.
- [MESA] is Mesa's src/freedreno/drm/msm/*.c (MIT), as in msm-uapi.md.
- [DT] is the mainline device tree, arch/arm64/boot/dts/qcom/sc8280xp.dtsi and sc8280xp-lenovo-thinkpad-x13s.dts. Both files carry "SPDX-License-Identifier: BSD-3-Clause" on their first line, which was checked before they were read. Values marked [DT live] were read from /proc/device-tree on the X13s during run1.
- [WHENCE] is linux-firmware's WHENCE file, used only for file names, versions and licence lines.
- [ARM-SMMU] is the ARM System MMU architecture specification, version 2 (public). The SMMU-500 implements it.
- [OBSD] is OpenBSD's sys/dev/fdt/qcscm.c (ISC), for the Qualcomm secure call convention and the PAS command numbers.
- [CALC] is arithmetic on cited values, done in this spec. The inputs are cited next to it.


1. Identity [DEV, MESA, OBS run1 open msmparams]

Mesa's device table has one entry for this GPU, listed under two ids [DEV]:

    GPUId(690)
    GPUId(chip_id=0xffff06090000, name="FD690")    (the default no-speedbin fallback)
    generation        CHIP.A6XX, with the a6xx_base and a6xx_gen4 property sets
    num_ccu           8
    num_vsc_pipes     32
    highest_bank_bit  16
    tile_align_w/h    64 / 32
    tile_max_w/h      1024 / 1024

The chip id is a 64-bit value with core, major, minor and patch in bytes 3, 2, 1 and 0, and a fuse or speed bin id in bits 47:32. A table entry whose bits 47:32 are 0xffff matches any fuse id [DEV freedreno_dev_info.c].

What the Linux driver reports [OBS run1 open msmparams, copied to observations/a690-msm-uapi-params.txt]:

    GPU_ID        0 (none)
    CHIP_ID       0xffff06090000
    GMEM_SIZE     0x400000 (4 MB)
    GMEM_BASE     0
    MAX_FREQ      690000000
    VA_START      0x100000000
    VA_SIZE       0xffff00000000
    PRIORITIES    3
    HIGHEST_BANK_BIT 16, UBWC_SWIZZLE 6, MACROTILE_MODE 1, UCHE_TRAP_BASE 0x1fffffffff000, HAS_PRR 1, RAYTRACING 0

The kernel reports GPU_ID 0, and Mesa then derives 690 from the chip id as core * 100 + major * 10 + minor [DEV freedreno_dev_info.h]. With these values Mesa 26.0.8 reported the renderer "FD690" [OBS run1 rd, kmscube output]. The chip id revision byte read back from the GPU was also 6.9.0.0 [OBS run1 hang crashdec, "revision: 0 (6.9.0.0)"].

What pd_msm_info needs:

    name          "a690"
    gpu_id        0, as Linux reports it. Do not report 690 [OBS, DEV].
    chip_id       0xffff06090000 [OBS]
    gmem_size     0x400000 [OBS]
    gmem_base     0 [OBS]
    max_freq      690000000 [OBS], the highest operating point in the DT
    core_clk      none. The GPU frequency is chosen by the GMU through HFI votes (section 5.5). The GPU node has no clocks [DT].
    va_start/end  0x100000000 to 0x1000000000000 for user buffers [OBS]. The kernel's own buffers live in a separate upper half with bit 48 set (section 6.3).
    funcs         a new a6xx generation, described in sections 4 to 6.

The full parameter table, including the parameters that differ from the A306, is in msm-uapi.md.


2. Resources from the device tree [DT, OBS]

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

The GPU interrupt arrived as GIC INTID 332 (SPI 300), one interrupt per fence [OBS run1 clean/submit interrupts.before/after, 0 then 1]. No capture touched cx_mem or cx_dbgc [OBS run1, all captures]: a first driver does not need them.

The GPU operating points, each with its RPMh level and the peak memory bandwidth vote. The levels were confirmed from the live DT [DT live opp-level]:

    270 MHz   level 64   (LOW_SVS)    451000 kBps
    410 MHz   level 128  (SVS)        1555000 kBps
    500 MHz   level 192  (SVS_L1)     1555000 kBps
    547 MHz   level 224  (SVS_L2)     1555000 kBps
    606 MHz   level 256  (NOM)        2736000 kBps
    640 MHz   level 320  (NOM_L1)     2736000 kBps
    655 MHz   level 384  (TURBO)      2736000 kBps
    690 MHz   level 416  (TURBO_L1)   2736000 kBps

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

Linux runs the HFI exchange by polling, not on these two interrupts. Both interrupt masks are left closed except for fault bits (section 5.2).

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

The GCC, whose GPU clocks section 3.2 uses, is clock-controller@100000, 0x00100000 up to 0x002f4200 [DT live reg].


3. Register address spaces [XML a6xx.xml, XML a6xx_gmu.xml, CRASHDEC, CALC, OBS]

Register offsets in the XML are in 32-bit words. A GPU register at offset N is at byte address 0x03d00000 + 4 * N [CALC from DT].

a6xx_gmu.xml gives the GMU registers in the same word space as the GPU registers, starting at 0x1a800. The CPU address of a GMU register at offset N is again 0x03d00000 + 4 * N [CALC]. This was confirmed: every GMU access Linux made lands on a named a6xx_gmu.xml register at that address, and the sequence makes sense under those names (section 5) [OBS run1 clean/load]. For example:

    GMU_CM3_SYSRESET   0x1f800   0x03d7e000
    GMU_HFI_CTRL_INIT  0x1f986   0x03d7e618
    GMU_AO_INTERRUPT_EN 0x23b03  0x03d8ec0c
    GPU_CC_GX_GDSCR    0x24403   0x03d9100c

The first three fall inside the DT "gmu" region (0x03d6a000 to 0x03d9e000). GPU_CC_GX_GDSCR falls inside the gpucc region at 0x03d90000, which overlaps the end of the "gmu" region.

The RSCC registers in a6xx_gmu.xml are relative offsets. On this chip they are relative to the DT "rscc" region, 0x03de0000 [OBS run1 clean/load 456.223101]. The evidence: with that base, RSCC_PDC_SEQ_START_ADDR (byte 0x20) receives 0x4520, and RSCC_PDC_MATCH_VALUE_LO and _HI (0x24, 0x28) receive 0x4510 and 0x4514. Those are byte offsets inside the gmu_pdc region, and the gmu_pdc register at +0x4520 is GPU_SEQ_START_ADDR in the XML's A6XX_PDC domain [XML a6xx.xml]. The A6XX_PDC domain is relative to the DT "gmu_pdc" region at 0x0b290000 in the same way. Treat the RSCC names below as very likely, not certain: two of the RSCC words written have no XML name.

cx_mem, cx_dbgc and the rest of gmu_pdc are named by the DT. No capture accessed them.


4. The a6xx command processor [XML a6xx.xml, XML pm4, PM4H, QRISC, OBS]

4.1 What changes from a3xx

On a3xx the command processor was a prefetch parser (PFP) and a micro engine (ME), each with microcode written word by word through a data port (a306 spec, section 4). From a6xx on, both are replaced by a single microcontroller called the SQE [FDDOC, QRISC]. Starting with Adreno 660, the SQE firmware also carries the firmware for LPAC (low priority asynchronous compute), and the SQE boots LPAC itself [QRISC]. The a690 has LPAC (has_lpac in a6xx_gen4 [DEV]). Linux writes only CP_SQE_INSTR_BASE: there is no separate LPAC instruction base write [OBS run1 clean/load 456.263892].

4.2 SQE firmware placement [OBS run1 load, sqe.txt]

The SQE runs its microcode from GPU memory. CP_SQE_INSTR_BASE (0x0830, 64-bit) holds the GPU virtual address of the code, and CP_SQE_CNTL (0x0808) is set to 1 after the ring registers [OBS run1 clean/load 456.263897].

    CP_SQE_INSTR_BASE   0x0001000000009000   [OBS run1 clean/load 456.263892, the same at every power-up]

The page at that address was found by walking the GPU context bank's page table through /dev/mem, and the GPU's copy was compared with the firmware file by SHA-256, without printing either. The 43288 bytes at CP_SQE_INSTR_BASE equal the file from byte 4 onwards, and do not equal the file from byte 0 [OBS run1 sqe.txt]. So:

- The first 32-bit word of a660_sqe.fw is a header. The GPU is given the file from byte 4.
- The copy is mapped read-only to the GPU. The level 3 descriptor has AP = 0b11 (read-only), AttrIndx 0 (MAIR0 byte 0, 0x44, normal non-cacheable), SH = 0b10 (outer shareable), AF and nG set [OBS run1 sqe.txt descriptor 0x0000000152a50ec3, ARM-SMMU].
- Alignment: the copy starts on a 4 KB page.

4.3 Ring buffer and memory addresses [OBS run1 clean/load, clean/hang crashdec]

    CP_RB_BASE           0x0001000000001000   ring of 32768 bytes [OBS clean/hang crashdec "size: 32768"]
    CP_RB_CNTL           0x0000020c           bits 5:0 = 12 = log2(32768 / 8) [CALC], bits 9:8 = 2
    CP_RB_RPTR_ADDR      0x0001000000014000   where the CP writes its read pointer
    fence address        0x0001000000000004   where each submission's fence value is written (4.5)

The kernel keeps its own GPU buffers (ring, SQE copy, read pointer and fence words) at addresses with bit 48 set, in the TTBR1 half of the GPU address space (section 6.3).

The ring is restarted from offset 0 at every GPU power-up: CP_RB_WPTR is written 9 after CP_ME_INIT, then 0xb after CP_SET_SECURE_MODE, on the cold start and on every warm start [OBS run1 clean/load 456.263899 and 456.305354, clean/submit 581.690468 and 581.690500].

4.4 Packets at ring start [OBS run1 clean/hang devcoredump ring words 0 to 10]

The first 11 dwords of the ring, written at each power-up:

    CP_ME_INIT (0x48), 8 payload dwords:  0x0000002f 0x00000003 0x20000000 0 0 0 0 0
    CP_SET_SECURE_MODE (0x66), 1 dword:   0   (leave secure mode)

On the cold start, CP_ME_INIT is submitted (WPTR 9), then the zap shader is loaded through secure calls (section 7.2), then CP_SET_SECURE_MODE 0 is submitted (WPTR 0xb). On warm starts there are no secure calls, but the same two packets are submitted [OBS run1 clean/load 456.263899 to 456.305354, clean/submit 581.690468 to 581.690500]. The meaning of the CP_ME_INIT payload words is not in any allowed source. A first driver can send it verbatim.

4.5 Packets around one submission [OBS run1 clean/hang devcoredump ring words 11 to 67, rd]

For a submission of one IB (here a6xxsub's 7-dword IB at GPU address 0x100000000), Linux wrote these packets after the ring start. Decoded with [XML pm4]:

    CP_SET_PROTECTED_MODE 0
    PKT4 RBBM_PERFCTR_SRAM_INIT_CMD = 1
    CP_SMMU_TABLE_UPDATE   TTBR0 0x1_41cdb000, ASID 0, CONTEXTIDR 0, CONTEXTBANK 0
    CP_MEM_WRITE           to 0x1000000000810: 0x41cdb000, 0x00000001, 3
    CP_EVENT_WRITE         CACHE_INVALIDATE (0x31)
    CP_WAIT_REG_MEM        register 0x050f (RBBM_PERFCTR_SRAM_INIT_STATUS) == 1, mask 1, POLL_REGISTER
    CP_SET_PROTECTED_MODE 1
    CP_REG_TO_MEM          0x0400 (RBBM_PERFCTR_CP[0]), 64-bit, to 0x1000000000050
    CP_REG_TO_MEM          0x0982 (CP_ALWAYS_ON_CONTEXT), 64-bit, to 0x1000000000060
    CP_EVENT_WRITE         PC_CCU_INVALIDATE_DEPTH (0x18)
    CP_EVENT_WRITE         PC_CCU_INVALIDATE_COLOR (0x19)
    CP_INDIRECT_BUFFER     0x0000000100000000, 7 dwords
    CP_REG_TO_MEM          0x0400, 64-bit, to 0x1000000000058
    CP_REG_TO_MEM          0x0982, 64-bit, to 0x1000000000068
    PKT4 CP_SCRATCH[2]     = fence value
    CP_EVENT_WRITE         0x80000004 = CACHE_FLUSH_TS with the IRQ bit (31), address 0x1000000000004, value = fence value

What a first PureDarwin driver needs from this:

- The fence is a CACHE_FLUSH_TS event write with bit 31 (IRQ) set and three more words: the 64-bit address and the value. Bit 30 (TIMESTAMP) is not set. The CACHE_FLUSH_TS event writes the value by itself.
- The interrupt it raises is RBBM_INT_0_STATUS bit 20 (CP_CACHE_FLUSH_TS). The value read was exactly 0x00100000, and the handler acknowledged it by writing the same value to RBBM_INT_CLEAR_CMD [OBS run1 clean/submit 581.690753, 581.690754]. The interrupt came 33 microseconds after the CP_RB_WPTR write [OBS clean/submit 581.690712 to 581.690745].
- Around the interrupt handler, Linux set GMU_GMU_PWR_COL_KEEPALIVE (0x1f8c3) to 1 and back to 0, and read GMU_AO_AHB_FENCE_CTRL [OBS clean/submit 581.690747 to 581.690765].
- CP_SMMU_TABLE_UPDATE switches the GPU's TTBR0 to the submitting process's page table from the ring. A driver with one shared page table can leave it out, together with the CP_MEM_WRITE that records the table.
- The performance counter SRAM init, the counter reads and the CCU invalidates are housekeeping. Whether the GPU needs any of them to run an IB is not known. The safe start is to send them as observed.

Ring fence numbers start just below 2^32. The first submission after boot carried 0xffffff01 [OBS run1 clean/submit msm_gpu_submit_flush "ring=0:-255"], and the second, in the hang capture, 0xffffff02 [OBS clean/hang crashdec last-fence]. So the counter starts at 0xffffff00 [CALC] and wraps through 2^32 after 255 submissions, which exercises wrap handling early. A PureDarwin driver may start wherever it likes, but should compare fences with wrap in mind.

Your own IB (a CP_MEM_WRITE of a marker) executed and the marker was written [OBS run1 submit cmd.out, clean/submit cmd.out]. cffdump decoded its header 0x703d8003 as CP_MEM_WRITE with three payload words [OBS run1 rd rd.txt]. The PKT7 encoding in 4.6 is therefore confirmed by the hardware and by Mesa's decoder.

4.6 Packet formats [XML pm4, PM4H]

a6xx uses type 4 and type 7 packets. Type 0 and type 3 are not used on this generation.

    type 4 (register write):  0x40000000 | count | (odd_parity(count) << 7)
                              | ((register & 0x3ffff) << 8) | (odd_parity(register) << 27)
                              count is the number of payload words, 1 to 0x7e
    type 7 (opcode):          0x70000000 | count | (odd_parity(count) << 15)
                              | ((opcode & 0x7f) << 16) | (odd_parity(opcode) << 23)

odd_parity(v) is the bit that makes the number of set bits odd. Mesa computes it as (0x9669 >> (0xf & (v ^ v>>4 ^ v>>8 ^ v>>12 ^ v>>16 ^ v>>20 ^ v>>24 ^ v>>28))) & 1 [PM4H].

The opcodes and events used above [XML pm4]: CP_NOP 0x10, CP_REG_RMW 0x21, CP_WAIT_FOR_IDLE 0x26, CP_WAIT_REG_MEM 0x3c, CP_MEM_WRITE 0x3d, CP_REG_TO_MEM 0x3e, CP_INDIRECT_BUFFER 0x3f, CP_EVENT_WRITE 0x46, CP_ME_INIT 0x48, CP_SMMU_TABLE_UPDATE 0x53, CP_SET_PROTECTED_MODE 0x5f, CP_SET_SECURE_MODE 0x66. Events: CACHE_FLUSH_TS 0x04, PC_CCU_INVALIDATE_DEPTH 0x18, PC_CCU_INVALIDATE_COLOR 0x19, CACHE_INVALIDATE 0x31 (A6XX names).

4.7 GPU register initialisation [OBS run1 clean/load 456.263472 to 456.263899]

This is the order Linux wrote the GPU registers in, after the GMU started (section 5.3) and the GPU power OOB was granted, up to the first CP_RB_WPTR write. The same sequence, with the same values, repeats at every warm power-up [OBS run1 clean/submit 581.689983 to 581.690468]. Offsets are dwords. Interleaved GMU writes are included, marked GMU.

    GBIF_SCACHE_CNTL0            0x3c01   0x00003100   (read 0 first)
    GBIF_SCACHE_CNTL1            0x3c02   0x00c6318c   (read 0 first)
      ... GPU power OOB request and grant here (section 5.4) ...
    GBIF_HALT                    0x3c45   0            (read back 0)
    RBBM_GBIF_HALT               0x0016   0            (read back 0)
    RBBM_SECVID_TSB_CNTL         0xf803   0
    RBBM_SECVID_TSB_TRUSTED_BASE 0xf800   0 (64-bit)
    RBBM_SECVID_TSB_TRUSTED_SIZE 0xf802   0
    *_ADDR_MODE_CNTL             1        for CP 0x0842, VSC 0x0c01, GRAS 0x8601, RB 0x8e05, PC 0x9e01, HLSQ 0xbe05,
                                          VFD 0xa601, VPC 0x9601, UCHE 0x0e00, SP 0xae01, TPL1 0xb601,
                                          RBBM_SECVID_TSB 0xf810 (64-bit addressing everywhere)
    GMU GPU_GMU_AO_GMU_CGC_MODE_CNTL     0x00020200
    GMU GPU_GMU_AO_GMU_CGC_DELAY_CNTL    0x00010111
    GMU GPU_GMU_AO_GMU_CGC_HYST_CNTL     0x00005555
    RBBM_CLOCK_CNTL              0x00ae   read (0)
    GMU GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL read 0, write 0
    hardware clock gating table, below
    RBBM_CLOCK_CNTL              0x00ae   0x8aa8aa82
    RBBM_ISDB_CNT                0x0533   0x00000182
    RBBM_RAC_THRESHOLD_CNT       0x0044   0
    RBBM_SP_HYST_CNT             0x0042   0
    RBBM_CLOCK_CNTL_GMU_GX       0x0118   0x00000222
    RBBM_CLOCK_DELAY_GMU_GX      0x0119   0x00000111
    RBBM_CLOCK_HYST_GMU_GX       0x011a   0x00000555
    GMU GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL read 0, write 1
    RBBM_CLOCK_CNTL              0x00ae   0x8aa8aa82 (again)
    RBBM_GBIF_CLIENT_QOS_CNTL    0x0011   3
    RBBM_PERFCTR_GPU_BUSY_MASKED 0x050b   0xffffffff
    UCHE_WRITE_RANGE_MAX         0x0e05   0x0001ffffffffffc0 (64-bit)
    UCHE_TRAP_BASE               0x0e09   0x0001fffffffff000 (64-bit), the UCHE_TRAP_BASE parameter of section 1
    UCHE_WRITE_THRU_BASE         0x0e07   0x0001fffffffff000 (64-bit)
    UCHE_FILTER_CNTL             0x0e18   0x00000804
    UCHE_CACHE_WAYS              0x0e17   0x00000004
    CP_ROQ_THRESHOLDS_2          0x08c2   0x02000140
    CP_ROQ_THRESHOLDS_1          0x08c1   0x8040362c
    0x0b34 (unnamed for A6XX)             0x00000020   the XML names 0x0b34 CP_LPAC_PROG_FIFO_SIZE for A7XX only
    CP_MEM_POOL_SIZE             0x08c3   0x00000080
    PC_DBG_ECO_CNTL              0x9e00   0x00800200
    CP_AHB_CNTL                  0x098d   0x00000001
    RBBM_PERFCTR_CNTL            0x0500   0x00000001
    CP_PERFCTR_CP_SEL[0]         0x08d0   0
    RB_NC_MODE_CNTL              0x8e08   0x00000816
    TPL1_NC_MODE_CNTL            0xb604   0x00000006
    SP_NC_MODE_CNTL              0xae02   0x00000026
    UCHE_MODE_CNTL               0x0e01   0x00600000
    RBBM_NC_MODE_CNTL            0x0534   0x00000001
    RBBM_INTERFACE_HANG_INT_CNTL 0x001f   0x404fffff
    UCHE_CLIENT_PF               0x0e19   0x00000081
    TPL1_BICUBIC_WEIGHTS_TABLE[0..4] 0xb608   0x00000000 0x3fe05ff4 0x3fa0ebee 0x3f5193ed 0x3f0243f0
    GMU GPU_GMU_AO_GPU_CX_BUSY_MASK      0xff000000
    GMU GMU_CX_GMU_POWER_COUNTER_SELECT_0 read 0, write 0x00000020
    GMU GMU_CX_GMU_POWER_COUNTER_ENABLE   1
    CP_PROTECT_CNTL              0x084f   0x0000000b   (ACCESS_PROT_EN, ACCESS_FAULT_ON_VIOL_EN, LAST_SPAN_INF_RANGE)
    CP_PROTECT[0..31] and 3 more, below
    CP_CHICKEN_DBG               0x0841   0x00028801
    RBBM_GBIF_CLIENT_QOS_CNTL    0x0011   0
    UCHE_CMDQ_CONFIG             0x0e3c   0x00000090
    CP_APRIV_CNTL                0x0844   0x0000006e   (bits 1 RBFETCH, 2 RBPRIVLEVEL, 3 RBRPWB, 5 CDREAD, 6 CDWRITE; not 0 ICACHE, not 4 SCRATCHWT)
    RBBM_INT_0_MASK              0x0038   0x03d0e242
      ... secure call 0x42000c1b here (section 7.3) ...
    CP_SQE_INSTR_BASE            0x0830   0x0001000000009000
    CP_RB_BASE                   0x0800   0x0001000000001000
    CP_RB_CNTL                   0x0802   0x0000020c
    CP_RB_RPTR_ADDR              0x0804   0x0001000000014000
    CP_SQE_CNTL                  0x0808   1
    CP_RB_WPTR                   0x0807   9

RBBM_INT_0_MASK 0x03d0e242 enables bits 1 CP_AHB_ERROR, 6 RBBM_ATB_ASYNCFIFO_OVERFLOW, 9 CP_HW_ERROR, 13 CP_IB2, 14 CP_IB1, 15 CP_RB, 20 CP_CACHE_FLUSH_TS, 22 RBBM_ATB_BUS_OVERFLOW, 23 RBBM_HANG_DETECT, 24 UCHE_OOB_ACCESS and 25 UCHE_TRAP_INTR [CALC, XML a6xx.xml bitset A6XX_RBBM_INT_0_MASK].

After CP_RB_WPTR, Linux read GMU_AHB_FENCE_STATUS (0), polled GPU_GMU_AO_GPU_CX_BUSY_STATUS until it read 0x71000000, and read RBBM_STATUS (1) and RBBM_INT_0_STATUS (0) [OBS clean/load 456.263900 to 456.263914].

The hardware clock gating table, in write order (offset, value) [OBS run1 clean/load 456.263610 to 456.263647]:

    RBBM_CLOCK_CNTL_SP0 0x00b0 0x02222222      RBBM_CLOCK_CNTL2_SP0 0x00b4 0x02222220
    RBBM_CLOCK_DELAY_SP0 0x00b8 0x00000080     RBBM_CLOCK_HYST_SP0 0x00bc 0x0000f3cf
    RBBM_CLOCK_CNTL_TP0 0x00c0 0x22222222      RBBM_CLOCK_CNTL2_TP0 0x00c4 0x22222222
    RBBM_CLOCK_CNTL3_TP0 0x00c8 0x22222222     RBBM_CLOCK_CNTL4_TP0 0x00cc 0x00022222
    RBBM_CLOCK_DELAY_TP0 0x00d0 0x11111111     RBBM_CLOCK_DELAY2_TP0 0x00d4 0x11111111
    RBBM_CLOCK_DELAY3_TP0 0x00d8 0x11111111    RBBM_CLOCK_DELAY4_TP0 0x00dc 0x00011111
    RBBM_CLOCK_HYST_TP0 0x00e0 0x77777777      RBBM_CLOCK_HYST2_TP0 0x00e4 0x77777777
    RBBM_CLOCK_HYST3_TP0 0x00e8 0x77777777     RBBM_CLOCK_HYST4_TP0 0x00ec 0x00077777
    RBBM_CLOCK_CNTL_RB0 0x00f0 0x22222222      RBBM_CLOCK_CNTL2_RB0 0x00f4 0x01002222
    RBBM_CLOCK_CNTL_CCU0 0x00f8 0x00002220     RBBM_CLOCK_HYST_RB_CCU0 0x0100 0x00040f00
    RBBM_CLOCK_CNTL_RAC 0x0104 0x25222022      RBBM_CLOCK_CNTL2_RAC 0x0105 0x00005555
    RBBM_CLOCK_DELAY_RAC 0x0106 0x00000011     RBBM_CLOCK_HYST_RAC 0x0107 0x00445044
    RBBM_CLOCK_CNTL_TSE_RAS_RBBM 0x0108 0x04222222
    RBBM_CLOCK_MODE_VFD 0x0111 0x00002222      RBBM_CLOCK_MODE_GPC 0x0114 0x00222222
    RBBM_CLOCK_DELAY_HLSQ_2 0x0117 0x00000002  RBBM_CLOCK_MODE_HLSQ 0x011b 0x00002222
    RBBM_CLOCK_DELAY_TSE_RAS_RBBM 0x0109 0x00004000
    RBBM_CLOCK_DELAY_VFD 0x0112 0x00002222     RBBM_CLOCK_DELAY_GPC 0x0115 0x00000200
    RBBM_CLOCK_DELAY_HLSQ 0x011c 0              RBBM_CLOCK_HYST_TSE_RAS_RBBM 0x010a 0
    RBBM_CLOCK_HYST_VFD 0x0113 0                RBBM_CLOCK_HYST_GPC 0x0116 0x04104004
    RBBM_CLOCK_HYST_HLSQ 0x011d 0
    RBBM_CLOCK_CNTL_TEX_FCHE 0x0120 0x00000222 RBBM_CLOCK_DELAY_TEX_FCHE 0x0121 0x00000111
    RBBM_CLOCK_HYST_TEX_FCHE 0x0122 0
    RBBM_CLOCK_CNTL_UCHE 0x010b 0x22222222     RBBM_CLOCK_HYST_UCHE 0x0110 0x00000004
    RBBM_CLOCK_DELAY_UCHE 0x010f 0x00000002

Clock gating is a power optimisation. A first driver can skip the table and leave RBBM_CLOCK_CNTL at its reset value, which was read as 0 before the writes, and add the table later. That is a choice, not an observation: Linux always wrote it.

The CP_PROTECT table, register and value [OBS run1 clean/load 456.263675 to 456.263700]:

    [0]  0x13fc0000   [1]  0x00040501   [2]  0x0bd0050b   [3]  0x8000050e
    [4]  0x80000510   [5]  0x80000534   [6]  0x82080800   [7]  0x802008a0
    [8]  0x809008ab   [9]  0x02b808de   [10] 0x81340900   [11] 0x89c8098d
    [12] 0x80040e00   [13] 0x80300e03   [14] 0x830c3c00   [15] 0x7ffc3cc4
    [16] 0x873c8630   [17] 0x80008e00   [18] 0x801c8e08   [19] 0x807c8e50
    [20] 0x89fc8e80   [21] 0x876c9624   [22] 0x80449e60   [23] 0x861c9e78
    [24] 0x873ca630   [25] 0x8000ae02   [26] 0x84bcae50   [27] 0x8000b604
    [28] 0x8018b608   [29] 0x8004be02   [30] 0x857cbe20   [31] 0x97fcd000
    0x0870 0xaffcf000   0x0871 0x7ffcfc00   0x087f 0x80011c00

The XML defines 32 CP_PROTECT registers for A6XX (0x0850 to 0x086f). Linux also wrote dwords 0x0870, 0x0871 and 0x087f, which the XML does not name for A6XX. They continue the same table: this GPU has more protect registers than the XML lists, and 0x087f is the last one, whose span runs to the end of the register space (LAST_SPAN_INF_RANGE in CP_PROTECT_CNTL). The entry encoding (a 17- or 18-bit base register in the low bits, a span count above it, bit 31 for read protection) is read from the values, not from a source. Decode it from the XML's a6x_cp_protect type before relying on it.


5. The GMU and HFI [XML a6xx_gmu.xml, CRASHDEC, OBS]

5.1 What the GMU does

On a6xx the GPU does not power itself up from the kernel's register writes. The GMU, a Cortex-M3 class processor (its registers are named GMU_CM3_*), runs its own firmware and owns the GX power domain, the GPU frequency and the power state votes to RPMh. RPMh is the SoC's resource power manager, reached through the RSCC and PDC regions. The kernel talks to the GMU through HFI, a set of message queues in shared memory, and through out-of-band (OOB) request bits.

This was confirmed. The host never wrote GPU_CC_GX_GDSCR, the GX power switch. It only read it at power-off, after the GMU had already turned GX off (bit 31 clear, 0x00222801) [OBS run1 clean/load 456.387771]. Every GPU frequency change went through HFI (section 5.5). The host never set a GPU core clock rate through the clock controller.

5.2 GMU boot sequence [OBS run1 clean/load 456.260812 to 456.263460]

After the CX power domain and the clocks are on (section 3.2 of the clocks, section 9), and the SMMU banks are programmed (section 6), Linux started the GMU like this, with these values. The warm start does exactly the same [OBS run1 clean/submit 581.686683 to 581.689962].

    GMU_AO_HOST_INTERRUPT_CLR        0xffffffff
    GMU_AO_HOST_INTERRUPT_MASK       0xffffffd6   (unmasks bit 0 WDOG_BITE, bit 3 FENCE_ERR, bit 5 HOST_AHB_BUS_ERROR)
    GPU_GMU_CX_GMU_CX_FALNEXT_INTF   1
    GPU_GMU_CX_GMU_CX_FAL_INTF       1
    GMU_GENERAL_7                    1
    GMU_CORE_FW_VERSION              read: 0x3001000a (major 3, minor 0x001, step 0x000a)
    GMU_CM3_FW_INIT_RESULT           0
    GMU_CM3_BOOT_CONFIG              2
    GMU_HFI_QTBL_ADDR                0x60008000   (GMU address of the HFI queue table)
    GMU_HFI_QTBL_INFO                1
    GMU_AHB_FENCE_RANGE_0            0x802800a0
    GMU_CM3_CFG                      0x00004052
    GMU_HFI_SFR_ADDR                 0x06090000   (the chip id's low word, written into this register)
    GPU_GMU_CX_GMU_PWR_COL_CP_MSG    0x60004003
    GBIF_QSB_SIDE0..3 (GPU 0x3c03..0x3c06)  0x00071620 each
    GMU_SYS_BUS_CONFIG               1
    GMU_ICACHE_CONFIG                1
    GMU_DCACHE_CONFIG                1
    GMU_PWR_COL_INTER_FRAME_CTRL     0x09c40400   (bit 0 IFPC_ENABLE clear)
    GMU_RPMH_CTRL                    read 0, then 0x00000f11 (RPMH_INTERFACE_ENABLE, LLC, DDR, MX, CX, GFX votes)
    GMU_CORE_FW_VERSION              read again
    GMU_CM3_SYSRESET                 1
    GPU_GMU_CX_GMU_PWR_COL_CP_RESP   0
    GMU_CM3_SYSRESET                 0   (releases the CM3)
    GMU_CM3_FW_INIT_RESULT           polled about every 100 us until it reads 0x00000100
    GMU_HFI_CTRL_INIT                1
    GMU_HFI_CTRL_STATUS              read: 1
    four HFI messages (5.5)
    GMU_GMU2HOST_INTR_CLR            0xffffffff
    GMU_GMU2HOST_INTR_MASK           0xff7fffff   (unmasks only bit 23 CM3_FAULT)
    one more HFI message, the first GPU frequency vote (5.5)

The firmware reported its initialisation done 1.9 ms after the CM3 left reset on the cold start [OBS run1 clean/load 456.260864 to 456.262764], and 2.4 ms on the warm start [OBS run1 clean/submit 581.686734 to 581.689135].

GMU_CORE_FW_VERSION 0x3001000a is version 3.01.0A, for the file loaded in run1 (sha256 8acab7b4…e5e7) [OBS]. Version 0 of this spec quoted 3.01.0B from WHENCE. Either WHENCE describes a different revision of the file or version 0 misread it, and it was not rechecked. Identify the image by this register, not by WHENCE.

5.2a Where the GMU firmware image goes [OBS run2 gmumap.txt]

Run1 could not settle this. No capture contains a traced write to the GMU's TCM windows. But the rwmmio tracepoints only see the ordinary register accessors, not bulk copies made with the raw ones. Run2 (same kernel and firmware, a later boot) compared contents instead. With the GPU held powered after `modprobe msm`, it read what the GMU sees at each block's address of a660_gmu.bin and compared it by SHA-256 with that block of the file. It read only the image's block headers.

The image is a run of blocks. Each is a 16-byte little-endian header (GMU address, payload size, type, value) followed by the payload, padded to 4 bytes. A size of 0 marks a descriptor with no payload. Payload blocks carry 0xefbeadde in both type and value. The headers of the file run2 used (sha256 8acab7b4…e5e7):

    file   GMU address   size     type   value     where it is, as the GMU sees it
    0x000  0x00000000    0        1      0x01dc    ITCM (not a bank-1 address)
    0x010  0x00004000    0        1      0xd4c8    mapped in bank 1
    0x020  0x00000200    0        1      0x3a9c    ITCM
    0x030  0x00100000    0        1      0x85b4    mapped in bank 1
    0x040  0x00200000    0        1      0x12a80   mapped in bank 1
    0x050  0x00300000    0        1      0x70a0    mapped in bank 1
    0x060  0x10004000    0        1      0x2f20    DTCM
    0x070  0x10007000    0        1      0x0f84    DTCM
    0x080  0x10007fdc    0        1      0x0018    DTCM
    0x090  0x60000000    0        7      0x1000    mapped in bank 1
    0x0a0  0x60001000    0        7      0x1000    mapped in bank 1
    0x0b0  0x00000000    0x1dc    payload          ITCM, identical to the file
    0x29c  0x00004000    0xd4c8   payload          bank 1 RAM at GMU 0x4000, identical to the file
    0xd774 0x10007f98    0x28     payload          DTCM, identical to the file
    0xd7ac 0x10007fdc    0x18     payload          DTCM, different (live data: GMU_CORE_FW_VERSION is at 0x10007fe0)

So a loader puts each payload at its own GMU address:

- Payloads at GMU addresses 0x0 to 0x3fff go into the ITCM through the host window at 0x03d6d000 (GMU_CM3_ITCM_START, word 0x1b400). The window shows GMU address 0 at its first byte.
- Payloads at 0x10004000 to 0x10007fff go into the DTCM through the host window at 0x03d71000 (GMU_CM3_DTCM_START, word 0x1c400). The window shows GMU address 0x10004000 at its first byte. That puts GMU_CORE_FW_VERSION (0x03d74fe0) at DTCM address 0x10007fe0, which agrees.
- Any other payload goes into RAM that the host maps in the GMU's SMMU bank (6.4) at the payload's own GMU address. Here that is the 53 KB block at 0x4000.
- Descriptors (size 0) outside the TCMs name GMU memory the host must map. Every one was mapped. Type 1 descriptors give a region's extent in their value (0x4000 + 0xd4c8 for the code block, then 0x100000, 0x200000 and 0x300000 with no payload). Type 7 descriptors ask for a 4 KB page each (0x60000000, 0x60001000). Descriptors at TCM addresses need no mapping.

The copy into the TCMs happens before the CM3 leaves reset (GMU_CM3_SYSRESET 1, then 0, in 5.2). The exact moment is not observed, because the copy is invisible to the trace. Run2 did not test whether the GMU boots when the 0x100000/0x200000/0x300000 regions are mapped but left uninitialised. Linux mapped them inside one larger allocation (below), whose contents were not examined.

5.2b The GMU's address space [OBS run2 gmumap.txt, OBS run1 hang crashdec]

Everything the host mapped in bank 1 after a cold start. All mappings are read-write, MAIR index 0 (attr 0x44, normal non-cacheable, 6.3) and executable:

    0x00004000  0x441000 (4.25 MB)  one physically contiguous run: the image's code block and the
                                    0x100000, 0x200000 and 0x300000 regions named by descriptors
    0x00445000  0x1000
    0x00446000  0x8000
    0x0044e000  0xbb2000 (11.7 MB)  up to 0x01000000
    0x60000000  0x4000              the two type 7 pages and two more
    0x60004000  0x8000              GMU log (16 KB at 0x60004000) and HFI (16 KB at 0x60008000:
                                    queue table, queue 0 at 0x60009000, queue 1 at 0x6000a000)
    0x60400000  0x7000              GMU debug area (28 KB)

The log, HFI and debug addresses are the ones the GPU state dump reports [OBS run1 hang crashdec gmu-log, gmu-hfi, gmu-debug]. What the host keeps at 0x445000, 0x446000 and 0x44e000 to 0x1000000 is not known. They are mapped and writable. A first PureDarwin driver should map the same ranges, allocated and zeroed, and narrow them down once the GMU boots.

5.3 HFI queue table [OBS run1 clean/hang crashdec gmu-hfi, CRASHDEC]

The table starts with six words: version 0, size 120 (words, the whole table), qhdr0_offset 6, qhdr_size 12, num_queues 2, active_queues 2. Then two queue headers of 12 words each:

    queue 0 (host to GMU)   status 1, iova 0x60009000, type 0xa00, size 1024 words, msg_size 0,
                            rx_watermark 1, tx_watermark 1, rx_request 1, tx_request 0
    queue 1 (GMU to host)   status 1, iova 0x6000a000, type 0xa01, size 1024 words, the same watermarks

read_index and write_index count words and wrap at the size. Version 0 of this spec could not say how many queues there are or which is which. There are two: commands go in queue 0 and acknowledgements come back in queue 1. There is no debug queue on this firmware.

5.4 Doorbells, acknowledgements and the GPU OOB [OBS run1 clean/load 456.262776 to 456.263590, 456.305379]

For each HFI message the host writes the message into queue 0, then sets bit 0 of GMU_HOST2GMU_INTR_SET. It then polls GMU_GMU2HOST_INTR_INFO until bit 0 (MSGQ) reads 1, which took about 110 microseconds each time, and clears it by writing 1 to GMU_GMU2HOST_INTR_CLR. The acknowledgement is then in queue 1.

The GPU power OOB is requested through the same doorbell register, with different bits from the ones a6xx_gmu.xml lists:

    request    GMU_HOST2GMU_INTR_SET = 0x40000000   (bit 30)
    granted    GMU_GMU2HOST_INTR_INFO reads 0x80000000 (bit 31), about 105 us later
    acknowledge GMU_GMU2HOST_INTR_CLR = 0x80000000
    release    GMU_HOST2GMU_INTR_SET = 0x80000000   (bit 31), after the CP has started and gone idle

The XML's A6XX_GMU_OOB bitset (GPU set 18, check and clear 26) does not match what this firmware did. On this GPU, use bits 30 and 31 as observed. The register the OOB bits go in, an open question in version 0, is GMU_HOST2GMU_INTR_SET for requests and releases, and GMU_GMU2HOST_INTR_INFO and GMU_GMU2HOST_INTR_CLR for the grant. The request came after the HFI start messages and the GBIF_SCACHE writes, and before any GX register was touched. The release came after the CP had started, the first fence or idle had been reached, and the host had read RBBM_STATUS [OBS clean/load 456.263484 to 456.263590, 456.305379].

5.5 HFI messages [OBS run1 clean/hang crashdec gmu-hfi, CRASHDEC]

The message header is the one crashdec documents: id in bits 7:0, size in words including the header in bits 15:8, type in bits 19:16 (0 command, 1 acknowledgement), sequence number in bits 31:20. An acknowledgement in queue 1 has the same id, size 3 and type 1, and its payload is the header of the acknowledged message followed by an error word (0 for success).

At every GPU power-up, the host sends these messages in this order:

    1. H2F_MSG_PERF_TABLE (4), 59 words
    2. H2F_MSG_BW_TABLE (3), 160 words
    3. H2F_MSG_CORE_FW_START (14), payload: handle 0
    4. H2F_MSG_START (10), no payload
    (unmask GMU2HOST bit 23)
    5. H2F_MSG_GX_BW_PERF_VOTE (30), payload: ack_type 1, freq = performance index, bw 0

H2F_MSG_INIT, H2F_MSG_FW_VERSION and H2F_MSG_TEST, which crashdec knows, were never sent. Every acknowledgement in the queue reported error 0.

The performance table (59 words) sent to this firmware:

    num_gpu_levels 9, num_gmu_levels 3
    gx levels, each {vote, acd, freq in kHz}:
      [0] 0x00000000  0xffffffff  0
      [1] 0x00400201  0xffffffff  270000
      [2] 0x00800202  0xffffffff  410000
      [3] 0x00c00303  0xffffffff  500000
      [4] 0x00e00404  0xffffffff  547000
      [5] 0x01000505  0xffffffff  606000
      [6] 0x01400606  0xffffffff  640000
      [7] 0x01800707  0xffffffff  655000
      [8] 0x01a00808  0xffffffff  690000
    gmu (cx) levels, each {vote, freq in kHz}:
      [0] 0x00000000  0
      [1] 0x00400202  200000
      [2] 0x00800203  500000

This settles one of version 0's questions: the newer format with an acd word per GX level applies to this firmware, and acd is 0xffffffff (unused) at every level. In each vote, bits 31:16 are the RPMh level of that operating point in the DT (64 for 270 MHz up to 416 for 690 MHz, matching section 2) [CALC, DT live]. The low two bytes are small indices (gx level 1 has 0x02 then 0x01, level 2 has 0x02 then 0x02, and so on). They are probably positions in the RPMh level tables of the MX and GX rails from the command database, but that is not established. A driver can send this table verbatim on this chip.

The bandwidth table (160 words) has a single level and no DDR bandwidth in it: bw_level_num 1, cnoc_cmds_num 1, ddr_cmds_num 3, both wait bitmasks 1, cnoc command address 0x0005003c with data 0x40000000 (level 0) and 0x60000001 (level 1), and DDR command addresses 0x00050004, 0x00050000 and 0x000500ac with data 0x40000000 for the one level. Every GX_BW_PERF_VOTE carried bw 0. The GPU's DDR bandwidth is voted by the host CPU through the interconnect framework instead (section 9.3).

The frequency is chosen by a GX_BW_PERF_VOTE whose freq field is the index into the gx level table: 1 for 270 MHz, 4 for 547 MHz, 8 for 690 MHz [OBS run1 clean/submit 581.689745 (index 1 at power-up), 581.690524 (index 4 on submit), freq 2830.505196 and 2830.509751 (index 8 with the floor raised to 690 MHz)]. Each vote is one doorbell and one acknowledgement. Under Linux's governor, the GPU powered up at 270 MHz, rose to 547 MHz when work arrived, and dropped back about 50 ms after it finished [OBS clean/submit 581.740726].

5.6 Registers named in a6xx_gmu.xml that this sequence uses

All offsets are in the GPU word space (section 3). The ones version 0 already listed are not repeated here.

    GPU_GMU_CX_GMU_CX_FALNEXT_INTF, GPU_GMU_CX_GMU_CX_FAL_INTF, GMU_GENERAL_7, GMU_AHB_FENCE_RANGE_0,
    GPU_GMU_CX_GMU_PWR_COL_CP_MSG, GPU_GMU_CX_GMU_PWR_COL_CP_RESP, GPU_GMU_AO_GMU_CGC_MODE_CNTL/DELAY_CNTL/HYST_CNTL,
    GPU_GMU_AO_GPU_CX_BUSY_MASK, GMU_CX_GMU_POWER_COUNTER_SELECT_0, GMU_CX_GMU_POWER_COUNTER_ENABLE,
    GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L/H, GMU_GMU_PWR_COL_KEEPALIVE, GPU_GMU_CX_GMU_RPMH_POWER_STATE,
    GPU_CC_GX_DOMAIN_MISC (0x24542, at gpucc + 0x1508)

Their CPU addresses follow from section 3. The run1 register listings name each access.


6. GPU SMMU [ARM-SMMU, DT, OBS]

6.1 Ownership

The operating system owns the GPU SMMU completely. Linux programmed the global registers, every stream match and every context bank itself, and made no secure call about the SMMU [OBS run1 clean/boot 1.802501 to 1.803457, clean/load 456.215584 to 456.260565]. Version 0's question whether the OS may program the global registers is answered: it may.

The SMMU loses its state when the GPU's CX domain is off. Linux reprogrammed all of it, the global registers, the stream mapping and both context banks, every time the SMMU's runtime power came back [OBS run1 clean/load 455.944428, 456.215991, 456.259993]. A driver that lets CX collapse has to do the same.

6.2 Discovery and global setup [OBS run1 clean/boot]

    SMMU_IDR0   0x4c017e09   NUMSMRG 9 (bits 7:0)
    SMMU_IDR1   0x30000007   PAGESIZE 4 KB, NUMPAGENDXB 3 (16 pages per half), NUMCB 7
    SMMU_IDR2   0x00005511
    SMMU_IDR7   0x00000024
    SMMU_sCR0   0x00200406 at handoff (CLIENTPD 0, GFRE, GFIE, USFCFG 1)

So context bank n is at 0x03db0000 + n * 0x1000, as version 0 computed [CALC]. Global register space 1 is at 0x03da1000.

The probe at boot, in order: write sCR0 0x00200407 (CLIENTPD 1, bypass everything while programming), clear sGFSR, write every S2CRn 0x00010000 (TYPE bypass) and SMRn 0 for n = 0 to 8, write every CBn SCTLR 0 and FSR 0xc00001fe for n = 0 to 6, invalidate with TLBIALLH and TLBIALLNSNH (0xffffffff), read IDR7, set sACR from 0x1c to 0x51c, set each CBn ACTLR from 0xb to 0x9, sync with sTLBGSYNC and poll sTLBGSTATUS until 0, then write sCR0 0x00201836. After that it measured the SMR fields by writing SMR0 0x00007fff and 0x7fff0000 and reading them back (both read back unchanged: 15 ID bits and 15 mask bits) [OBS run1 clean/boot].

The final sCR0, 0x00201836, has CLIENTPD 0 and USFCFG 0 [ARM-SMMU]: streams that match no SMR bypass translation. A driver that would rather fault on unknown streams can set USFCFG 1, which is what the firmware left at handoff.

6.3 Streams and the GPU context bank [OBS run1 clean/load 456.216170 to 456.216890, 456.260287 to 456.260409]

    SMR0  0x8c000000   VALID, MASK 0x0c00, ID 0     S2CR0 0x00000000   translate, context bank 0   (GPU)
    SMR1  0x8c000001   VALID, MASK 0x0c00, ID 1     S2CR1 0x00000000   translate, context bank 0   (GPU)
    SMR2  0x8c000005   VALID, MASK 0x0c00, ID 5     S2CR2 0x00000001   translate, context bank 1   (GMU)
    SMR3..8  0                                      S2CR3..8 0x00010000 bypass (unused)

The GPU context bank, bank 0, in the order Linux wrote it at attach:

    CB0 ACTLR    0x00000303 (later 0x00000301)
    CBA2R0       0x00000001   VA64, the AArch64 page table format
    CBAR0        0x0001f300   TYPE 1 (stage 1 with stage 2 bypass)
    CB0 TCR2     0x00038011   PASize 1 (36-bit), AS 1 (16-bit ASID), SEP 7 (upstream)
    CB0 TCR      0xb5100080   TTBR0 walks disabled (EPD0), T1SZ 16 (48-bit), TG1 4 KB,
                              inner shareable, write-back write-allocate walks
    CB0 TTBR0    0
    CB0 TTBR1    the kernel's page table (0x12d30f000 in clean/load), ASID 0
    CB0 MAIR0    0xf404ff44   attr0 0x44 normal non-cacheable, attr1 0xff write-back, attr2 0x04 device, attr3 0xf4
    CB0 MAIR1    0xffffffff
    CB0 SCTLR    0x00001167, then 0x000001e7 at the first use, and 0x000011e7 on every later power-up
    S2CR0, S2CR1 = 0 (switch the GPU streams to translate only after the bank is ready)

SCTLR 0x11e7 is M, TRE, AFE, CFRE, CFIE, CFCFG (stall on fault), HUPCF and bit 12 [ARM-SMMU]. MAIR is the same as on the A306.

When a process opens the GPU, its page table goes into TTBR0 and TCR becomes 0xb5103510, which enables TTBR0 walks with T0SZ 16 and the same walk attributes [OBS run1 open 375.701421 and 375.701436]. During submissions the CP switches TTBR0 itself with CP_SMMU_TABLE_UPDATE (4.5).

Which half an address uses: SEP 7 means the sign bit comes from the upstream bus, and the GPU's addresses are 49 bits wide. Bit 48 selects TTBR1. This was confirmed by walking the SQE copy at 0x0001000000009000 through TTBR1 [OBS run1 sqe.txt]. So the GPU address space is:

    0x0000000100000000 .. 0x0000ffffffffffff   per-process buffers, TTBR0 (VA_START to 2^48)
    0x0001000000000000 .. 0x0001ffffffffffff   kernel buffers, TTBR1 (ring, SQE, fences; UCHE_TRAP_BASE at the top)

A first driver with one shared page table can put everything in the TTBR1 half, with TCR EPD0 set, and leave CP_SMMU_TABLE_UPDATE out. This is a choice, not an observation.

6.4 The GMU context bank [OBS run1 clean/load 456.216786 to 456.216844]

    CB1 ACTLR    0x00000303 (later 0x00000301)
    CBA2R1       0x00000001
    CBAR1        0x0001f300
    CB1 TCR2     0x00038011
    CB1 TCR      0x00803510   TTBR1 walks disabled (EPD1), T0SZ 16, the same walk attributes
    CB1 TTBR0    ASID 1, the GMU's own page table (0x0001000139a48000 in clean/load)
    CB1 TTBR1    0x0001000000000000 (ASID 1, no table)
    CB1 MAIR0/1  as CB0
    CB1 SCTLR    0x00001167

The GMU uses its own page table. What it maps, from GMU address 0x4000 up to 0x60407000, is listed in 5.2b [OBS run2]. The CPU is never involved in a GMU translation.

6.5 Context bank registers [ARM-SMMU]

The register offsets inside a bank, and the TLB maintenance after an unmap (TLBIVA or TLBIVAL, then TLBSYNC, then poll TLBSTATUS), are as in version 0 and the A306 spec. At reset Linux invalidated everything with TLBIALLH and TLBIALLNSNH and synchronised with sTLBGSYNC [OBS run1 clean/boot].


7. Firmware and secure calls [WHENCE, DT, OBSD, OBS]

7.1 Files

    SQE microcode    qcom/a660_sqe.fw, sha256 d222f3fe…cf76 in run1. The GPU is given the file from byte 4 (4.2).
    GMU firmware     qcom/a660_gmu.bin, sha256 8acab7b4…e5e7 in run1. It reports version 0x3001000a (5.2).
    Zap shader       qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn, sha256 d7b8cc64…b34d5c in run1 [DT x13s.dts firmware-name].

The a690 loads the a660 SQE and GMU images: linux-firmware has no a690-named files [WHENCE, OBS TheNightman 2026-10-19].

Licences [WHENCE]: the Adreno GPU firmware block and the Lenovo zap shader entry are both "Redistributable. See LICENSE.qcom and NOTICE.qcom for details". These files are not stored in the PureDarwin tree. PureDarwin loads them at first open from:

    /Library/Firmware/qcom/a660_sqe.fw
    /Library/Firmware/qcom/a660_gmu.bin
    /Library/Firmware/qcom/sc8280xp/LENOVO/21BX/qcdxkmsuc8280.mbn

LICENSE.qcom allows the binaries to be redistributed only in binary form, only for platforms with Qualcomm chipsets, with the licence text included and the notices kept intact. It also forbids reverse engineering, disassembling or decompiling them. So nobody on this project looks inside these files.

The GPU state dump, which msm writes on a hang and which debugfs offers as the "gpu" file, contains the SQE microcode. It holds it in the indexed register block CP_SQE_UCODE_DBG (32768 dwords), and at times in CP_ROQ_DBG and CP_MEM_POOL_DBG too. Run1 found runs of four consecutive words of a660_sqe.fw in all three, compared by value without printing them, and removed all three blocks, and every MMIO read of their data registers, from the captures before anything else read them. Anyone repeating these captures must do the same. A PureDarwin driver that writes a similar dump must leave these blocks out, or it would be distributing the microcode inside a crash report.

7.2 Zap shader loading [OBS run1 clean/load 456.266972 to 456.305312, OBSD]

The zap shader is loaded once per boot, on the first GPU power-up, after CP_ME_INIT has been submitted and before CP_SET_SECURE_MODE 0 (4.4). It is not loaded again on warm power-ups [OBS run1 clean/submit, no PAS calls].

The secure calls use the convention OpenBSD's qcscm documents: x0 = (1 << 30, SMC64) | (owner 2, SiP, << 24) | (service << 8) | command, with bit 31 clear (a yielding call), x1 = the argument info word (number of arguments in bits 3:0, a 2-bit type per argument from bit 4), then the arguments [OBSD]. The PIL service is 2. Observed:

    1. PAS_INIT_IMAGE      x0 0x42000201, x1 0x82 (2 arguments, the second a read-write buffer),
                           pas_id 0xd, metadata at physical 0xecd78000.
                           It returned 1 (interrupted). The call was then resumed 39 times with x0 = 1, the same
                           x1 to x3, and x6 going 0x61900273, 0x61900274, ... 0x61900299, each resume about 1 ms
                           later. The last resume returned 0, 38 ms after the first call.
    2. PAS_MEM_SETUP       x0 0x42000202, x1 0x3, pas_id 0xd, address 0x8bf00000, size 0x1000. Returned 0.
    3. PAS_AUTH_AND_RESET  x0 0x42000205, x1 0x1, pas_id 0xd. Returned 0 after 0.4 ms.

The GPU's PAS id is 13 (0xd). The region handed over is the DT carve-out pil_gpu_mem at 0x8bf00000, with size 0x1000 (the image's extent), not the carve-out's 0x2000. The metadata buffer is ordinary memory the kernel allocated. Its content (the image's ELF and hash segments) is what OpenBSD's qcpas prepares for any PAS image [OBSD]. The result codes and the resume convention (x0 = 1 means interrupted, repeat with x0 = 1 and the returned x6) match OpenBSD's handling [OBSD].

Whether the GPU works without the zap shader was not tested. Linux always loaded it, and leaving secure mode (CP_SET_SECURE_MODE 0) depends on it.

7.3 One more secure call at every GPU start [OBS run1 clean/load 456.263711, clean/submit 581.690250]

Immediately before CP_SQE_INSTR_BASE is written, at every power-up including warm ones, Linux made two SiP calls:

    x0 0x42000601, x1 0x1, x2 0x02000c1b     service 6, command 1: is this call available? It returned x1 = 1 (yes).
    x0 0x42000c1b, x1 0x4, x2..x5 0xffff0000 0xffffffff 0xffffffff 0xffffffff
                                             service 0x0c, command 0x1b, four value arguments. It returned 0.

The first is the availability query OpenBSD's qcscm also issues before optional calls [OBSD]. What service 0x0c command 0x1b does is not in any allowed source. A driver can replicate it exactly, with the availability check first.


8. Observation plan and how it was run [OBS]

Run1 was carried out on 19 October 2026 by TheNightman, following runbooks/x13s-a690-observation-runbook.md (outside the tree):

    machine        Lenovo ThinkPad X13s 21BX0015US, BIOS N3HET91W (1.63)
    kernel         Linux 7.0.14 stable (commit 458c6079f), built from Ubuntu 26.04's configuration with MMIO tracing,
                   kprobe events, devcoredump and /dev/mem access. msm built as a module and loaded only inside a capture.
    firmware       linux-firmware 20260319.git217ca6e4 (Ubuntu). Hashes in section 7.1.
    decode tools   Mesa 44fd0fde8187 (cffdump, crashdec, register XML). The system Mesa 26.0.8 ran kmscube.

Each capture traced every MMIO access (the rwmmio tracepoints), every SMC (a kprobe on the generic arm64 SMCCC entry point), clock, runtime PM, interconnect and RPMh events, and the GPU interrupt, around one command. Physical addresses were recovered through /proc/vmallocinfo, and registers were named with the XML. The raw captures stay outside the tree, like the MSM8916 run's. The first pass (folders boot to idle) ran with the kernel also copying every trace event to the console, which slowed it. Timings in this spec therefore come from clean/, a second boot without that, which repeated boot, load, submit and hang with identical register sequences. A cold start took 444 ms from modprobe to suspend under clean/, against 499 ms in the first pass.

Run2, the same day on the same kernel and firmware, answered the one remaining question about the GMU image (5.2a), following runbooks/x13s-a690-gmu-image-runbook.md. It walked the GMU bank's page table through /dev/mem and compared contents by SHA-256, with the GPU held powered.

What each step of version 0's plan yielded:

    1 Identity           done. Section 1 and msm-uapi.md.
    2 SMMU, secure calls done. Section 6 and 7. The OS owns the SMMU, and the zap uses PAS id 13.
    3 Clocks, power      done for the host side. Section 9. GX is the GMU's.
    4 GMU boot           done. Section 5.2 for the registers, 5.2a and 5.2b (run2) for where the image goes.
    5 GPU init           done. Section 4.7.
    6 SQE placement      done. Section 4.2: file byte 4, read-only.
    7 Ring contents      done. Section 4.4 and 4.5, decoded from the hang dump's ring and the rd capture.
    8 HFI traffic        done. Section 5.3 to 5.5.
    9 Idle and power off done. Section 9.4. The content of the one HFI message sent at power-off is open.

One finding about the plan itself: the GPU's cold bring-up happens when the msm module loads, not at the first open, and every later power-up repeats the full GMU boot, the HFI start and the GPU register init. Only the zap shader and the RSCC/PDC programming are once per boot.


9. Clocks, power domains and power off [OBS run1 clean/load, clean/submit, DT]

9.1 The order of a cold power-up [OBS run1 clean/load 456.259878 to 456.263899]

    1. CX GDSC on (gpucc + 0x106c), wait for it (9.2).
    2. GCC branch clocks for the GPU's memory paths: gcc_gpu_memnoc_gfx_clk, gcc_gpu_snoc_dvm_gfx_clk (9.3).
    3. gpucc hub clock root at its boot setting, then gpu_cc_ahb_clk, gpu_cc_hlos1_vote_gpu_smmu_clk,
       gpu_cc_gmu_clk_src, gpu_cc_cx_gmu_clk, gpu_cc_hub_cx_int_clk, gpu_cc_hub_aon_clk.
    4. SMMU reprogrammed (6.2, 6.3, 6.4).
    5. GCC GPLL0 outputs for the GPU voted on, gpu_cc_gmu_clk_src moved to GPLL0 / 2 at 200 MHz, gpu_cc_hub_clk_src
       moved to GPLL0 at 200 MHz.
    6. gcc_ddrss_gpu_axi_clk.
    7. GMU boot and HFI start (5.2, 5.5).
    8. GPU power OOB (5.4), GPU register init (4.7), CP start (4.4).
    9. On the first power-up of a boot: zap shader (7.2), then CP_SET_SECURE_MODE 0.
   10. OOB released.

The RSCC and PDC were programmed once per boot. That happened during an earlier, short power-up while msm was loading, after the SMMU banks were set and before the first GMU boot [OBS run1 clean/load 456.223101 to 456.223119]. The values:

    rscc + 0x010   0x01000000          rscc + 0x02c   1
    rscc + 0x038   0                   rscc + 0x034   0
    rscc + 0x040   0                   rscc + 0x03c   0
    rscc + 0x048   0x80000000          rscc + 0x044   0
    rscc + 0x400   0                   (RSCC_OVERRIDE_START_ADDR)
    rscc + 0x020   0x4520              (RSCC_PDC_SEQ_START_ADDR)
    rscc + 0x024   0x4510              (RSCC_PDC_MATCH_VALUE_LO)
    rscc + 0x028   0x4514              (RSCC_PDC_MATCH_VALUE_HI)
    rscc + 0x600   0xeaaae5a0          (RSCC_SEQ_MEM_0_DRV0, then four more words)
    rscc + 0x604   0xe1a1ebab
    rscc + 0x608   0xa2e0a581
    rscc + 0x60c   0xecac82e2
    rscc + 0x610   0x0020edad
    gmu_pdc + 0x4520   0               (GPU_SEQ_START_ADDR)
    gmu_pdc + 0x4500   0x80000001      (GPU_ENABLE_PDC)

The five sequencer words are written by the host, so they are a value the driver supplies, not firmware. Version 0's question whether the bootloader programs the RSCC and PDC is answered: it does not, the OS does.

9.2 Power domains [OBS run1 clean/load, genpd summaries]

The CX GDSC is the gpucc register at 0x03d9106c. It follows the same model as the A306's oxili GDSC [a306 spec section 2]. Power on: read (0x00282801), clear bit 0 (write 0x00282800), poll gpucc + 0x1540 until bit 31 is set (it read 0x80001fe0 after about 24 polls, 30 microseconds), then 0x106c reads 0xf8282800 with bit 31 (PWR_ON) set. Power off: set bit 0 again (0xf8282801) [OBS clean/load 456.259878 to 456.259911, 456.387919].

The GX GDSC (GPU_CC_GX_GDSCR, gpucc + 0x100c) is switched by the GMU, never by the host (5.1). At power-off the host read it (0x00222801, off) and then wrote GPU_CC_GX_DOMAIN_MISC (gpucc + 0x1508) = 1 [OBS clean/load 456.387771 to 456.387773].

genpd showed cx_gdsc and gx_gdsc off at rest, both under the CX rail [OBS run1 genpd summaries]. The GFX rail that gpucc's DT entry names is voted by the GMU through RPMh (GMU_RPMH_CTRL in 5.2), not by the host.

9.3 Clock registers [OBS run1 clean/load, clk events next to each access]

Each access was attributed by the clock event it sat inside. Offsets are from the controller's base (gcc 0x00100000, gpucc 0x03d90000).

    gcc + 0x52000    vote register    bit 15 gcc_gpu_gpll0_clk_src (600 MHz), bit 16 gcc_gpu_gpll0_div_clk_src (300 MHz)
    gcc + 0x71010    gcc_gpu_memnoc_gfx_clk    read only: 0x00004221 (already on, bit 31 clear)
    gcc + 0x71020    gcc_gpu_snoc_dvm_gfx_clk  read only: 0x00000001
    gcc + 0x7115c    gcc_ddrss_gpu_axi_clk     read only: 0x00300001
    gpucc + 0x117c   gpu_cc_hub_clk_src CMD    \ root clock generator: CFG 0x505 = source 5 (GPLL0), divider field 5 (/3) = 200 MHz
    gpucc + 0x1180   gpu_cc_hub_clk_src CFG    /
    gpucc + 0x1120   gpu_cc_gmu_clk_src CMD    \ CFG 0x602 = source 6 (GPLL0 / 2), divider field 2 (/1.5) = 200 MHz
    gpucc + 0x1124   gpu_cc_gmu_clk_src CFG    /
    gpucc + 0x11bc   gpu_cc_hub_cx_int_div_clk_src divider, read 0 (divide by 1)
    gpucc + 0x11c0   gpu_cc_hub_ahb_div_clk_src divider, read 0
    gpucc + 0x1078   gpu_cc_ahb_clk            bit 0 written 1
    gpucc + 0x5000   gpu_cc_hlos1_vote_gpu_smmu_clk  bit 0 written 1, reads back 1
    gpucc + 0x1098   gpu_cc_cx_gmu_clk         read only: 0x00006221
    gpucc + 0x1204   gpu_cc_hub_cx_int_clk     read only: 1
    gpucc + 0x1178   gpu_cc_hub_aon_clk        read only: 1

The divider field rule is the A306's: divide = (field + 1) / 2 [a306 spec section 2, CALC: 600 / 3 = 200, 300 / 1.5 = 200]. The root clock generator update sequence, observed for both roots, is: write CMD 2, write CFG, read CMD (0x12), write CMD 0x13 (bit 0 update), poll until bit 0 clears (reads 2), write CMD 0. The source numbers 5 and 6 are what set_parent to GPLL0 and GPLL0 / 2 produced [OBS clean/load 456.260681 to 456.260738].

Several enables produced only reads: the clock was already running when Linux enabled it, and it only checked the status bit. A PureDarwin driver should check bit 31 (off) of each of these and set bit 0 if a clock is off. At handoff every gpucc root ran at 19.2 MHz (the crystal) [OBS clean/load clk_summary.before].

The GPU's memory bandwidth is voted by the host through the interconnect framework, on the path from qnm_gpu in the gem NoC to ebi: peak 451000 kBps at power-up (the 270 MHz operating point's value), 2736000 kBps while running fast, and 0 at power-off [OBS run1 clean/load icc_set_bw events]. These are RPMh bus votes made by the CPU, which a PureDarwin driver needs an RPMh path for. Whether the GPU runs with the bootloader's bandwidth, without any vote, was not tested.

9.4 Power off [OBS run1 clean/submit 581.773312 to 581.773460, clean/load 456.387546 to 456.387919]

When the GPU suspends (Linux waited 66 ms of idle, its autosuspend delay), in this order:

    1. Read GPU_GMU_CX_GMU_RPMH_POWER_STATE.
    2. GBIF_HALT = 1, read GBIF_HALT_ACK (3), GBIF_HALT = 2, read GBIF_HALT_ACK (3), GBIF_HALT = 0.
    3. GMU_CX_GMU_POWER_COUNTER_ENABLE = 0.
    4. One HFI message (doorbell and acknowledgement as in 5.4). Its content is not in the captures, because the dumps
       were taken while the GPU was up. It is probably the slumber request.
    5. GMU_AO_AHB_FENCE_CTRL = 0. Read GPU_GMU_AO_GPU_CX_BUSY_STATUS (0x30000000).
    6. GMU_AO_HOST_INTERRUPT_MASK = 0xffffffff, GMU_GMU2HOST_INTR_MASK = 0xffffffff.
    7. Read GPU_CC_GX_GDSCR (GX already off), GPU_CC_GX_DOMAIN_MISC = 1.
    8. Clocks off in the reverse order of 9.1, the GPLL0 votes withdrawn, the root generators back to the crystal (CFG 0).
    9. CX GDSC off (9.2).
   10. Interconnect vote to 0, SMMU runtime power off.

A first driver can keep the GPU powered and skip this. Before a clean shutdown, steps 2 to 9 are the order to follow.

9.5 Hang behaviour [OBS run1 hang, clean/hang]

A submission that never completes (a CP_WAIT_REG_MEM on memory that never changes) was detected as a hang. The GPU state showed RBBM_STATUS 0x00800005, which crashdec decodes as GPU_BUSY_IGN_AHB, CP_BUSY and CP_AHB_BUSY_CX_MASTER, with the IB fetched completely (IB1 remaining 0). Linux recovered, ran the submission a second time, detected the hang again and dropped it [OBS run1 hang, console 1584.415 to 1585.633]. Recovery is out of scope for a first driver.


Open items

Closed by run1:

- GMEM size and base, the exact CHIP_ID, the VA range and MAX_FREQ (section 1).
- The SQE firmware header: one word, skipped (4.2).
- Clock and GDSC registers and the power-on order on the host side (9).
- How the GPU frequency is chosen: by HFI GX_BW_PERF_VOTE index (5.5).
- Whether the OS may program the SMMU: yes, completely. The banks, ASIDs, TCR and MAIR values (6).
- The zap shader's secure call sequence and PAS id (7.2).
- The kernel's ring start packets and per-submission packets, and the fence interrupt (4.4, 4.5).
- The register side of the GMU boot, the RSCC and PDC programming, the HFI queue layout and the start-up messages (5, 9.1).
- Where each block of the GMU image goes (ITCM and DTCM through their windows, the rest in bank 1 RAM at its own address), and the GMU's address map (5.2a, 5.2b, run2).
- Where the OOB bits go (5.4).
- cx_mem and cx_dbgc: not used.

Still open, with the capture that would close each:

- What the host keeps in the GMU ranges 0x445000, 0x446000 and 0x44e000 to 0x1000000 (5.2b), and whether the GMU needs the descriptor regions 0x100000 to 0x3070a0 initialised. Try zeroed memory first, on PureDarwin.
- The HFI message sent at power-off (9.4). Take a GPU state dump through debugfs right after a suspend has started, or record HFI queue 0 at its GMU address through the GMU page table.
- What SiP service 0x0c command 0x1b does (7.3). Try a start without it on PureDarwin and see whether CP_ME_INIT still completes.
- The meaning of the CP_ME_INIT payload words and the low bytes of the GX votes (4.4, 5.5). Both can be sent verbatim.
- The CP_PROTECT entry encoding and the entries past 32 (4.7). Decode from the XML's a6x_cp_protect type, then confirm on PureDarwin with a deliberate protected write.
- Whether the GPU runs without the zap shader, without the bandwidth vote, or with clock gating off. Each is a PureDarwin-side experiment once the observed sequence works.
- The reads and writes at gcc + 0x27010 and gcc + 0x27018 during the load capture were not attributed to any GPU clock event. They are probably another driver's, and no GPU sequence depends on them.
- Hang recovery, preemption, LPAC use, IFPC (inter-frame power collapse) and performance counters. Out of scope for version 1.
