#include "PDAmdGPU.h"

#include <IOKit/IOLib.h>
#include <string.h>
#include <IOKit/IODeviceMemory.h>

// dcn 3.1.5 dmcub registers, absolute dword indices from dcn_3_1_5_offset.h (MIT) over the dcn segment bases
#define regDMCUB_REGION5_OFFSET         0x3658
#define regDMCUB_REGION5_OFFSET_HIGH    0x3659
#define regDMCUB_REGION5_TOP_ADDRESS    0x3662
#define regDMCUB_REGION3_CW3_BASE       0x3668
#define regDMCUB_REGION3_CW4_BASE       0x3669
#define regDMCUB_REGION3_CW5_BASE       0x366a
#define regDMCUB_REGION3_CW6_BASE       0x366b
#define regDMCUB_REGION3_CW3_TOP        0x3670
#define regDMCUB_REGION3_CW4_TOP        0x3671
#define regDMCUB_REGION3_CW5_TOP        0x3672
#define regDMCUB_REGION3_CW6_TOP        0x3673
#define regDMCUB_REGION3_CW3_OFFSET     0x367b
#define regDMCUB_REGION3_CW3_OFFSET_HI  0x367c
#define regDMCUB_REGION3_CW4_OFFSET     0x367d
#define regDMCUB_REGION3_CW4_OFFSET_HI  0x367e
#define regDMCUB_REGION3_CW5_OFFSET     0x367f
#define regDMCUB_REGION3_CW5_OFFSET_HI  0x3680
#define regDMCUB_REGION3_CW6_OFFSET     0x3681
#define regDMCUB_REGION3_CW6_OFFSET_HI  0x3682
#define regDMCUB_INBOX1_BASE            0x3694
#define regDMCUB_INBOX1_SIZE            0x3695
#define regDMCUB_INBOX1_WPTR            0x3696
#define regDMCUB_INBOX1_RPTR            0x3697
#define regDMCUB_OUTBOX0_BASE           0x3698
#define regDMCUB_OUTBOX0_SIZE           0x3699
#define regDMCUB_OUTBOX0_WPTR           0x369a
#define regDMCUB_OUTBOX0_RPTR           0x369b
#define regDMCUB_OUTBOX1_BASE           0x369c
#define regDMCUB_OUTBOX1_SIZE           0x369d
#define regDMCUB_OUTBOX1_WPTR           0x369e
#define regDMCUB_OUTBOX1_RPTR           0x369f
#define regDMCUB_SCRATCH0               0x36a3
#define regDMCUB_SCRATCH7               0x36aa
#define regDMCUB_SCRATCH14              0x36b1
#define regDMCUB_SCRATCH15              0x36b2
#define regDMCUB_CNTL                   0x36b6
#define regDMCUB_GPINT_DATAIN1          0x36b8
#define regDMCUB_CNTL2                  0x36c0
#define regMMHUBBUB_SOFT_RESET          0x3802

#define kDmcubEnable        0x00010000
#define kDmcubTraceportEn   0x00080000
#define kDmcubPwaitStatus   0x00100000
#define kDmcubSoftReset     0x00000001
#define kDmuifSoftReset     0x00000100
#define kWindowEnable       0x80000000
#define kWindowTopMask      0x1fffffff

// dmub_srv.c and dmub_cmd.h
#define DMUB_CW3_BASE       0x63000000u
#define DMUB_CW4_BASE       0x64000000u
#define DMUB_CW5_BASE       0x65000000u
#define DMUB_CW6_BASE       0x66000000u
#define DMUB_REGION5_BASE   0xa0000000u
#define DMUB_RB_SIZE        (64 * 128)
#define TRACE_ENTRY_OFFSET  16
#define DMUB_FW_META_MAGIC  0x444d5542
#define DMUB_FW_META_OFFSET 0x24
#define kGpintStopFw        2
#define kGpintStreamMask    8           // DMUB_GPINT__IDLE_OPT_NOTIFY_STREAM_MASK
#define kGpintStopResponse  0xdeaddead
#define kGfxFwTypeDmub      51
#define kPspHeaderBytes     256


