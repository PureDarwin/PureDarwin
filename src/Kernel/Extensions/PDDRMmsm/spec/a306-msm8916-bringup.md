A306 on MSM8916, bring-up spec, version 0


What this is

This spec describes the behaviour needed to take the Adreno 306 from power off to executing command buffers.

Status on 19 October 2026: verified on hardware by the clean-room PureDarwin driver, the PDDRMmsm kernel extension in this tree. It was first brought up as PDAdreno in a separate MSM8916 tree on 18 October. Sections 2 to 6 were implemented as written. Under XNU the command processor (CP) consumed CP_ME_INIT, executed an indirect buffer (IB) containing a CP_MEM_WRITE through the SMMU, and signalled a CACHE_FLUSH_TS fence with its interrupt. The secure page table buffer was a no-map carve-out at 0x9bf00000.

Every value cites its source with one of these tags:

- [OBS run1 ...] is a black-box capture on the UZ801 stick under Linux 7.0.12, in observations/run1/. The tools/ folder explains each file.
- [XML ...] is the freedreno register database (Mesa, MIT), in Mesa's src/freedreno/registers/.
- [DT] is the board device tree (OpenStick dtb0, or mainline msm8916-yiming-uz801v3).
- [LK] is the lk and lk2nd MSM clock framework (MIT and BSD-3).
- [ARM-SMMU] is the ARM System MMU architecture specification, version 2 (public).
- [FW] is the firmware files a300_pm4.fw and a300_pfp.fw (from linux-firmware, under LICENSE.qcom).


1. Resources [DT]

- GPU registers: base 0x01c00000, size 0x20000.
- GPU interrupt: GIC SPI 33, level high.
- GPU SMMU: base 0x01f08000, size 0x10000. Its context banks are at +0x1000 and +0x2000, with interrupts SPI 241 and 242. Its secure id is 0x12.
- Power domain: the GCC GDSC named "oxili", described below. A GDSC is a power switch for one block of the chip, controlled from the global clock controller (GCC).
- Operating points: 19.2 MHz and 400 MHz.


2. Clocks and power domain (GCC at 0x01800000) [OBS run1 clkmap-load.txt, load/trace.txt 47.717]

The register model [LK, and it matches every access in OBS]:

- A branch clock is controlled by its CBCR register. Bit 0 enables it, and bit 31 reads 1 when the clock is off (a read-only status bit).
- A voted branch clock has its enable bit in the APCS vote register at 0x4500c, and its status in bit 31 of the clock's own CBCR.
- A root clock generator has a CMD_RCGR register and a CFG_RCGR register. In CFG_RCGR, bits 10:8 select the source and bits 4:0 hold the divider field, where divide = (field + 1) / 2. After writing CFG_RCGR, set bit 0 (update) in CMD_RCGR and poll until it reads 0.
- A GDSC has bit 0 SW_COLLAPSE, which is cleared to power on, and bit 31 PWR_ON, which is the status.

The registers, named by their Linux clock names, with what turns them on and off:

    gcc_smmu_cfg_clk (vote)   0x4500c bit 12                    on: set, off: clear
    gcc_gfx_tcu_clk (vote)    0x4500c bit 2, status 0x12020      on: set, off: clear
    oxili GDSC                0x5901c                            on: clear bit 0, poll bit 31 = 1, off: set bit 0
    gfx3d_clk_src CFG         0x59004                            0x303 = source 3, /2 = 400 MHz
                                                                 0x31f = source 3, /16 = 50 MHz
    gfx3d_clk_src CMD         0x59000                            write 0x11 after CFG, poll bit 0 = 0
    gcc_oxili_gfx3d_clk       0x59020                            bit 0
    gcc_oxili_ahb_clk         0x59028                            bit 0
    gcc_oxili_gmem_clk        0x59024                            bit 0
    gcc_bimc_gfx_clk          0x31024                            bit 0
    gcc_bimc_gpu_clk          0x31040                            bit 0

Source 3 of gfx3d is the 800 MHz PLL that also feeds the CPU mux as gpll0_vote, which gives 400 MHz at /2 and 50 MHz at /16 [OBS run1 clkmap-load set_rate requests]. Still to do: confirm the source mapping from the parents listed in clk_summary.txt.

The power-on order observed [OBS run1 load/trace.txt 47.717143-47.717443]:

