#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>
#include <string.h>

// mp 13.0.5 and gc 10.3 offsets from the amdgpu register headers (MIT)
#define regMP0_SMN_C2PMSG_64        0x0080
#define regMP0_SMN_C2PMSG_67        0x0083
#define regMP0_SMN_C2PMSG_69        0x0085
#define regMP0_SMN_C2PMSG_70        0x0086
#define regMP0_SMN_C2PMSG_71        0x0087
#define regMP0_SMN_C2PMSG_81        0x0091
#define mmGCMC_VM_FB_OFFSET         0x16e7

// nbio 7.2 moves the hdp flush controls into the last page of the register bar
#define regBIF_BX0_REMAP_HDP_MEM_FLUSH_CNTL  0x012d
#define regBIF_BX0_REMAP_HDP_REG_FLUSH_CNTL  0x012e
#define kMmioRemapOffset            (0x80000 - 0x1000)
#define mmGCMC_VM_FB_LOCATION_BASE  0x16fc

#define kPspRingTypeKM  2
#define kPspDestroyRings 0x00030000
#define kPspRingSize    0x1000

// carveout layout from pspBase, the psp wants 4 KiB aligned buffers
#define kPspFenceOff    0x1000
#define kPspCmdOff      0x2000
#define kPspFwPriOff    0x100000
#define kPspFwPriSize   0x100000
#define kPspTmrOff      0x1000000

// psp_gfx_if.h: command id at +8, command body at +28, response at +864
#define kGfxCmdLoadToc  0x20
#define kCmdIdWord      2
#define kCmdBodyWord    7
#define kCmdRespWord    216

#define kGfxCmdSetupTmr     0x05
#define kGfxCmdDestroyTmr   0x07
#define kGfxCmdLoadIpFw     0x06
#define kGfxCmdAutoloadRlc  0x21

// gc 10.3 registers for the autoload check
#define mmCP_STAT                   0x0f40
#define mmRLC_RLCS_BOOTLOAD_STATUS  0x4e8d

// firmware stays an unmodified binary, LICENSE.amdgpu is installed with the kext
#define FW_BLOB(sym, file) \
    ".p2align 4\n.private_extern _pd_amd_" #sym "_start\n_pd_amd_" #sym "_start:\n" \
    ".incbin \"" PDAMDGPU_FW_DIR "/" file "\"\n" \
    ".private_extern _pd_amd_" #sym "_end\n_pd_amd_" #sym "_end:\n"
__asm__(".section __TEXT,__const\n"
        FW_BLOB(toc, "psp_13_0_5_toc.bin")
        FW_BLOB(sdma, "sdma_5_2_6.bin")
        FW_BLOB(ce, "gc_10_3_6_ce.bin")
        FW_BLOB(pfp, "gc_10_3_6_pfp.bin")
        FW_BLOB(me, "gc_10_3_6_me.bin")
        FW_BLOB(mec, "gc_10_3_6_mec.bin")
        FW_BLOB(rlc, "gc_10_3_6_rlc.bin")
        ".text\n");
#define FW_DECL(sym) extern "C" const uint8_t pd_amd_##sym##_start[], pd_amd_##sym##_end[];
FW_DECL(toc) FW_DECL(sdma) FW_DECL(ce) FW_DECL(pfp) FW_DECL(me) FW_DECL(mec) FW_DECL(rlc)

// common_firmware_header from amdgpu_ucode.h
struct AmdFwHeader {
    uint32_t size_bytes, header_size_bytes;
    uint16_t header_major, header_minor, ip_major, ip_minor;
    uint32_t ucode_version, ucode_size_bytes, ucode_array_offset_bytes, crc32;
};

// gfx_firmware_header_v1_0, the jump table sits at the end of the mec image
struct AmdGfxHeader {
    AmdFwHeader common;
    uint32_t feature_version, jt_offset, jt_size;
};

// rlc_firmware_header_v2_2, as dwords after the common header
enum {
    kRlcCntlSize = 21, kRlcCntlOff, kRlcGpmSize = 25, kRlcGpmOff, kRlcSrmSize = 29, kRlcSrmOff,
    kRlcIramSize, kRlcIramOff, kRlcDramSize, kRlcDramOff,
};