// carveout layout from dmubBase, each window on its own 4 KiB boundary
#define kDmubMailboxOff     0x00000
#define kDmubTraceOff       0x10000
#define kDmubStateOff       0x30000
#define kDmubVbiosOff       0x40000

// the psp loads the instruction image into the tmr, the driver only owns the data windows
bool PDAmdGPU::dmubLoad(void)
{
    const uint32_t *h = (const uint32_t *)chip->fw->dmcub.start;
    uint32_t total = chip->fw->dmcub.end - chip->fw->dmcub.start;

    // dmcub_firmware_header_v1_0: the common header, then inst_const_bytes and bss_data_bytes
    uint32_t arrayOff = h[6], instBytes = h[8], bssBytes = h[9];
    if (arrayOff + instBytes > total) {
        LOG("dmub: firmware image is malformed\n");
        return false;
    }
    dmubInst = chip->fw->dmcub.start + arrayOff;
    dmubInstSize = instBytes;
    LOG("dmub: firmware v0x%08x, inst %u bytes, bss %u bytes\n", h[4], instBytes, bssBytes);

    // the meta block sits just before the last 0x24 bytes of the image between the psp header and footer
    const uint8_t *blob = dmubInst + kPspHeaderBytes;
    uint32_t blobSize = instBytes - 2 * kPspHeaderBytes;
    dmubStateSize = 64 * 1024;
    dmubTraceSize = 64 * 1024;
    for (uint32_t off = blobSize - DMUB_FW_META_OFFSET - 4; off > blobSize - 256 && off < blobSize; off -= 4) {
        uint32_t magic;
        memcpy(&magic, blob + off, 4);
        if (magic != DMUB_FW_META_MAGIC) continue;
        memcpy(&dmubStateSize, blob + off + 4, 4);
        memcpy(&dmubTraceSize, blob + off + 8, 4);
        break;
    }
    LOG("dmub: fw state 0x%x bytes, trace 0x%x bytes\n", dmubStateSize, dmubTraceSize);
    bool ok = pspLoadIpFw("dmcub", kGfxFwTypeDmub, dmubInst, dmubInstSize);
    dmubDump("after psp load");
    return ok;
}

// the video bios from the pci expansion rom, the window dmub reads its command tables from
bool PDAmdGPU::readVbios(void)
{
    uint32_t save = pci->configRead32(kIOPCIConfigExpansionROMBase);
    IODeviceMemory *bar = pci->getDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);
    uint32_t len = bar ? (uint32_t)bar->getLength() : 0x20000;
    uint32_t base = bar ? (uint32_t)bar->getPhysicalAddress() : (save & ~0x7ffu);

    // an unassigned rom bar reads back its size mask, so place it in a free hole of the 32-bit window
    if (!base || base == (0xffffffffu & ~(len - 1) & ~0x7ffu) || (save & ~0x7ffu) == ((~(len - 1)) & ~0x7ffu)) {
        if (!findRomHole(len, &base)) {
            LOG("dmub: no free window for the expansion rom\n");
            return false;
        }
    }
    pci->configWrite32(kIOPCIConfigExpansionROMBase, base | 1);
    IODeviceMemory *mem = IODeviceMemory::withRange(base, len);
    IOMemoryMap *map = mem ? mem->map(kIOMapInhibitCache) : NULL;
    OSSafeReleaseNULL(mem);

    bool ok = false;
    if (map) {
        volatile uint8_t *r = (volatile uint8_t *)map->getVirtualAddress();
        uint32_t size = r[2] * 512;
        if (r[0] == 0x55 && r[1] == 0xaa && size && size <= len) {
            vbios = (uint8_t *)IOMallocZeroData(size);
            if (vbios) {
                for (uint32_t i = 0; i < size; i += 4)
                    *(uint32_t *)(vbios + i) = *(volatile uint32_t *)(r + i);
                vbiosSize = size;
                ok = true;
            }
        }
        map->release();
    }
    pci->configWrite32(kIOPCIConfigExpansionROMBase, save);
    LOG("dmub: video bios %s, %u bytes from rom bar 0x%x\n", ok ? "read" : "not found", vbiosSize, base);
    return ok;
}

