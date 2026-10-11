#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

// sdma 5.2 registers, offsets in gc segment 0 from the amdgpu headers (MIT)
#define mmSDMA0_CNTL                    0x001c
#define mmSDMA0_STATUS_REG              0x0025
#define mmSDMA0_UCODE_CHECKSUM          0x0029
#define mmSDMA0_GFX_CONTEXT_STATUS      0x0091
#define mmSDMA0_GFX_STATUS              0x00a8
#define mmSDMA0_F32_CNTL                0x002a
#define mmSDMA0_GFX_RB_CNTL             0x0080
#define mmSDMA0_GFX_RB_BASE             0x0081
#define mmSDMA0_GFX_RB_BASE_HI          0x0082
#define mmSDMA0_GFX_RB_RPTR             0x0083
#define mmSDMA0_GFX_RB_RPTR_HI          0x0084
#define mmSDMA0_GFX_RB_WPTR             0x0085
#define mmSDMA0_GFX_RB_WPTR_HI          0x0086
#define mmSDMA0_GFX_RB_RPTR_ADDR_HI     0x0088
#define mmSDMA0_GFX_RB_RPTR_ADDR_LO     0x0089
#define mmSDMA0_GFX_IB_CNTL             0x008a
#define mmSDMA0_GFX_DOORBELL            0x0092
#define mmSDMA0_GFX_RB_WPTR_POLL_CNTL   0x0087
#define mmSDMA0_GFX_RB_WPTR_POLL_ADDR_HI 0x00b2
#define mmSDMA0_GFX_RB_WPTR_POLL_ADDR_LO 0x00b3
#define mmSDMA0_GFX_MINOR_PTR_UPDATE    0x00b5

#define mmGRBM_STATUS                   0x0da4
#define mmGRBM_SOFT_RESET               0x0da8
#define mmSDMA0_UTCL1_CNTL              0x003c

#define kSoftResetSdma0     0x00800000
#define kCntlUtcL1Enable    0x00000002
#define kCntlMidcmdPreempt  0x00000020
#define kCntlAutoCtxsw      0x00040000

#define kRbEnable           0x00000001
#define kRbRptrWriteback    0x00001000
#define kF32Halt            0x00000001
#define kIbEnable           0x00000001
#define kDoorbellEnable     0x10000000

// navi10_sdma_pkt_open.h: op in bits 0-7, sub op in bits 8-15
#define kSdmaOpWrite        2
#define kSdmaSubWriteLinear 0

// carveout layout from sdmaBase: the ring, then the rptr writeback and the test word
#define kSdmaRingSize       0x1000
#define kSdmaRptrOff        0x1000
#define kSdmaTestOff        0x1010
#define kSdmaWptrOff        0x1020
#define kF32PollEnable      0x00000004

uint32_t PDAmdGPU::sdmaReg(uint32_t off)
{
    return soc15(kHwGC, 0, off);
}