// psp_gfx_fw_type values
enum {
    kFwCpMe = 1, kFwCpPfp = 2, kFwCpCe = 3, kFwCpMec = 4, kFwRlcG = 8, kFwSdma0 = 9,
    kFwRlcGpmMem = 20, kFwRlcSrmMem = 21, kFwRlcSrmCntl = 22, kFwRlcIram = 26, kFwRlcDramBoot = 48,
};

uint32_t PDAmdGPU::soc15(uint16_t hw, uint8_t seg, uint32_t off)
{
    const AmdIp *ip = ips.find(hw);

    if (!ip || seg >= ip->count) return 0;
    return ip->base[seg] + off;
}

bool PDAmdGPU::waitReg(uint32_t idx, uint32_t want, uint32_t mask, uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++) {
        if ((rreg(idx) & mask) == want) return true;
        IODelay(1000);
    }
    return false;
}

void PDAmdGPU::writeVram(uint64_t pos, uint32_t v, uint32_t len)
{
    for (uint32_t i = 0; i < len / 4; i++, pos += 4) {
        wreg(mmMM_INDEX, (uint32_t)pos | 0x80000000);
        wreg(mmMM_INDEX_HI, (uint32_t)(pos >> 31));
        wreg(mmMM_DATA, v);
    }
}

void PDAmdGPU::remapHdp(void)
{
    wreg(soc15(kHwNBIF, 2, regBIF_BX0_REMAP_HDP_MEM_FLUSH_CNTL), kMmioRemapOffset);
    wreg(soc15(kHwNBIF, 2, regBIF_BX0_REMAP_HDP_REG_FLUSH_CNTL), kMmioRemapOffset + 4);
}

// pushes cpu writes out of the hdp so the psp sees them, then posts with a plain read
void PDAmdGPU::flushHdp(void)
{
    wreg(kMmioRemapOffset / 4, 0);
    (void)rreg(mmRCC_CONFIG_MEMSIZE);
}

void PDAmdGPU::copyToVram(uint64_t pos, const void *buf, uint32_t len)
{
    const uint8_t *p = (const uint8_t *)buf;

    for (uint32_t i = 0; i < len; i += 4, pos += 4) {
        uint32_t v = 0;
        memcpy(&v, p + i, len - i < 4 ? len - i : 4);
        wreg(mmMM_INDEX, (uint32_t)pos | 0x80000000);
        wreg(mmMM_INDEX_HI, (uint32_t)(pos >> 31));
        wreg(mmMM_DATA, v);
    }
}

// one command through the km ring, as psp_cmd_submit_buf does, then waits on the fence
bool PDAmdGPU::pspSubmit(const uint32_t *cmd, uint32_t words, uint32_t *resp, uint32_t respWords)
{
    uint32_t wptrReg = soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_67);
    uint32_t frame[16] = {};
    uint64_t cmdMc = fbStart + pspBase + kPspCmdOff;
    uint64_t fenceMc = fbStart + pspBase + kPspFenceOff;

    writeVram(pspBase + kPspCmdOff, 0, 0x1000);
    copyToVram(pspBase + kPspCmdOff, cmd, words * 4);

    uint32_t wptr = rreg(wptrReg);
    uint32_t index = ++pspFence;
    frame[0] = (uint32_t)cmdMc;
    frame[1] = (uint32_t)(cmdMc >> 32);
    frame[3] = (uint32_t)fenceMc;
    frame[4] = (uint32_t)(fenceMc >> 32);
    frame[5] = index;
    copyToVram(pspBase + (wptr % (kPspRingSize / 4)) * 4, frame, sizeof(frame));
    flushHdp();
    wreg(wptrReg, (wptr + 16) % (kPspRingSize / 4));

    uint32_t fence = 0;
    for (int i = 0; i < 2000 && fence != index; i++) {
        IODelay(1000);
        readVram(pspBase + kPspFenceOff, &fence, 4);
    }
    readVram(pspBase + kPspCmdOff + kCmdRespWord * 4, resp, respWords * 4);
    if (fence != index) {
        LOG("psp: cmd 0x%x timed out, wptr %u fence %u\n", cmd[kCmdIdWord], wptr, fence);
        return false;
    }
    return true;
}