// a free, rom-sized and naturally aligned slot below the 32-bit mmio top that no pci range overlaps
bool PDAmdGPU::findRomHole(uint32_t size, uint32_t *outAddr)
{
    struct range { uint64_t base, len; } used[96];
    uint32_t n = 0;
    OSIterator *it = getMatchingServices(serviceMatching("IOPCIDevice"));

    if (!it) return false;
    OSObject *o;
    while ((o = it->getNextObject())) {
        IOPCIDevice *d = OSDynamicCast(IOPCIDevice, o);
        if (!d) continue;
        for (unsigned i = 0; i < d->getDeviceMemoryCount() && n < 96; i++) {
            IODeviceMemory *m = d->getDeviceMemoryWithIndex(i);
            if (!m) continue;
            used[n].base = m->getPhysicalAddress();
            used[n].len = m->getLength();
            n++;
        }
    }
    it->release();

    for (uint64_t a = 0xfe000000ULL - size; a >= 0xc0000000ULL; a -= size) {
        bool ok = true;
        for (uint32_t i = 0; i < n; i++)
            if (a < used[i].base + used[i].len && used[i].base < a + size)
                ok = false;
        if (ok) {
            *outAddr = (uint32_t)a;
            return true;
        }
    }
    return false;
}

static void writeWindow(PDAmdGPU *g, uint32_t offLo, uint32_t offHi, uint32_t baseReg, uint32_t topReg,
                        uint64_t mc, uint32_t base, uint32_t size)
{
    g->wreg(offLo, (uint32_t)mc);
    g->wreg(offHi, (uint32_t)(mc >> 32));
    g->wreg(baseReg, base);
    g->wreg(topReg, ((base + size) & kWindowTopMask) | kWindowEnable);
}

// dmub_dcn31_reset: ask a running dmcub to stop, then hold it in reset with its pointers cleared
void PDAmdGPU::dmubReset(void)
{
    if (!(rreg(regDMCUB_CNTL2) & kDmcubSoftReset)) {
        wreg(regDMCUB_GPINT_DATAIN1, (1u << 28) | (kGpintStopFw << 16));
        for (int i = 0; i < 100000 && rreg(regDMCUB_SCRATCH7) != kGpintStopResponse; i++)
            IODelay(1);
        for (int i = 0; i < 100000 && !(rreg(regDMCUB_CNTL) & kDmcubPwaitStatus); i++)
            IODelay(1);
    }
    if (rreg(regDMCUB_CNTL) & kDmcubEnable) {
        wreg(regDMCUB_CNTL2, rreg(regDMCUB_CNTL2) | kDmcubSoftReset);
        wreg(regMMHUBBUB_SOFT_RESET, rreg(regMMHUBBUB_SOFT_RESET) | kDmuifSoftReset);
        wreg(regDMCUB_CNTL, rreg(regDMCUB_CNTL) & ~kDmcubEnable);
    }
    wreg(regDMCUB_INBOX1_RPTR, 0);
    wreg(regDMCUB_INBOX1_WPTR, 0);
    wreg(regDMCUB_OUTBOX1_RPTR, 0);
    wreg(regDMCUB_OUTBOX1_WPTR, 0);
    wreg(regDMCUB_OUTBOX0_RPTR, 0);
    wreg(regDMCUB_OUTBOX0_WPTR, 0);
    wreg(regDMCUB_SCRATCH0, 0);
    wreg(regDMCUB_GPINT_DATAIN1, 0);
}