1. Turn the SMMU clocks on (the smmu_cfg vote and the gfx_tcu vote).
2. Turn the oxili GDSC on.
3. Set gfx3d_clk_src to 400 MHz (CFG 0x303, then CMD 0x11).
4. Enable oxili_gfx3d, oxili_ahb, oxili_gmem, bimc_gfx and bimc_gpu.
5. Initialise the GPU registers (section 4).

Power-off is the reverse, with gfx3d dropped to 50 MHz (0x31f) before the GDSC collapses (the GDSC write of a0282001 at 45.403243).

No regulator events touch the GPU [OBS run1: no regulator_* events in the window]. The GPU sits on the CX rail, which is already up.


3. GPU SMMU

3.1 Secure setup, once at boot [OBS run1 boot/trace.txt 1.48-1.50, SMC32 calls]

All of these are SMC calls into the secure world. The function id goes in x0, x1 is the argument descriptor, and x2 onwards are the arguments.

1. x0=0x02000c03 x1=1 x2=0 returns the size of the secure page table (dmesg shows 94208, which is 0x17000).
2. x0=0x02000c04 x1=0x23 x2=(physical address of a 0x17000 byte buffer) x3=0x17000 hands the buffer to the secure world. Linux allocated it at 0x97e20000. The operating system never uses the buffer again.
3. x0=0x02000c02 x1=2 x2=(secure id) x3=(context bank) is made once for each context bank that will be used. For the GPU SMMU that is secure id 0x12 [DT] with banks 1 and 2. Secure id 0x11 with banks 4 and 5 belongs to the other SMMU, the one for MDP and video.

3.2 Context banks [OBS run1 iommu-mmio-decoded.txt 45.254333-45.254377, ARM-SMMU]

Both GPU banks (+0x1000 is bank 1, +0x2000 is bank 2) are programmed the same way, pointing at one shared LPAE page table, with ASIDs 1 and 2. The writes, in order, with register name, offset and value:

    SCTLR           0x000   0 (disable while programming)
    FAR             0x060   0
    FSR             0x058   0xc00001fe (write 1 to clear all faults)
    TTBR0 (64-bit)  0x020   (ASID << 48) | table physical address
    TTBR1 (64-bit)  0x028   0
    TCR2            0x010   0x38002
    TCR             0x030   0x80802020 (EAE, T0SZ 32 for a 4 GB VA space, walk attributes)
    MAIR0           0x038   0xf404ff44
    MAIR1           0x03c   0xffffffff
    SCTLR           0x000   0x10e7 (enable, fault reporting)

The page table format is ARMv8 LPAE stage 1 with a 4 KB granule [ARM-SMMU, TCR value].

TLB maintenance after an unmap: write TLBIVAL (0x620) with (VA >> 12) | (ASID << 48 ...). An observed value is 0x100a001, meaning VA page 0x100a and ASID 1. Then write TLBSYNC (0x7f0) and poll TLBSTATUS (0x7f4).


4. GPU register init [OBS run1 gpu-mmio-collapsed.txt, names from XML a3xx.xml and adreno_common.xml]

Offsets are in dwords within the GPU register window. The writes, in this order, with register name, offset and value:

    VBIF_ROUND_ROBIN_QOS_ARB      0x3049   0x3
    VBIF_OUT_RD_LIM_CONF0         0x3034   0xa
    VBIF_OUT_WR_LIM_CONF0         0x3035   0xa
    RBBM_GPU_BUSY_MASKED          0x0088   0xffffffff
    RBBM_SP_HYST_CNT              0x0012   0x10
    RBBM_WAIT_IDLE_CLOCKS_CTL     0x0033   0x10
    RBBM_AHB_CTL0                 0x0020   0x1
    RBBM_AHB_CTL1                 0x0021   0xa6ffffff
    RBBM_RBBM_CTL                 0x0100   0x30000
    RBBM_INTERFACE_HANG_INT_CTL   0x0050   0x10fff
    UCHE_CACHE_MODE_CONTROL_REG   0x0e82   0x1
    RBBM_CLOCK_CTL                0x0010   0xaaaaaaaa
    RBBM_PERFCTR_CTL              0x0080   0x1
    SP_PERFCOUNTER6_SELECT        0x0eca   0x1d
    SP_PERFCOUNTER7_SELECT        0x0ecb   0xe
    RBBM_INT_0_MASK               0x0063   0x0231ef22
    CP_RB_CNTL                    0x01c1   0x0800020c
    CP_RB_BASE                    0x01c0   ring GPU VA (0x01001000 here)
    CP_PROTECT_CTRL               0x045e   0x7
    CP_PROTECT[0..12]             0x0460.. 0x63000040 0x62000080 0x600000cc 0x60000108
                                           0x64000140 0x66000400 0x65000700 0x610007d8
                                           0x620007e0 0x61001178 0x64001180 0x60003300
                                           0x6b00c000
    CP_DEBUG                      0x01fc   0x0a000000
    CP_ME_RAM_WADDR               0x01f8   0
    CP_ME_RAM_DATA                0x01fa   a300_pm4.fw words 1 to end (2304 words)
    CP_PFP_UCODE_ADDR             0x01c9   0
    CP_PFP_UCODE_DATA             0x01ca   a300_pfp.fw words 1 to end (288 words)
    CP_QUEUE_THRESHOLDS           0x01d5   0x000e0602
    CP_ME_CNTL                    0x01f6   0 (releases the micro engine)