// hands the toc to the psp, which answers with the tmr size the firmware set needs
void PDAmdGPU::pspLoadToc(void)
{
    const AmdFwHeader *h = (const AmdFwHeader *)pd_amd_toc_start;
    uint32_t total = pd_amd_toc_end - pd_amd_toc_start;

    if (total < sizeof(*h) || h->ucode_array_offset_bytes + h->ucode_size_bytes > total) {
        LOG("psp: toc image is malformed\n");
        return;
    }
    copyToVram(pspBase + kPspFwPriOff, pd_amd_toc_start + h->ucode_array_offset_bytes, h->ucode_size_bytes);

    uint64_t priMc = fbStart + pspBase + kPspFwPriOff;
    uint32_t cmd[kCmdBodyWord + 3] = {};
    cmd[kCmdIdWord] = kGfxCmdLoadToc;
    cmd[kCmdBodyWord] = (uint32_t)priMc;
    cmd[kCmdBodyWord + 1] = (uint32_t)(priMc >> 32);
    cmd[kCmdBodyWord + 2] = h->ucode_size_bytes;

    uint32_t resp[5] = {};
    if (!pspSubmit(cmd, kCmdBodyWord + 3, resp, 5)) return;
    LOG("psp: load toc v0x%x (%u bytes) status 0x%x, tmr size 0x%x\n", h->ucode_version, h->ucode_size_bytes,
        resp[0], resp[4]);
    if (resp[0] == 0 && resp[4] && pspSetupTmr(resp[4])) pspLoadGfx();
}

// stages one image in the firmware buffer and has the psp load it into its ip
bool PDAmdGPU::pspLoadIpFw(const char *name, uint32_t type, const uint8_t *data, uint32_t size)
{
    if (size == 0 || size > kPspFwPriSize) {
        LOG("psp: %s has bad size %u\n", name, size);
        return false;
    }
    copyToVram(pspBase + kPspFwPriOff, data, size);

    uint64_t priMc = fbStart + pspBase + kPspFwPriOff;
    uint32_t cmd[kCmdBodyWord + 4] = {};
    cmd[kCmdIdWord] = kGfxCmdLoadIpFw;
    cmd[kCmdBodyWord] = (uint32_t)priMc;
    cmd[kCmdBodyWord + 1] = (uint32_t)(priMc >> 32);
    cmd[kCmdBodyWord + 2] = size;
    cmd[kCmdBodyWord + 3] = type;

    uint32_t resp[1] = {};
    bool ok = pspSubmit(cmd, kCmdBodyWord + 4, resp, 1) && resp[0] == 0;
    LOG("psp: load %s (type %u, %u bytes) status 0x%x\n", name, type, size, resp[0]);
    return ok;
}

// the plain images: payload at ucode_array_offset, minus the jump table for mec
static bool fwPayload(const uint8_t *start, const uint8_t *end, bool dropJt, const uint8_t **data, uint32_t *size)
{
    const AmdGfxHeader *h = (const AmdGfxHeader *)start;
    uint32_t total = end - start;

    if (total < sizeof(AmdFwHeader) || h->common.ucode_array_offset_bytes + h->common.ucode_size_bytes > total)
        return false;
    *data = start + h->common.ucode_array_offset_bytes;
    *size = h->common.ucode_size_bytes - (dropJt ? h->jt_size * 4 : 0);
    return true;
}

// psp 13.0.5 has no boot-time tmr, so the driver hands it one, by gpu and by system physical address
bool PDAmdGPU::pspSetupTmr(uint32_t size)
{
    uint64_t tmrMc = fbStart + pspBase + kPspTmrOff;
    uint64_t tmrPa = tmrMc - fbStart + fbOffset;
    uint32_t cmd[kCmdBodyWord + 6] = {};

    cmd[kCmdIdWord] = kGfxCmdSetupTmr;
    cmd[kCmdBodyWord] = (uint32_t)tmrMc;
    cmd[kCmdBodyWord + 1] = (uint32_t)(tmrMc >> 32);
    cmd[kCmdBodyWord + 2] = size;
    cmd[kCmdBodyWord + 3] = 1 << 1;     // virt_phy_addr
    cmd[kCmdBodyWord + 4] = (uint32_t)tmrPa;
    cmd[kCmdBodyWord + 5] = (uint32_t)(tmrPa >> 32);

    uint32_t resp[1] = {};
    bool ok = pspSubmit(cmd, kCmdBodyWord + 6, resp, 1) && resp[0] == 0;
    LOG("psp: setup tmr mc 0x%llx pa 0x%llx size 0x%x status 0x%x\n", tmrMc, tmrPa, size, resp[0]);
    return ok;
}