// dm_dmub_hw_init and dmub_srv_hw_init for dcn 3.1.5
bool PDAmdGPU::dmubInit(void)
{
    dmubBase = pspBase + 0x6000000;
    uint64_t mc = fbStart + dmubBase;

    // the register indices above assume dcn segment 2 at 0x34c0, as dmub_dcn315.c does
    const AmdIp *dmu = ips.find(271);
    LOG("dmub: discovery dmu segment 2 base 0x%x (expected 0x34c0)\n", dmu && dmu->count > 2 ? dmu->base[2] : 0);
    LOG("dmub: before reset SCRATCH0 0x%08x CNTL 0x%08x CNTL2 0x%08x\n", rreg(regDMCUB_SCRATCH0), rreg(regDMCUB_CNTL),
        rreg(regDMCUB_CNTL2));
    dmubReset();

    writeVram(dmubBase, 0, kDmubVbiosOff);
    if (vbios || readVbios())
        copyToVram(dmubBase + kDmubVbiosOff, vbios, vbiosSize);
    flushHdp();

    // cw3 the vbios (none yet), cw4 the inbox and outbox rings, cw5 the trace buffer, cw6 the firmware state
    writeWindow(this, regDMCUB_REGION3_CW3_OFFSET, regDMCUB_REGION3_CW3_OFFSET_HI, regDMCUB_REGION3_CW3_BASE,
                regDMCUB_REGION3_CW3_TOP, mc + kDmubVbiosOff, DMUB_CW3_BASE, vbiosSize);
    writeWindow(this, regDMCUB_REGION3_CW4_OFFSET, regDMCUB_REGION3_CW4_OFFSET_HI, regDMCUB_REGION3_CW4_BASE,
                regDMCUB_REGION3_CW4_TOP, mc + kDmubMailboxOff, DMUB_CW4_BASE, 2 * DMUB_RB_SIZE);
    writeWindow(this, regDMCUB_REGION3_CW5_OFFSET, regDMCUB_REGION3_CW5_OFFSET_HI, regDMCUB_REGION3_CW5_BASE,
                regDMCUB_REGION3_CW5_TOP, mc + kDmubTraceOff, DMUB_CW5_BASE, dmubTraceSize);
    wreg(regDMCUB_REGION5_OFFSET, (uint32_t)(mc + kDmubTraceOff));
    wreg(regDMCUB_REGION5_OFFSET_HIGH, (uint32_t)((mc + kDmubTraceOff) >> 32));
    wreg(regDMCUB_REGION5_TOP_ADDRESS, ((dmubTraceSize - 1) & kWindowTopMask) | kWindowEnable);
    writeWindow(this, regDMCUB_REGION3_CW6_OFFSET, regDMCUB_REGION3_CW6_OFFSET_HI, regDMCUB_REGION3_CW6_BASE,
                regDMCUB_REGION3_CW6_TOP, mc + kDmubStateOff, DMUB_CW6_BASE, dmubStateSize);

    // outbox0 in the trace buffer past its header, inbox1 and outbox1 splitting the mailbox
    wreg(regDMCUB_OUTBOX0_BASE, DMUB_REGION5_BASE + TRACE_ENTRY_OFFSET);
    wreg(regDMCUB_OUTBOX0_SIZE, dmubTraceSize - TRACE_ENTRY_OFFSET);
    wreg(regDMCUB_INBOX1_BASE, DMUB_CW4_BASE);
    wreg(regDMCUB_INBOX1_SIZE, DMUB_RB_SIZE);
    wreg(regDMCUB_OUTBOX1_BASE, DMUB_CW4_BASE + DMUB_RB_SIZE);
    wreg(regDMCUB_OUTBOX1_SIZE, DMUB_RB_SIZE);

    // amdgpu's boot options: enable_dpia only, then dmub_dcn31_reset_release
    wreg(regDMCUB_SCRATCH14, 0x80);
    wreg(regMMHUBBUB_SOFT_RESET, rreg(regMMHUBBUB_SOFT_RESET) & ~kDmuifSoftReset);
    wreg(regDMCUB_SCRATCH15, 0);
    wreg(regDMCUB_CNTL, rreg(regDMCUB_CNTL) | kDmcubEnable | kDmcubTraceportEn);
    wreg(regDMCUB_CNTL2, rreg(regDMCUB_CNTL2) & ~kDmcubSoftReset);

    // dmub_srv_wait_for_auto_load: dal firmware and mailbox ready in scratch 0
    uint32_t status = 0;
    for (int i = 0; i < 1000; i++) {
        status = rreg(regDMCUB_SCRATCH0);
        if ((status & 0x3) == 0x3) break;
        IODelay(100);
    }
    LOG("dmub: %s, SCRATCH0 0x%08x CNTL 0x%08x CNTL2 0x%08x\n", (status & 0x3) == 0x3 ? "firmware up" : "auto load timed out",
        status, rreg(regDMCUB_CNTL), rreg(regDMCUB_CNTL2));
    dmubWptr = 0;
    if ((status & 0x3) != 0x3) {
        IOSleep(50);
        dmubDump("after release");
    }
    return (status & 0x3) == 0x3;
}

