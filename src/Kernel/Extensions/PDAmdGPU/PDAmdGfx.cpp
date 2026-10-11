#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

#include "clearstate_defs.h"
#include "clearstate_gfx10.h"
#include "v10_structs.h"

// gc 10.3 cp registers, segment 0 unless noted, from the amdgpu headers (MIT)
#define mmCP_ME_CNTL                0x0f56
#define mmCP_STAT                   0x0f40
#define mmCP_RB0_RPTR               0x0f60
#define mmCP_RB_WPTR_DELAY          0x0f61
#define mmCP_RB_WPTR_POLL_CNTL      0x0f62
#define mmCP_RB0_BASE               0x1de0
#define mmCP_RB0_CNTL               0x1de1
#define mmCP_RB0_RPTR_ADDR          0x1de3
#define mmCP_RB0_RPTR_ADDR_HI       0x1de4
#define mmCP_DEVICE_ID              0x1deb
#define mmCP_RB_VMID                0x1df1
#define mmCP_RB0_WPTR               0x1df4
#define mmCP_RB0_WPTR_HI            0x1df5
#define mmCP_MAX_CONTEXT            0x1e4e
#define mmCP_RB0_BASE_HI            0x1e51
#define mmCP_RB_WPTR_POLL_ADDR_LO   0x1e8b
#define mmCP_RB_WPTR_POLL_ADDR_HI   0x1e8c
#define mmCP_RB_DOORBELL_CONTROL    0x1e8d
#define mmCP_RB_ACTIVE              0x1f40
#define mmCP_RB_DOORBELL_RANGE_LOWER 0x1dfa
#define mmCP_RB_DOORBELL_RANGE_UPPER 0x1dfb
#define regRCC_DOORBELL_APER_EN     0x00c0      // nbio segment 2
#define mmCP_GFX_MQD_BASE_ADDR      0x1e7e
#define mmCP_GFX_MQD_BASE_ADDR_HI   0x1e7f
#define mmCP_GFX_MQD_CONTROL        0x1e9a
#define mmCP_GFX_HQD_ACTIVE         0x1e80
#define mmCP_GFX_HQD_VMID           0x1e81
#define mmCP_GFX_HQD_QUEUE_PRIORITY 0x1e84
#define mmCP_GFX_HQD_QUANTUM        0x1e85
#define mmCP_GFX_HQD_BASE           0x1e86
#define mmCP_GFX_HQD_BASE_HI        0x1e87
#define mmCP_GFX_HQD_RPTR           0x1e88
#define mmCP_GFX_HQD_RPTR_ADDR      0x1e89
#define mmCP_GFX_HQD_RPTR_ADDR_HI   0x1e8a
#define mmCP_GFX_HQD_CNTL           0x1e8f
#define mmCP_GFX_HQD_WPTR           0x1e91
#define mmCP_GFX_HQD_WPTR_HI        0x1e92
#define mmSCRATCH_REG0              0x2040      // segment 1
#define mmRLC_SRM_CNTL              0x4c80      // segment 1
#define mmRLC_CP_SCHEDULERS         0x4ca1      // segment 1
#define mmRLC_CSIB_ADDR_LO          0x4ca2      // segment 1
#define mmRLC_CSIB_ADDR_HI          0x4ca3      // segment 1
#define mmRLC_CSIB_LENGTH           0x4ca4      // segment 1
#define mmPA_SC_TILE_STEERING_OVERRIDE 0x00d7   // segment 1

#define kSrmAutoIncr    0x00000002
#define kSrmEnable      0x00000001

#define kCpHaltMask     (0x01000000 | 0x04000000 | 0x10000000)  // ce, pfp, me