// the gfx firmware in amdgpu's ucode id order, then the rlc autoload starts the lot
void PDAmdGPU::pspLoadGfx(void)
{
    struct { const char *name; uint32_t type; const uint8_t *start, *end; bool jt; } plain[] = {
        { "sdma0", kFwSdma0, pd_amd_sdma_start, pd_amd_sdma_end, false },
        { "cp ce", kFwCpCe, pd_amd_ce_start, pd_amd_ce_end, false },
        { "cp pfp", kFwCpPfp, pd_amd_pfp_start, pd_amd_pfp_end, false },
        { "cp me", kFwCpMe, pd_amd_me_start, pd_amd_me_end, false },
        { "cp mec1", kFwCpMec, pd_amd_mec_start, pd_amd_mec_end, true },
        { "cp mec2", kFwCpMec, pd_amd_mec_start, pd_amd_mec_end, true },
    };

    for (auto &f : plain) {
        const uint8_t *data;
        uint32_t size;
        if (!fwPayload(f.start, f.end, f.jt, &data, &size)) {
            LOG("psp: %s image is malformed\n", f.name);
            return;
        }
        if (!pspLoadIpFw(f.name, f.type, data, size)) return;
    }

    const uint32_t *rlc = (const uint32_t *)pd_amd_rlc_start;
    const uint32_t *w = rlc + sizeof(AmdFwHeader) / 4;
    struct { const char *name; uint32_t type, size, off; } lists[] = {
        { "rlc restore cntl", kFwRlcSrmCntl, w[kRlcCntlSize], w[kRlcCntlOff] },
        { "rlc restore gpm", kFwRlcGpmMem, w[kRlcGpmSize], w[kRlcGpmOff] },
        { "rlc restore srm", kFwRlcSrmMem, w[kRlcSrmSize], w[kRlcSrmOff] },
        { "rlc iram", kFwRlcIram, w[kRlcIramSize], w[kRlcIramOff] },
        { "rlc dram", kFwRlcDramBoot, w[kRlcDramSize], w[kRlcDramOff] },
    };
    uint32_t rlcTotal = pd_amd_rlc_end - pd_amd_rlc_start;

    for (auto &l : lists) {
        if (l.size == 0) continue;
        if (l.off + l.size > rlcTotal) {
            LOG("psp: %s runs past the rlc image\n", l.name);
            return;
        }
        if (!pspLoadIpFw(l.name, l.type, pd_amd_rlc_start + l.off, l.size)) return;
    }

    const uint8_t *data;
    uint32_t size;
    if (!fwPayload(pd_amd_rlc_start, pd_amd_rlc_end, false, &data, &size) || !pspLoadIpFw("rlc g", kFwRlcG, data, size))
        return;

    uint32_t cmd[kCmdBodyWord] = {};
    uint32_t resp[1] = {};
    cmd[kCmdIdWord] = kGfxCmdAutoloadRlc;
    bool ok = pspSubmit(cmd, kCmdBodyWord, resp, 1);
    LOG("psp: autoload rlc %s, status 0x%x\n", ok ? "sent" : "failed", resp[0]);
    if (!ok) return;

    uint32_t cpStat = 0, boot = 0;
    for (int i = 0; i < 1000; i++) {
        cpStat = rreg(soc15(kHwGC, 0, mmCP_STAT));
        boot = rreg(soc15(kHwGC, 1, mmRLC_RLCS_BOOTLOAD_STATUS));
        if (cpStat == 0 && (boot & 0x80000000)) break;
        IODelay(1000);
    }
    if (dmubLoad() && dmubInit()) {
        dmubQueryCaps();
        vbiosDisplayPaths();
        if (clkInit()) hdmiLinkUp();
    }
    LOG("gfx: autoload %s, CP_STAT 0x%08x RLC_RLCS_BOOTLOAD_STATUS 0x%08x\n",
        cpStat == 0 && (boot & 0x80000000) ? "complete" : "timed out", cpStat, boot);
    if (cpStat == 0 && (boot & 0x80000000)) {
        traceFault("autoload");
        smuDisallowGfxOff("after autoload");
        goldenInit();
        gfxhubInit();
        traceFault("gfxhub");
        rlcInit();
        traceFault("rlc");
        irqInit();
        ihInit();
        sdmaTest();
        traceFault("sdma");
        gfxStart();
    }
}