// brings up the sdma0 gfx ring the way sdma_v5_2_gfx_resume_instance does, then runs amdgpu's ring test
void PDAmdGPU::sdmaTest(void)
{
    uint64_t base = pspBase + 0x2000000;
    uint64_t ringMc = fbStart + base;
    uint64_t rptrMc = fbStart + base + kSdmaRptrOff;
    uint64_t testMc = fbStart + base + kSdmaTestOff;

    writeVram(base, 0, kSdmaRingSize + 0x100);
    writeVram(base + kSdmaTestOff, 0xcafedead, 4);

    // sdma_v5_2_start: soft reset, unhalt, context switching on, then the ring
    uint32_t grbm = soc15(kHwGC, 0, mmGRBM_SOFT_RESET);
    wreg(grbm, rreg(grbm) | kSoftResetSdma0);
    (void)rreg(grbm);
    IODelay(50);
    wreg(grbm, rreg(grbm) & ~kSoftResetSdma0);
    (void)rreg(grbm);
    IODelay(50);

    wreg(sdmaReg(mmSDMA0_F32_CNTL), rreg(sdmaReg(mmSDMA0_F32_CNTL)) & ~kF32Halt);
    wreg(sdmaReg(mmSDMA0_CNTL), rreg(sdmaReg(mmSDMA0_CNTL)) | kCntlAutoCtxsw);

    // ring size is log2 of the dword count, in bits 1-5
    uint32_t rbCntl = (rreg(sdmaReg(mmSDMA0_GFX_RB_CNTL)) & ~(0x1fu << 1) & ~kRbEnable) | (10 << 1);
    wreg(sdmaReg(mmSDMA0_GFX_RB_CNTL), rbCntl);
    wreg(sdmaReg(mmSDMA0_GFX_RB_RPTR), 0);
    wreg(sdmaReg(mmSDMA0_GFX_RB_RPTR_HI), 0);
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR), 0);
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_HI), 0);
    // the engine polls a shadow copy of wptr, so it has to point at one we keep current
    uint64_t wptrMc = fbStart + base + kSdmaWptrOff;
    uint32_t pollCntl = rreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_POLL_CNTL));
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_POLL_ADDR_LO), (uint32_t)wptrMc);
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_POLL_ADDR_HI), (uint32_t)(wptrMc >> 32));
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_POLL_CNTL), pollCntl | kF32PollEnable);
    LOG("sdma: wptr poll cntl was 0x%08x\n", pollCntl);

    wreg(sdmaReg(mmSDMA0_GFX_RB_RPTR_ADDR_HI), (uint32_t)(rptrMc >> 32));
    wreg(sdmaReg(mmSDMA0_GFX_RB_RPTR_ADDR_LO), (uint32_t)rptrMc & ~3u);
    rbCntl |= kRbRptrWriteback;
    wreg(sdmaReg(mmSDMA0_GFX_RB_BASE), (uint32_t)(ringMc >> 8));
    wreg(sdmaReg(mmSDMA0_GFX_RB_BASE_HI), (uint32_t)(ringMc >> 40));

    wreg(sdmaReg(mmSDMA0_GFX_MINOR_PTR_UPDATE), 1);
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR), 0);
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_HI), 0);
    wreg(sdmaReg(mmSDMA0_GFX_DOORBELL), rreg(sdmaReg(mmSDMA0_GFX_DOORBELL)) & ~kDoorbellEnable);
    wreg(sdmaReg(mmSDMA0_GFX_MINOR_PTR_UPDATE), 0);

    wreg(sdmaReg(mmSDMA0_CNTL), rreg(sdmaReg(mmSDMA0_CNTL)) | kCntlUtcL1Enable | kCntlMidcmdPreempt);
    // resp mode 3 (non-copy addresses), redo delay 9
    uint32_t utcl1 = rreg(sdmaReg(mmSDMA0_UTCL1_CNTL)) & ~0x00000e3eu;
    wreg(sdmaReg(mmSDMA0_UTCL1_CNTL), utcl1 | (3 << 9) | (9 << 1));
    wreg(sdmaReg(mmSDMA0_F32_CNTL), rreg(sdmaReg(mmSDMA0_F32_CNTL)) & ~kF32Halt);
    wreg(sdmaReg(mmSDMA0_GFX_RB_CNTL), rbCntl | kRbEnable);
    wreg(sdmaReg(mmSDMA0_GFX_IB_CNTL), rreg(sdmaReg(mmSDMA0_GFX_IB_CNTL)) | kIbEnable);

    LOG("sdma: regs at 0x%x, RB_CNTL 0x%08x (wrote 0x%08x) RB_BASE 0x%08x (wrote 0x%08x) GRBM_STATUS 0x%08x\n",
        sdmaReg(0), rreg(sdmaReg(mmSDMA0_GFX_RB_CNTL)), rbCntl | kRbEnable, rreg(sdmaReg(mmSDMA0_GFX_RB_BASE)),
        (uint32_t)(ringMc >> 8), rreg(soc15(kHwGC, 0, mmGRBM_STATUS)));

    uint32_t pkt[5] = {
        kSdmaOpWrite | (kSdmaSubWriteLinear << 8),
        (uint32_t)testMc,
        (uint32_t)(testMc >> 32),
        0,                  // count, one dword
        0xdeadbeef,
    };
    copyToVram(base, pkt, sizeof(pkt));
    uint32_t shadow[2] = { (uint32_t)sizeof(pkt), 0 };
    copyToVram(base + kSdmaWptrOff, shadow, sizeof(shadow));
    flushHdp();
    // the 64-bit wptr commits on the high half, as sdma_v5_2_ring_set_wptr writes it
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR), sizeof(pkt));
    wreg(sdmaReg(mmSDMA0_GFX_RB_WPTR_HI), 0);

    uint32_t test = 0;
    for (int i = 0; i < 1000 && test != 0xdeadbeef; i++) {
        IODelay(100);
        readVram(base + kSdmaTestOff, &test, 4);
    }
    uint32_t rptrWb = 0;
    readVram(base + kSdmaRptrOff, &rptrWb, 4);
    if (test != 0xdeadbeef) dumpGfxState("sdma ring test");
    LOG("sdma: ring test %s, word 0x%08x, rptr %u (writeback %u) wptr %u, status 0x%08x f32 0x%08x\n",
        test == 0xdeadbeef ? "passed" : "failed", test, rreg(sdmaReg(mmSDMA0_GFX_RB_RPTR)), rptrWb,
        rreg(sdmaReg(mmSDMA0_GFX_RB_WPTR)), rreg(sdmaReg(mmSDMA0_STATUS_REG)), rreg(sdmaReg(mmSDMA0_F32_CNTL)));
}