// nvd.h
#define PACKET3(op, n)              ((3u << 30) | (((op) & 0xff) << 8) | (((n) & 0x3fff) << 16))
#define PACKET3_SET_UCONFIG_REG     0x79
#define PACKET3_SET_UCONFIG_START   0x0000c000
#define PACKET3_SET_BASE            0x11
#define PACKET3_CLEAR_STATE         0x12
#define PACKET3_CONTEXT_CONTROL     0x28
#define PACKET3_PREAMBLE_CNTL       0x4a
#define PACKET3_SET_CONTEXT_REG     0x69
#define PACKET3_SET_CONTEXT_START   0x0000a000
#define kPreambleBegin              (2u << 28)
#define kPreambleEnd                (3u << 28)
#define kCePartitionBase            3

#define kCsbOff         0x3900000
#define kCsbMaxDwords   1024

// carveout layout from gfxBase: 4 KiB ring, then the rptr writeback and the wptr shadow
#define kGfxRingSize    0x1000
#define kGfxRptrOff     0x1000
#define kGfxWptrOff     0x1010
#define kGfxMqdOff      0x2000

// amdgpu's gfx ring 0 doorbell, AMDGPU_NAVI10_DOORBELL_GFX_RING0 doubled for a 64-bit doorbell, in dwords into bar2
#define kGfxDoorbell        (0x8b << 1)
#define kDoorbellEn         0x40000000

// golden_settings_gc_10_3_6, at the gc 10.1 offsets gfx_v10_0.c is built with
static const struct { const char *name; uint8_t seg; uint16_t off; uint32_t andMask, orMask; } kGolden[] = {
    { "CGTT_SPI_CS_CLK_CTRL", 1, 0x507c, 0xff7f0fff, 0x78000100 },
    { "CH_PIPE_STEER", 1, 0x2d90, 0x000000ff, 0x00000044 },
    { "CPF_GCR_CNTL", 0, 0x1f53, 0x0007ffff, 0x0000c200 },
    { "DB_DEBUG3", 0, 0x13ae, 0xffffffff, 0x00000280 },
    { "DB_DEBUG4", 0, 0x13af, 0xffffffff, 0x00800000 },
    { "GB_ADDR_CONFIG", 0, 0x13de, 0x0c1807ff, 0x00000042 },
    { "GCR_GENERAL_CNTL", 0, 0x1580, 0x1ff1ffff, 0x00000500 },
    { "GL1_PIPE_STEER", 1, 0x2d10, 0x000000ff, 0x00000044 },
    { "GL2_PIPE_STEER_0", 1, 0x2e25, 0x77777777, 0x32103210 },
    { "GL2_PIPE_STEER_1", 1, 0x2e26, 0x77777777, 0x32103210 },
    { "GL2A_ADDR_MATCH_MASK", 1, 0x2e21, 0xffffffff, 0xfffffff3 },
    { "GL2C_ADDR_MATCH_MASK", 1, 0x2e03, 0xffffffff, 0xfffffff3 },
    { "GL2C_CM_CTRL1", 1, 0x2e08, 0xff8fff0f, 0x580f1008 },
    { "GL2C_CTRL3", 1, 0x2e0c, 0xf7ffffff, 0x00f80988 },
    { "LDS_CONFIG", 0, 0x10a2, 0x000001ff, 0x00000020 },
    { "PA_CL_ENHANCE", 0, 0x1025, 0xf17fffff, 0x01200007 },
    { "PA_SC_BINNER_TIMEOUT_COUNTER", 0, 0x1070, 0xffffffff, 0x00000800 },
    { "PA_SC_ENHANCE_2", 0, 0x107c, 0xffffffbf, 0x00000820 },
    { "SQG_CONFIG", 0, 0x10ba, 0x000017ff, 0x00001000 },
    { "SX_DEBUG_1", 0, 0x11b8, 0xffffff7f, 0x00010020 },
    { "TA_CNTL_AUX", 0, 0x12e2, 0xfff7ffff, 0x01030000 },
    { "UTCL1_CTRL", 0, 0x1588, 0xffffffff, 0x00100000 },
};

