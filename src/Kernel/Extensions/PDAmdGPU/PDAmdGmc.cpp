#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>

// gc 10.3 gfxhub registers, all segment 0, from the amdgpu headers (MIT)
#define mmGCVM_L2_CNTL                                  0x15bc
#define mmGCVM_L2_CNTL2                                 0x15bd
#define mmGCVM_L2_CNTL3                                 0x15be
#define mmGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32    0x15cb
#define mmGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32    0x15cc
#define mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_LO32  0x15ce
#define mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_HI32  0x15cf
#define mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_LO32 0x15d0
#define mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_HI32 0x15d1
#define mmGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_LO32 0x15d2
#define mmGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_HI32 0x15d3
#define mmGCVM_L2_CNTL4                                 0x15d4
#define mmGCVM_L2_CNTL5                                 0x15dc
#define mmGCVM_CONTEXT0_CNTL                            0x15fc
#define mmGCVM_INVALIDATE_ENG0_ADDR_RANGE_LO32          0x1643
#define mmGCVM_INVALIDATE_ENG0_ADDR_RANGE_HI32          0x1644
#define mmGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32       0x1667
#define mmGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32       0x1668
#define mmGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32      0x1687
#define mmGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32      0x1688
#define mmGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32        0x16a7
#define mmGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32        0x16a8
#define mmGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB      0x16e8
#define mmGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB      0x16e9
#define mmGCMC_VM_AGP_TOP                               0x16fe
#define mmGCMC_VM_AGP_BOT                               0x16ff
#define mmGCMC_VM_AGP_BASE                              0x1700
#define mmGCMC_VM_SYSTEM_APERTURE_LOW_ADDR              0x1701
#define mmGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR             0x1702
#define mmGCMC_VM_MX_L1_TLB_CNTL                        0x1703

#define mmGCVM_L2_CNTL3_DEFAULT     0x80100007
#define mmGCVM_L2_CNTL4_DEFAULT     0x000000c1
#define mmGCVM_L2_CNTL5_DEFAULT     0x00003fe0

// the gart covers mc 0 to 512 MiB, one 8 byte pte per 4 KiB page, all invalid for now
#define kGartSize       0x20000000ull
#define kGartTableOff   0x4000000
#define kGartTableSize  (kGartSize / 4096 * 8)
#define kScratchPageOff 0x3800000

static inline uint32_t setField(uint32_t v, uint32_t mask, uint32_t field)
{
    return (v & ~mask) | ((field << __builtin_ctz(mask)) & mask);
}