// every sdma0 register plus the gc status and vm setup, eight to a line
void PDAmdGPU::dumpGfxState(const char *why)
{
    static const struct { const char *name; uint8_t seg; uint32_t off; } gc[] = {
        { "GRBM_STATUS", 0, 0x0da4 }, { "GRBM_STATUS2", 0, 0x0da2 }, { "GRBM_STATUS_SE0", 0, 0x0da5 },
        { "CP_STAT", 0, 0x0f40 }, { "RLC_STAT", 1, 0x4c04 }, { "GCVM_L2_CNTL", 0, 0x15bc },
        { "GCVM_L2_PROTECTION_FAULT_STATUS", 0, 0x15c8 }, { "GCVM_L2_PROTECTION_FAULT_ADDR_LO32", 0, 0x15c9 },
        { "GCVM_L2_PROTECTION_FAULT_ADDR_HI32", 0, 0x15ca }, { "RLC_CSIB_ADDR_LO", 1, 0x4ca2 },
        { "RLC_CSIB_LENGTH", 1, 0x4ca4 }, { "CP_ME_CNTL", 0, 0x0f56 }, { "CP_RB0_RPTR", 0, 0x0f60 },
        { "CP_CPF_STATUS", 0, 0x0e27 }, { "CP_CPF_BUSY_STAT", 0, 0x0e28 }, { "CP_CPF_STALLED_STAT1", 0, 0x0e29 },
        { "CP_ME_HEADER_DUMP", 0, 0x0f41 }, { "CP_PFP_HEADER_DUMP", 0, 0x0f42 }, { "CP_GFX_HQD_ACTIVE", 0, 0x1e80 },
        { "RLC_CP_SCHEDULERS", 1, 0x4ca1 },
        { "CP_PFP_IC_BASE_LO", 1, 0x5840 }, { "CP_PFP_IC_BASE_HI", 1, 0x5841 }, { "CP_PFP_IC_BASE_CNTL", 1, 0x5842 },
        { "CP_ME_IC_BASE_LO", 1, 0x5844 }, { "CP_ME_IC_BASE_HI", 1, 0x5845 }, { "CP_ME_IC_BASE_CNTL", 1, 0x5846 },
        { "CP_CE_IC_BASE_LO", 1, 0x5848 }, { "CP_CE_IC_BASE_HI", 1, 0x5849 }, { "CP_CPC_IC_BASE_LO", 1, 0x584c }, { "GCVM_CONTEXT0_CNTL", 0, 0x15fc },
        { "GCMC_VM_FB_LOCATION_BASE", 0, 0x16fc }, { "GCMC_VM_FB_LOCATION_TOP", 0, 0x16fd },
        { "GCMC_VM_SYSTEM_APERTURE_LOW", 0, 0x1701 }, { "GCMC_VM_SYSTEM_APERTURE_HIGH", 0, 0x1702 },
        { "GCMC_VM_MX_L1_TLB_CNTL", 0, 0x1703 },
    };

    LOG("dump after %s failure\n", why);
    for (auto &r : gc)
        LOG("  %-32s 0x%08x\n", r.name, rreg(soc15(kHwGC, r.seg, r.off)));
    for (uint32_t off = 0; off < 0xc0; off += 8) {
        uint32_t v[8];
        for (int i = 0; i < 8; i++) v[i] = rreg(sdmaReg(off + i));
        LOG("  sdma0+%03x %08x %08x %08x %08x %08x %08x %08x %08x\n", off, v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]);
    }
}

// fault status plus where the cp halts sit, to see which step a fault shows up after
void PDAmdGPU::traceFault(const char *step)
{
    LOG("trace %s: fault status 0x%08x addr 0x%08x, CP_ME_CNTL 0x%08x CP_STAT 0x%08x\n", step,
        rreg(soc15(kHwGC, 0, 0x15c8)), rreg(soc15(kHwGC, 0, 0x15c9)), rreg(soc15(kHwGC, 0, 0x0f56)),
        rreg(soc15(kHwGC, 0, 0x0f40)));
}