void PDAmdGPU::dmubStop(void)
{
    if (dmubBase) dmubReset();
}

// the code windows the psp should have set, the fault registers and the first trace entries, named registers only
void PDAmdGPU::dmubDump(const char *why)
{
    static const struct { const char *name; uint32_t idx; } r[] = {
        { "CW0_OFFSET", 0x3675 }, { "CW0_OFFSET_HI", 0x3676 }, { "CW0_BASE", 0x3665 }, { "CW0_TOP", 0x366d },
        { "CW1_OFFSET", 0x3677 }, { "CW1_OFFSET_HI", 0x3678 }, { "CW1_BASE", 0x3666 }, { "CW1_TOP", 0x366e },
        { "CW2_OFFSET", 0x3679 }, { "CW2_BASE", 0x3667 }, { "CW2_TOP", 0x366f },
        { "SEC_CNTL", 0x368e }, { "MEM_CNTL", 0x368f }, { "TIMER_CURRENT", 0x36bd },
        { "INST_FETCH_FAULT", 0x368c }, { "DATA_WRITE_FAULT", 0x368d }, { "UNDEF_ADDR_FAULT", 0x36ba },
        { "SCRATCH0", regDMCUB_SCRATCH0 }, { "SCRATCH7", regDMCUB_SCRATCH7 }, { "CNTL", regDMCUB_CNTL }, { "CNTL2", regDMCUB_CNTL2 },
    };

    LOG("dmub: dump %s\n", why);
    for (auto &e : r)
        LOG("dmub:   %-18s 0x%08x\n", e.name, rreg(e.idx));
    if (dmubBase) {
        uint32_t t[8];
        readVram(dmubBase + kDmubTraceOff, t, sizeof(t));
        LOG("dmub:   trace %08x %08x %08x %08x %08x %08x %08x %08x\n", t[0], t[1], t[2], t[3], t[4], t[5], t[6], t[7]);
    }
}

// dmub_srv_cmd_queue and execute: one 64-byte command in the inbox ring, then wait for the firmware to consume it,
// with ret_status set the firmware writes its answer back into the same slot
bool PDAmdGPU::dmubCmd(uint32_t *cmd, bool wantReply)
{
    uint32_t slot = dmubWptr;

    copyToVram(dmubBase + kDmubMailboxOff + slot, cmd, 64);
    flushHdp();
    dmubWptr = (dmubWptr + 64) % DMUB_RB_SIZE;
    wreg(regDMCUB_INBOX1_WPTR, dmubWptr);

    uint32_t rptr = 0;
    for (int i = 0; i < 100000 && (rptr = rreg(regDMCUB_INBOX1_RPTR)) != dmubWptr; i++)
        IODelay(1);
    if (rptr != dmubWptr) {
        LOG("dmub: command type %u sub %u not consumed, rptr 0x%x wptr 0x%x SCRATCH0 0x%08x\n", cmd[0] & 0xff,
            (cmd[0] >> 8) & 0xff, rptr, dmubWptr, rreg(regDMCUB_SCRATCH0));
        return false;
    }
    if (wantReply)
        readVram(dmubBase + kDmubMailboxOff + slot, cmd, 64);
    return true;
}

// dmub_cmd_header: type, sub type, ret_status at bit 16, payload bytes at bit 24
static inline uint32_t dmubHeader(uint32_t type, uint32_t sub, bool ret, uint32_t payload)
{
    return type | (sub << 8) | (ret ? 1u << 16 : 0) | ((payload & 0x3f) << 24);
}