// soc15_program_register_sequence: a full mask writes the value, otherwise only the masked bits change
void PDAmdGPU::goldenInit(void)
{
    for (auto &g : kGolden) {
        uint32_t reg = soc15(kHwGC, g.seg, g.off);
        uint32_t v = g.andMask == 0xffffffff ? g.orMask : (rreg(reg) & ~g.andMask) | (g.orMask & g.andMask);
        wreg(reg, v);
    }
    LOG("gfx: %u golden registers set, GB_ADDR_CONFIG 0x%08x CPF_GCR_CNTL 0x%08x\n", (unsigned)(sizeof(kGolden) / sizeof(kGolden[0])),
        rreg(soc15(kHwGC, 0, 0x13de)), rreg(soc15(kHwGC, 0, 0x1f53)));
}

// the clear state preamble shared by the rlc csb and the ring prologue, as amdgpu_gfx_csb_* emit it
static uint32_t emitClearState(uint32_t *b)
{
    uint32_t n = 0;

    b[n++] = PACKET3(PACKET3_PREAMBLE_CNTL, 0);
    b[n++] = kPreambleBegin;
    b[n++] = PACKET3(PACKET3_CONTEXT_CONTROL, 1);
    b[n++] = 0x80000000;
    b[n++] = 0x80000000;
    for (const cs_section_def *sect = gfx10_cs_data; sect->section; sect++) {
        for (const cs_extent_def *ext = sect->section; ext->extent; ext++) {
            if (sect->id != SECT_CONTEXT) continue;
            b[n++] = PACKET3(PACKET3_SET_CONTEXT_REG, ext->reg_count);
            b[n++] = ext->reg_index - PACKET3_SET_CONTEXT_START;
            for (uint32_t i = 0; i < ext->reg_count; i++)
                b[n++] = ext->extent[i];
        }
    }

    // gc 10.3 wants the tile steering override at 0
    b[n++] = PACKET3(PACKET3_SET_CONTEXT_REG, 1);
    b[n++] = mmPA_SC_TILE_STEERING_OVERRIDE;
    b[n++] = 0;
    b[n++] = PACKET3(PACKET3_PREAMBLE_CNTL, 0);
    b[n++] = kPreambleEnd;
    b[n++] = PACKET3(PACKET3_CLEAR_STATE, 0);
    b[n++] = 0;
    return n;
}

// gfx_v10_0_rlc_resume after autoload: point the rlc at the clear state buffer and turn on save/restore
void PDAmdGPU::rlcInit(void)
{
    uint32_t *csb = (uint32_t *)IOMallocZeroData(kCsbMaxDwords * 4);
    if (!csb) return;

    uint32_t n = emitClearState(csb);
    uint64_t csbMc = fbStart + pspBase + kCsbOff;
    copyToVram(pspBase + kCsbOff, csb, n * 4);
    flushHdp();
    IOFreeData(csb, kCsbMaxDwords * 4);

    wreg(soc15(kHwGC, 1, mmRLC_CSIB_ADDR_HI), (uint32_t)(csbMc >> 32));
    wreg(soc15(kHwGC, 1, mmRLC_CSIB_ADDR_LO), (uint32_t)csbMc & ~3u);
    wreg(soc15(kHwGC, 1, mmRLC_CSIB_LENGTH), n);
    wreg(soc15(kHwGC, 1, mmRLC_SRM_CNTL), rreg(soc15(kHwGC, 1, mmRLC_SRM_CNTL)) | kSrmAutoIncr | kSrmEnable);
    LOG("rlc: csb %u dwords at mc 0x%llx, srm on\n", n, csbMc);
}

// amdgpu maps the gfx queue through the kiq by default, the direct ring is its async_gfx_ring=0 path,
// which fails its ring test on gc 10.3.6 under amdgpu too, so it stays behind pdamdgpu_legacy_gfx=1
void PDAmdGPU::gfxStart(void)
{
    uint32_t legacy = 0;

    PE_parse_boot_argn("pdamdgpu_legacy_gfx", &legacy, sizeof(legacy));
    LOG("gfx: %s path\n", legacy ? "legacy ring" : "kiq");
    gfxTest(!legacy);
}