// gfxhub_v2_1_gart_enable: apertures, l1 tlb, l2 cache, context 0 on the gart, identity aperture off
void PDAmdGPU::gfxhubInit(void)
{
    uint64_t tableMc = fbStart + pspBase + kGartTableOff;
    uint64_t tablePa = tableMc - fbStart + fbOffset;
    uint64_t scratchPa = fbStart + pspBase + kScratchPageOff - fbStart + fbOffset;
    uint64_t fbEnd = fbStart + vramSize - 1;
    auto gc = [this](uint32_t off) { return soc15(kHwGC, 0, off); };

    writeVram(pspBase + kGartTableOff, 0, kGartTableSize);
    flushHdp();

    // the root of a depth 0 table is the table itself, by physical address with the valid bit
    uint64_t pd = tablePa | 1;
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_LO32), (uint32_t)pd);
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_BASE_ADDR_HI32), (uint32_t)(pd >> 32));
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_LO32), 0);
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_START_ADDR_HI32), 0);
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_LO32), (uint32_t)((kGartSize - 1) >> 12));
    wreg(gc(mmGCVM_CONTEXT0_PAGE_TABLE_END_ADDR_HI32), (uint32_t)((kGartSize - 1) >> 44));

    // no agp window, the system aperture is the carveout
    wreg(gc(mmGCMC_VM_AGP_BASE), 0);
    wreg(gc(mmGCMC_VM_AGP_BOT), 0xffffff);
    wreg(gc(mmGCMC_VM_AGP_TOP), 0);
    wreg(gc(mmGCMC_VM_SYSTEM_APERTURE_LOW_ADDR), (uint32_t)(fbStart >> 18));
    wreg(gc(mmGCMC_VM_SYSTEM_APERTURE_HIGH_ADDR), (uint32_t)(fbEnd >> 18));
    wreg(gc(mmGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_LSB), (uint32_t)(scratchPa >> 12));
    wreg(gc(mmGCMC_VM_SYSTEM_APERTURE_DEFAULT_ADDR_MSB), (uint32_t)(scratchPa >> 44));
    wreg(gc(mmGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_LO32), (uint32_t)(scratchPa >> 12));
    wreg(gc(mmGCVM_L2_PROTECTION_FAULT_DEFAULT_ADDR_HI32), (uint32_t)(scratchPa >> 44));

    // l1 tlb on, system access mode 3, advanced driver model, uncached
    uint32_t t = rreg(gc(mmGCMC_VM_MX_L1_TLB_CNTL));
    t = setField(t, 0x00000001, 1);
    t = setField(t, 0x00000018, 3);
    t = setField(t, 0x00000040, 1);
    t = setField(t, 0x00000020, 0);
    t = setField(t, 0x00003800, 3);
    wreg(gc(mmGCMC_VM_MX_L1_TLB_CNTL), t);

    t = rreg(gc(mmGCVM_L2_CNTL));
    t = setField(t, 0x00000001, 1);     // enable l2 cache
    t = setField(t, 0x00000002, 0);     // fragment processing
    t = setField(t, 0x00000800, 1);     // default page out to system memory
    t = setField(t, 0x00000100, 0);     // pde0 cache tag generation mode
    t = setField(t, 0x00040000, 0);     // pde fault classification
    t = setField(t, 0x00180000, 1);     // context1 identity access mode
    t = setField(t, 0x03e00000, 0);     // identity mode fragment size
    wreg(gc(mmGCVM_L2_CNTL), t);
    wreg(gc(mmGCVM_L2_CNTL2), rreg(gc(mmGCVM_L2_CNTL2)) | 0x3);    // invalidate l1 tlbs and l2
    // translate_further is off on this apu, so bank select 9 and bigk fragment 6, as amdgpu leaves it
    t = setField(mmGCVM_L2_CNTL3_DEFAULT, 0x0000003f, 9);
    wreg(gc(mmGCVM_L2_CNTL3), setField(t, 0x000f8000, 6));
    wreg(gc(mmGCVM_L2_CNTL4), mmGCVM_L2_CNTL4_DEFAULT & ~0xc0u);
    wreg(gc(mmGCVM_L2_CNTL5), setField(mmGCVM_L2_CNTL5_DEFAULT, 0x0000001f, 0));

    t = rreg(gc(mmGCVM_CONTEXT0_CNTL));
    t = setField(t, 0x00000001, 1);
    t = setField(t, 0x00000006, 0);
    t = setField(t, 0x00000080, 0);
    wreg(gc(mmGCVM_CONTEXT0_CNTL), t);

    wreg(gc(mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_LO32), 0xffffffff);
    wreg(gc(mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_LOW_ADDR_HI32), 0xf);
    wreg(gc(mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_LO32), 0);
    wreg(gc(mmGCVM_L2_CONTEXT1_IDENTITY_APERTURE_HIGH_ADDR_HI32), 0);
    wreg(gc(mmGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_LO32), 0);
    wreg(gc(mmGCVM_L2_CONTEXT_IDENTITY_PHYSICAL_OFFSET_HI32), 0);

    // the 18 invalidation engines sit two registers apart
    for (uint32_t i = 0; i < 18; i++) {
        wreg(gc(mmGCVM_INVALIDATE_ENG0_ADDR_RANGE_LO32) + i * 2, 0xffffffff);
        wreg(gc(mmGCVM_INVALIDATE_ENG0_ADDR_RANGE_HI32) + i * 2, 0x1f);
    }

    LOG("gmc: gfxhub up, gart table pa 0x%llx, system aperture 0x%llx-0x%llx, L2_CNTL 0x%08x CONTEXT0_CNTL 0x%08x\n",
        tablePa, fbStart, fbEnd, rreg(gc(mmGCVM_L2_CNTL)), rreg(gc(mmGCVM_CONTEXT0_CNTL)));
}