bool PDAmdGPU::dmubQueryCaps(void)
{
    uint32_t cmd[16] = {};

    cmd[0] = dmubHeader(6, 0, true, 60);
    if (!dmubCmd(cmd, true)) return false;
    LOG("dmub: feature caps %08x %08x %08x %08x\n", cmd[1], cmd[2], cmd[3], cmd[4]);
    return true;
}

static inline uint16_t rd16(const uint8_t *b, uint32_t o) { return b[o] | (b[o + 1] << 8); }

// atomfirmware.h display_object_info_table_v1_4: each path's connector, encoder and hpd pin, as bios_parser2 reads them
void PDAmdGPU::vbiosDisplayPaths(void)
{
    if (!vbios || vbiosSize < 0x50) return;

    uint16_t romHeader = rd16(vbios, 0x48);
    if (romHeader + 0x24 > vbiosSize) return;
    uint16_t master = rd16(vbios, romHeader + 32);                  // atom_rom_header_v2_2 masterdatatable_offset
    uint16_t objTable = rd16(vbios, master + 4 + 22 * 2);           // displayobjectinfo
    if (!objTable || objTable + 8 > vbiosSize) {
        LOG("vbios: no display object table\n");
        return;
    }

    uint8_t paths = vbios[objTable + 6];
    if (paths > 16) {
        LOG("vbios: display object table at 0x%x claims %u paths, not trusting it\n", objTable, paths);
        return;
    }
    LOG("vbios: display object table v%u.%u at 0x%x, %u paths\n", vbios[objTable + 2], vbios[objTable + 3], objTable, paths);
    for (uint8_t i = 0; i < paths && objTable + 8 + (i + 1) * 16 <= vbiosSize; i++) {
        uint32_t p = objTable + 8 + i * 16;
        uint16_t conn = rd16(vbios, p), recs = rd16(vbios, p + 2), enc = rd16(vbios, p + 4), tag = rd16(vbios, p + 12);
        int hpdPin = -1;

        // the connector's records, terminated by type 0xff
        for (uint32_t r = objTable + recs; recs && r + 2 <= vbiosSize && vbios[r] != 0xff && vbios[r + 1]; r += vbios[r + 1]) {
            if (vbios[r] == 2) hpdPin = vbios[r + 2];
        }
        LOG("vbios: path %u connector 0x%04x encoder 0x%04x device tag 0x%04x hpd pin %d\n", i, conn, enc, tag, hpdPin);
    }
}

// atomfirmware.h set_pixel_clock_parameter_v1_7 and dig_transmitter_control_parameters_v1_6, inside a vbios command
#define kDmubCmdVbios           128
#define kVbiosDig1Transmitter   1
#define kVbiosSetPixelClock     2

bool PDAmdGPU::dmubSetPixelClock(uint32_t pix100hz, uint8_t pll, uint8_t encoderId, uint8_t mode, uint8_t crtc)
{
    uint32_t cmd[16] = {};
    uint8_t *p = (uint8_t *)&cmd[1];

    memcpy(p, &pix100hz, 4);
    p[4] = pll;
    p[5] = encoderId;
    p[6] = mode;
    p[7] = 0;               // miscinfo
    p[8] = crtc;
    p[9] = 0;               // deep colour ratio off for 8 bpc
    cmd[0] = (kDmubCmdVbios | (kVbiosSetPixelClock << 8)) | (16u << 24);
    bool ok = dmubCmd(cmd, false);
    LOG("dmub: set pixel clock %u00 Hz pll %u encoder 0x%x mode %u crtc %u %s\n", pix100hz, pll, encoderId, mode, crtc,
        ok ? "done" : "failed");
    return ok;
}

bool PDAmdGPU::dmubTransmitter(uint8_t phy, uint8_t action, uint8_t mode, uint8_t lanes, uint32_t symclk10khz,
                               uint8_t hpd, uint8_t digfe, uint8_t connector)
{
    uint32_t cmd[16] = {};
    uint8_t *p = (uint8_t *)&cmd[1];

    p[0] = phy;
    p[1] = action;
    p[2] = mode;
    p[3] = lanes;
    memcpy(p + 4, &symclk10khz, 4);
    p[8] = hpd;
    p[9] = digfe;
    p[10] = connector;
    // the payload is the v1.6/v1.7 union, so always the 60 bytes of the larger v1.7 struct
    cmd[0] = (kDmubCmdVbios | (kVbiosDig1Transmitter << 8)) | (60u << 24);
    bool ok = dmubCmd(cmd, false);
    LOG("dmub: transmitter phy %u action %u mode %u lanes %u symclk %u0 kHz hpd %u %s\n", phy, action, mode, lanes, symclk10khz,
        hpd, ok ? "done" : "failed");
    return ok;
}