// gfx_v10_0_cp_gfx_resume and cp_gfx_start for ring 0, then gfx_v10_0_ring_test_ring
void PDAmdGPU::gfxTest(bool kiq)
{
    uint64_t base = pspBase + 0x3000000;
    uint64_t ringMc = fbStart + base;
    uint64_t rptrMc = ringMc + kGfxRptrOff;
    uint64_t wptrMc = ringMc + kGfxWptrOff;
    uint32_t scratch = soc15(kHwGC, 1, mmSCRATCH_REG0);

    writeVram(base, 0, kGfxMqdOff + 0x1000);

    // gfx 10.3 runs the gfx ring as a hardware queue: build its mqd the way gfx_v10_0_gfx_mqd_init does,
    // then load the same values into the hqd registers, which is what the kiq's map_queues would do
    uint64_t mqdMc = ringMc + kGfxMqdOff;
    auto gc = [this](uint32_t off) { return soc15(kHwGC, 0, off); };
    v10_gfx_mqd *mqd = (v10_gfx_mqd *)IOMallocZeroData(sizeof(v10_gfx_mqd));
    if (!mqd) return;
    mqd->cp_mqd_base_addr = (uint32_t)mqdMc & ~3u;
    mqd->cp_mqd_base_addr_hi = (uint32_t)(mqdMc >> 32);
    mqd->cp_gfx_mqd_control = (rreg(gc(mmCP_GFX_MQD_CONTROL)) & ~0x10fu) | 0x100;     // vmid 0, priv state
    mqd->cp_gfx_hqd_vmid = 0;
    mqd->cp_gfx_hqd_queue_priority = rreg(gc(mmCP_GFX_HQD_QUEUE_PRIORITY)) & ~0xfu;
    mqd->cp_gfx_hqd_quantum = rreg(gc(mmCP_GFX_HQD_QUANTUM)) | 0x1;
    mqd->cp_gfx_hqd_base = (uint32_t)(ringMc >> 8);
    mqd->cp_gfx_hqd_base_hi = (uint32_t)(ringMc >> 40);
    mqd->cp_gfx_hqd_rptr_addr = (uint32_t)rptrMc & ~3u;
    mqd->cp_gfx_hqd_rptr_addr_hi = (uint32_t)(rptrMc >> 32) & 0xffff;
    mqd->cp_rb_wptr_poll_addr_lo = (uint32_t)wptrMc & ~3u;
    mqd->cp_rb_wptr_poll_addr_hi = (uint32_t)(wptrMc >> 32) & 0xffff;
    mqd->cp_rb_doorbell_control = (kGfxDoorbell << 2) | kDoorbellEn;
    // rb_bufsz is log2 of the size in dwords minus one
    mqd->cp_gfx_hqd_cntl = (rreg(gc(mmCP_GFX_HQD_CNTL)) & ~0x3f3fu) | 9 | (7 << 8);
    mqd->cp_gfx_hqd_rptr = rreg(gc(mmCP_GFX_HQD_RPTR));
    mqd->cp_gfx_hqd_wptr = 0;
    mqd->cp_gfx_hqd_wptr_hi = 0;
    mqd->cp_gfx_hqd_active = 1;
    copyToVram(base + kGfxMqdOff, mqd, sizeof(v10_gfx_mqd));
    flushHdp();

    // gfx_v10_0_cp_gfx_set_doorbell plus nbio's aperture enable, both paths
    wreg(soc15(kHwNBIF, 2, regRCC_DOORBELL_APER_EN), rreg(soc15(kHwNBIF, 2, regRCC_DOORBELL_APER_EN)) | 0x1);
    wreg(soc15(kHwGC, 0, mmCP_RB_DOORBELL_CONTROL), (kGfxDoorbell << 2) | kDoorbellEn);
    wreg(soc15(kHwGC, 0, mmCP_RB_DOORBELL_RANGE_LOWER), (kGfxDoorbell << 2) & 0xffc);
    wreg(soc15(kHwGC, 0, mmCP_RB_DOORBELL_RANGE_UPPER), 0xffc);

    if (kiq) {
        // the async path: the kiq maps the queue from the mqd, nothing loads the gfx hqd by hand
        IOFreeData(mqd, sizeof(v10_gfx_mqd));
        if (!kiqMapGfx(mqdMc, wptrMc)) return;
    } else {
    wreg(gc(mmCP_GFX_MQD_BASE_ADDR), mqd->cp_mqd_base_addr);
    wreg(gc(mmCP_GFX_MQD_BASE_ADDR_HI), mqd->cp_mqd_base_addr_hi);
    wreg(gc(mmCP_GFX_MQD_CONTROL), mqd->cp_gfx_mqd_control);
    wreg(gc(mmCP_GFX_HQD_VMID), 0);
    wreg(gc(mmCP_GFX_HQD_QUEUE_PRIORITY), mqd->cp_gfx_hqd_queue_priority);
    wreg(gc(mmCP_GFX_HQD_QUANTUM), mqd->cp_gfx_hqd_quantum);
    wreg(gc(mmCP_GFX_HQD_BASE), mqd->cp_gfx_hqd_base);
    wreg(gc(mmCP_GFX_HQD_BASE_HI), mqd->cp_gfx_hqd_base_hi);
    wreg(gc(mmCP_GFX_HQD_RPTR_ADDR), mqd->cp_gfx_hqd_rptr_addr);
    wreg(gc(mmCP_GFX_HQD_RPTR_ADDR_HI), mqd->cp_gfx_hqd_rptr_addr_hi);
    wreg(gc(mmCP_GFX_HQD_CNTL), mqd->cp_gfx_hqd_cntl);
    wreg(gc(mmCP_GFX_HQD_WPTR), 0);
    wreg(gc(mmCP_GFX_HQD_WPTR_HI), 0);
    wreg(gc(mmCP_GFX_HQD_ACTIVE), 1);
    IOFreeData(mqd, sizeof(v10_gfx_mqd));

    wreg(soc15(kHwGC, 0, mmCP_RB_WPTR_DELAY), 0);
    wreg(soc15(kHwGC, 0, mmCP_RB_VMID), 0);

    // buffer size and block size are log2 of the size in qwords
    uint32_t cntl = 9 | (7 << 8);
    wreg(soc15(kHwGC, 0, mmCP_RB0_CNTL), cntl);
    wreg(soc15(kHwGC, 0, mmCP_RB0_WPTR), 0);
    wreg(soc15(kHwGC, 0, mmCP_RB0_WPTR_HI), 0);
    wreg(soc15(kHwGC, 0, mmCP_RB0_RPTR_ADDR), (uint32_t)rptrMc);
    wreg(soc15(kHwGC, 0, mmCP_RB0_RPTR_ADDR_HI), (uint32_t)(rptrMc >> 32) & 0xffff);
    wreg(soc15(kHwGC, 0, mmCP_RB_WPTR_POLL_ADDR_LO), (uint32_t)wptrMc);
    wreg(soc15(kHwGC, 0, mmCP_RB_WPTR_POLL_ADDR_HI), (uint32_t)(wptrMc >> 32));
    IODelay(1000);
    wreg(soc15(kHwGC, 0, mmCP_RB0_CNTL), cntl);
    wreg(soc15(kHwGC, 0, mmCP_RB0_BASE), (uint32_t)(ringMc >> 8));
    wreg(soc15(kHwGC, 0, mmCP_RB0_BASE_HI), (uint32_t)(ringMc >> 40));
    wreg(soc15(kHwGC, 0, mmCP_RB_ACTIVE), 1);
    }

    // cp_gfx_start: eight hardware contexts, then unhalt ce, pfp and me
    wreg(soc15(kHwGC, 0, mmCP_MAX_CONTEXT), 7);
    wreg(soc15(kHwGC, 0, mmCP_DEVICE_ID), 1);
    wreg(soc15(kHwGC, 0, mmCP_ME_CNTL), rreg(soc15(kHwGC, 0, mmCP_ME_CNTL)) & ~kCpHaltMask);
    for (int i = 0; i < 1000 && rreg(soc15(kHwGC, 0, mmCP_STAT)); i++)
        IODelay(10);

    traceFault("cp unhalt");

    // cp_gfx_start's prologue, then the ring test's scratch write
    wreg(scratch, 0xcafedead);
    uint32_t *ring = (uint32_t *)IOMallocZeroData(kGfxRingSize);
    if (!ring) return;
    uint32_t n = emitClearState(ring);
    ring[n++] = PACKET3(PACKET3_SET_BASE, 2);
    ring[n++] = kCePartitionBase;
    ring[n++] = 0x8000;
    ring[n++] = 0x8000;
    ring[n++] = PACKET3(PACKET3_SET_UCONFIG_REG, 1);
    ring[n++] = scratch - PACKET3_SET_UCONFIG_START;
    ring[n++] = 0xdeadbeef;
    copyToVram(base, ring, n * 4);
    IOFreeData(ring, kGfxRingSize);
    uint32_t shadow[2] = { n, 0 };
    copyToVram(base + kGfxWptrOff, shadow, sizeof(shadow));
    flushHdp();
    // gfx_v10_0_ring_set_wptr_gfx with a doorbell: the shadow already holds wptr, then the 64-bit doorbell write
    if (ringDoorbell(kGfxDoorbell, n))
        LOG("gfx: doorbell 0x%x rung with wptr %u, CP_RB_DOORBELL_CONTROL 0x%08x\n", kGfxDoorbell, n,
            rreg(soc15(kHwGC, 0, mmCP_RB_DOORBELL_CONTROL)));

    uint32_t v = 0;
    for (int i = 0; i < 1000 && (v = rreg(scratch)) != 0xdeadbeef; i++)
        IODelay(100);
    if (v != 0xdeadbeef) dumpGfxState("gfx ring test");
    LOG("gfx: ring test %s, SCRATCH_REG0 0x%08x, rptr %u wptr %u, hqd rptr %u wptr %u base 0x%08x active %u, CP_STAT 0x%08x\n",
        v == 0xdeadbeef ? "passed" : "failed", v, rreg(soc15(kHwGC, 0, mmCP_RB0_RPTR)),
        rreg(soc15(kHwGC, 0, mmCP_RB0_WPTR)), rreg(gc(mmCP_GFX_HQD_RPTR)), rreg(gc(mmCP_GFX_HQD_WPTR)),
        rreg(gc(mmCP_GFX_HQD_BASE)), rreg(gc(mmCP_GFX_HQD_ACTIVE)), rreg(soc15(kHwGC, 0, mmCP_STAT)));
}

// bar2 is the doorbell aperture, 64-bit doorbells are one 8-byte store at dword index * 4
bool PDAmdGPU::ringDoorbell(uint32_t index, uint64_t value)
{
    if (!doorbellMap) {
        doorbellMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2, kIOMapInhibitCache);
        if (!doorbellMap) {
            LOG("gfx: cannot map bar2 doorbells\n");
            return false;
        }
        LOG("gfx: doorbells at phys 0x%llx len 0x%llx\n", (uint64_t)doorbellMap->getPhysicalAddress(),
            (uint64_t)doorbellMap->getLength());
    }
    if ((uint64_t)index * 4 + 8 > doorbellMap->getLength()) return false;
    volatile uint64_t *db = (volatile uint64_t *)(doorbellMap->getVirtualAddress() + index * 4);
    *db = value;
    return true;
}