// creates the kernel-mode command ring the driver uses to talk to the psp
void PDAmdGPU::probePsp(void)
{
    uint32_t msg64 = soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_64);
    uint32_t msg81 = soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_81);

    if (!msg64 || msg64 >= regCount || regCount < kMmioRemapOffset / 4 + 2) {
        LOG("psp: no mp0 base or short register bar\n");
        return;
    }
    remapHdp();

    fbStart = (uint64_t)(rreg(soc15(kHwGC, 0, mmGCMC_VM_FB_LOCATION_BASE)) & 0xffffff) << 24;
    fbOffset = (uint64_t)rreg(soc15(kHwGC, 0, mmGCMC_VM_FB_OFFSET)) << 24;
    LOG("psp: fb mc start 0x%llx, system offset 0x%llx\n", fbStart, fbOffset);

    uint32_t sol = rreg(msg81);
    LOG("psp: sos %s (C2PMSG_81 0x%08x), C2PMSG_64 0x%08x\n", sol ? "alive" : "not running", sol, rreg(msg64));
    if (!sol) return;

    if (!waitReg(msg64, 0x80000000, 0x80000000, 2000)) {
        LOG("psp: sos not ready for ring creation\n");
        return;
    }

    // the psp keeps its rings across guest reboots, so drop whatever an earlier driver left behind
    wreg(msg64, kPspDestroyRings);
    IOSleep(20);
    bool gone = waitReg(msg64, 0x80000000, 0x8000ffff, 2000);
    LOG("psp: destroy rings %s, C2PMSG_64 0x%08x\n", gone ? "ok" : "failed", rreg(msg64));

    // halfway into the carveout, clear of the boot framebuffer and the tables at the top
    pspBase = vramSize / 2;
    uint64_t ringMc = fbStart + pspBase;
    writeVram(pspBase, 0, kPspRingSize);

    wreg(soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_69), (uint32_t)ringMc);
    wreg(soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_70), (uint32_t)(ringMc >> 32));
    wreg(soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_71), kPspRingSize);
    wreg(msg64, kPspRingTypeKM << 16);
    IOSleep(20);

    bool ok = waitReg(msg64, 0x80000000, 0x8000ffff, 2000);
    LOG("psp: km ring at mc 0x%llx %s, C2PMSG_64 0x%08x\n", ringMc, ok ? "created" : "failed", rreg(msg64));
    if (!ok) return;
    pspLoadToc();
    teardown();
}

// amdgpu's hw_fini order: halt the cp and sdma engines, unload the tmr, destroy the rings,
// so the next driver to run (ours or amdgpu) finds a psp that accepts SETUP_TMR and AUTOLOAD_RLC again
void PDAmdGPU::teardown(void)
{
    dmubStop();
    ihStop();
    irqStop();
    wreg(soc15(kHwGC, 0, 0x0f56), rreg(soc15(kHwGC, 0, 0x0f56)) | 0x15000000);     // CP_ME_CNTL ce, pfp, me halt
    wreg(soc15(kHwGC, 0, 0x0f55), rreg(soc15(kHwGC, 0, 0x0f55)) | 0x50000000);     // CP_MEC_CNTL me1, me2 halt
    wreg(soc15(kHwGC, 0, 0x002a), rreg(soc15(kHwGC, 0, 0x002a)) | 0x1);            // SDMA0_F32_CNTL halt

    uint32_t cmd[kCmdBodyWord] = {};
    uint32_t resp[1] = {};
    cmd[kCmdIdWord] = kGfxCmdDestroyTmr;
    bool tmr = pspSubmit(cmd, kCmdBodyWord, resp, 1);
    LOG("teardown: destroy tmr %s, status 0x%x\n", tmr ? "sent" : "failed", resp[0]);

    uint32_t msg64 = soc15(kHwMP0, 0, regMP0_SMN_C2PMSG_64);
    wreg(msg64, kPspDestroyRings);
    IOSleep(20);
    bool gone = waitReg(msg64, 0x80000000, 0x8000ffff, 2000);
    LOG("teardown: destroy rings %s, C2PMSG_64 0x%08x\n", gone ? "ok" : "failed", rreg(msg64));

    // amdgpu_device_ip_fini_early: on apus below gc 11 the unload ends in an asic reset, since the psp never stops
    smuMode2Reset("teardown");
}