// the panel power sequencer and backlight belong to the firmware on dcn31, so they go through the vbios table
#define kVbiosLvtmaControl          15

bool PDAmdGPU::dmubLvtma(uint8_t action)
{
    uint32_t cmd[16] = {};
    uint8_t *p = (uint8_t *)&cmd[1];

    p[0] = action;          // 12 power on, 13 power off, 3 backlight on, 2 backlight off
    p[1] = 0;               // the firmware waits the panel's delays
    p[4] = 0;               // power sequencer 0
    cmd[0] = (kDmubCmdVbios | (kVbiosLvtmaControl << 8)) | (8u << 24);
    bool ok = dmubCmd(cmd, false);
    LOG("dmub: lvtma control action %u %s\n", action, ok ? "done" : "failed");
    return ok;
}

#define kDmubCmdIdleOpt             72
#define kIdleOptDcnRestore          0
#define kIdleOptSetDcPowerState     3

// dc_set_power_state's notice to the firmware, d0 before any display work and d3 before it is stopped
bool PDAmdGPU::dmubDcPowerState(bool on)
{
    uint32_t cmd[16] = {};

    // dcn31_z10_restore, only when the firmware's boot status asks for it
    if (on && (rreg(regDMCUB_SCRATCH0) & 0x8)) {
        cmd[0] = dmubHeader(kDmubCmdIdleOpt, kIdleOptDcnRestore, false, 0);
        LOG("dmub: dcn restore %s\n", dmubCmd(cmd, false) ? "done" : "failed");
        memset(cmd, 0, sizeof(cmd));
    }
    cmd[0] = dmubHeader(kDmubCmdIdleOpt, kIdleOptSetDcPowerState, false, 4);
    cmd[1] = on ? 1 : 8;
    bool ok = dmubCmd(cmd, false);
    LOG("dmub: dc power state %s %s\n", on ? "d0" : "d3", ok ? "done" : "failed");
    return ok;
}

// dc_dmub_srv_notify_stream_mask: tells the firmware which streams are live, done when it clears the status bit
void PDAmdGPU::dmubNotifyStreamMask(uint16_t mask)
{
    uint32_t cmd = (kGpintStreamMask << 16) | mask;
    wreg(regDMCUB_GPINT_DATAIN1, (1u << 28) | cmd);
    int i = 0;
    for (; i < 100000 && rreg(regDMCUB_GPINT_DATAIN1) != cmd; i++) IODelay(1);
    LOG("dmub: stream mask 0x%x %s\n", mask, i < 100000 ? "acked" : "not acked");
}

// transmitter disable on uniphy a, as link_disable does before the pll is released
void PDAmdGPU::hdmiLinkDown(void)
{
    dmubTransmitter(0, 0, 3, 4, 148500 / 10, 1, 0, 0x0c);
}

// the hdmi path from the display object table: uniphy a, dig 0, connector hdmi type a, hpd 1
void PDAmdGPU::hdmiLinkUp(void)
{
    const uint8_t phy = 0, hdmiMode = 3, conn = 0x0c, hpd = 1, encoderId = 0x1e, pll = 20;
    const uint32_t pixKhz = 148500;

    // no transmitter init, as enc31_hw_init sends none
    dmubSetPixelClock(pixKhz * 10, pll, encoderId, hdmiMode, 0);                     // the phy pll for 1080p60
    dmubTransmitter(phy, 1, hdmiMode, 4, pixKhz / 10, hpd, 0, conn);                // enable tmds on the phy
    LOG("dmub: after link up SCRATCH0 0x%08x\n", rreg(regDMCUB_SCRATCH0));
}