The firmware format [FW + OBS]: little-endian 32-bit words. Word 0 is a header (0 in both files) and is not uploaded. Every following word is written to the data port in order.

The interrupt mask enables these interrupts [XML A3XX_INT0]: RBBM_AHB_ERROR, RBBM_ATB_BUS_OVERFLOW, CP_T0_PACKET_IN_IB, CP_OPCODE_ERROR, CP_RESERVED_BIT_ERROR, CP_HW_FAULT, CP_IB2_INT, CP_IB1_INT, CP_RB_INT, CP_REG_PROTECT_FAULT, CACHE_FLUSH_TS, CP_AHB_ERROR_HALT and UCHE_OOB_ACCESS.

Still to do: decode the CP_RB_CNTL fields from the XML (the state dump gives a ring size of 0x20000 bytes, which would be 32 KB, and there are read pointer update settings), and decode the CP_PROTECT encoding.


5. CP start [OBS run1 ring-decoded.txt, gpu-mmio-collapsed.txt 45.3199]

After CP_ME_CNTL is set to 0, the first thing in the ring is a single packet of 18 words. It is CP_ME_INIT (a type 3 packet with opcode 0x48 [XML adreno_pm4.xml]) with these 17 payload words:

    000003f7 00000000 00000000 00000000 00000080 00000100 00000180 00006600 00000150 0000014e
    00000154 00000001 00000000 00000000 00000000 00000000 00000000

Then CP_RB_WPTR is set to 0x12 (18), and the driver waits until CP_RB_RPTR reaches it with RBBM_STATUS idle.

The packet header encoding [XML adreno_pm4.xml]:

    type 3: (3 << 30) | ((count - 1) << 16) | (opcode << 8)
    type 0: register burst, (0 << 30) | ((count - 1) << 16) | register


6. Submission and fences [OBS run1 ring-decoded.txt words 18-45]

For each user submission the ring gets:

1. CP_INDIRECT_BUFFER_PFD with the IB's GPU VA and its size in dwords.
2. A type 2 nop.
3. A type 0 write of the fence sequence number to CP_SCRATCH_REG2 (0x057a).
4. CP_EVENT_WRITE HLSQ_FLUSH (7).
5. CP_WAIT_FOR_IDLE 0.
6. CP_EVENT_WRITE 0x80000004 with the fence memory's GPU VA and the sequence number. This is CACHE_FLUSH_TS (4) with bit 31 set, which writes the sequence number to memory and raises the CACHE_FLUSH_TS interrupt.

Then CP_RB_WPTR is set to the new write offset, in dwords.

The interrupt: RBBM_INT_0_STATUS (0x0064) shows bit 20 (CACHE_FLUSH_TS). Acknowledge it by writing the same bits to RBBM_INT_CLEAR_CMD (0x0061). The completed sequence number is read from the fence memory, or from CP_SCRATCH_REG2. [OBS run1 a3xx_irq accesses at 47.724892]


7. Not needed for the GPU

The MDSS display block was enabled under Linux only because the msm driver needs it as its DRM master. None of the GPU init above touches MDSS registers or clocks [OBS run1: the GPU time window 47.717-47.725 has no MDSS accesses]. A PureDarwin driver can drive the GPU on its own.


Open items

- The userspace interface is settled. PDDRMmsm speaks the msm uAPI (Mesa's include/drm-uapi/msm_drm.h, MIT), described in msm-uapi.md, and freedreno runs on it unmodified.
- The GPU VA layout. The ring at 0x01001000 and the fence memory at 0x01000004 suggest that kernel buffers sit low in the 4 GB space, with IBs at 0x01025000 and up. Document the allocator only as far as it is needed.
- Hang recovery, preemption (there is none on A3xx), performance counters, and power management (idle collapse).
