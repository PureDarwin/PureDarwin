#include "PDAmdGPU.h"
#include <IOKit/pwr_mgt/IOPMpowerState.h>

#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(PDAmdGPU, IOService);

static const char *barKind(IOPCIDevice *pci, int reg)
{
    uint32_t v = pci->configRead32(reg);

    if (v & 1) return "io";
    return (v & 6) == 4 ? "mem64" : "mem32";
}

// reads framebuffer memory through the MM_INDEX window, no aperture needed
void PDAmdGPU::readVram(uint64_t pos, void *buf, uint32_t len)
{
    uint32_t *out = (uint32_t *)buf;

    for (uint32_t i = 0; i < len / 4; i++, pos += 4) {
        wreg(mmMM_INDEX, (uint32_t)pos | 0x80000000);
        wreg(mmMM_INDEX_HI, (uint32_t)(pos >> 31));
        out[i] = rreg(mmMM_DATA);
    }
}

// the chips this driver knows, by the ip versions their discovery table reports
static const AmdChip kAmdChips[] = {
    // smu 13.0.5 (smu_v13_0_5_ppt.c), dcn315_smu.c's display mailbox
    { "Granite Ridge", { 10, 3, 6 }, { 13, 0, 5 }, { 3, 1, 5 }, true, &kAmdFirmwareGraniteRidge,
      0xbee142 + 0xb00000 / 4, 0xbee262 + 0xb00000 / 4, 0xbee261 + 0xb00000 / 4, 10, 23, 26,
      0x3b1050c / 4, 0x265, 0x266, true },
    // smu 13.0.8 is yellow carp's (yellow_carp_ppt.c, smu_v13_0_1_ppsmc.h), dcn316_smu.c's display mailbox
    { "Mendocino", { 10, 3, 7 }, { 13, 0, 8 }, { 3, 1, 6 }, false, &kAmdFirmwareMendocino,
      0x282, 0x292, 0x29a, 0x11, 0x1a, 0x0c,
      0x283, 0x293, 0x29b, false },
};

static bool ipIs(const AmdIp *ip, const uint8_t v[3])
{
    return ip && ip->major == v[0] && ip->minor == v[1] && ip->rev == v[2];
}

static const AmdChip *amdIdentify(const AmdIpTable &ips)
{
    const AmdIp *gc = ips.find(kHwGC), *mp0 = ips.find(kHwMP0), *dmu = ips.find(kHwDMU);

    for (const AmdChip &c : kAmdChips) {
        if (ipIs(gc, c.gc) && ipIs(mp0, c.mp0) && ipIs(dmu, c.dmu)) return &c;
    }
    LOG("unknown ip set: gc %u.%u.%u mp0 %u.%u.%u dmu %u.%u.%u\n", gc ? gc->major : 0, gc ? gc->minor : 0, gc ? gc->rev : 0,
        mp0 ? mp0->major : 0, mp0 ? mp0->minor : 0, mp0 ? mp0->rev : 0, dmu ? dmu->major : 0, dmu ? dmu->minor : 0,
        dmu ? dmu->rev : 0);
    return nullptr;
}

bool PDAmdGPU::start(IOService *provider)
{
    if (!super::start(provider)) return false;

    // on the power plane, so the system shutdown reaches systemWillShutdown
    static IOPMPowerState states[2] = {
        { kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
        { kIOPMPowerStateVersion1, kIOPMPowerOn, kIOPMPowerOn, kIOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
    };
    PMinit();
    provider->joinPMtree(this);
    registerPowerDriver(this, states, 2);

    pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci) return false;

    LOG("%04x:%04x rev %02x subsys %04x:%04x\n", pci->configRead16(kIOPCIConfigVendorID),
        pci->configRead16(kIOPCIConfigDeviceID), pci->configRead8(kIOPCIConfigRevisionID),
        pci->configRead16(kIOPCIConfigSubSystemVendorID), pci->configRead16(kIOPCIConfigSubSystemID));

    for (uint32_t i = 0; i < pci->getDeviceMemoryCount(); i++) {
        IODeviceMemory *m = pci->getDeviceMemoryWithIndex(i);
        if (m) LOG("range %u phys 0x%llx len 0x%llx\n", i, (uint64_t)m->getPhysicalAddress(), (uint64_t)m->getLength());
    }
    // the upper half of a 64-bit bar is skipped
    for (int r = kIOPCIConfigBaseAddress0; r <= kIOPCIConfigBaseAddress5; r += 4) {
        const char *kind = barKind(pci, r);
        LOG("bar%d raw 0x%08x %s\n", (r - kIOPCIConfigBaseAddress0) / 4, pci->configRead32(r), kind);
        if (kind[3] == '6') r += 4;
    }

    pci->setMemoryEnable(true);

    // gfx9 and later keep the registers in bar5
    regMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress5, kIOMapInhibitCache);
    if (!regMap) {
        LOG("cannot map bar5 registers\n");
        return true;
    }
    regs = (volatile uint32_t *)regMap->getVirtualAddress();
    regCount = regMap->getLength() / 4;
    LOG("bar5 regs phys 0x%llx len 0x%llx\n", (uint64_t)regMap->getPhysicalAddress(), (uint64_t)regMap->getLength());

    uint32_t memsize = rreg(mmRCC_CONFIG_MEMSIZE);
    LOG("RCC_CONFIG_MEMSIZE %u MiB\n", memsize);
    // the psp mailbox offset is granite ridge's, other chips are only read once discovery names them
    if (pci->configRead16(kIOPCIConfigDeviceID) == 0x13c0 && mmMP0_SMN_C2PMSG_33 < regCount)
        LOG("MP0 C2PMSG_33 0x%08x\n", rreg(mmMP0_SMN_C2PMSG_33));

    if (memsize == 0 || memsize == 0xffffffff) {
        LOG("no carveout size, skipping discovery\n");
        return true;
    }

    uint8_t *bin = (uint8_t *)IOMallocZeroData(kAmdDiscoverySize);
    if (!bin) return true;

    vramSize = (uint64_t)memsize << 20;
    readVram(vramSize - kAmdDiscoverySize, bin, kAmdDiscoverySize);
    bool found = amdParseDiscovery(bin, kAmdDiscoverySize, &ips);
    IOFreeData(bin, kAmdDiscoverySize);

    if (!found) return true;

    chip = amdIdentify(ips);
    if (!chip) {
        LOG("not a chip this driver knows, leaving it alone\n");
        return true;
    }
    // the segment bases the driver's fixed register indices depend on, logged to compare chips
    static const struct { uint16_t hw; const char *name; } key[] = {
        { kHwGC, "GC" }, { kHwMP0, "MP0" }, { kHwMP1, "MP1" }, { kHwDMU, "DMU" }, { 42, "SDMA0" },
        { kHwNBIF, "NBIF" }, { 41, "HDP" }, { 34, "MMHUB" }, { kHwOSSSYS, "OSSSYS" }, { 4, "SMUIO" },
    };
    for (const auto &k : key) {
        const AmdIp *ip = ips.find(k.hw);
        if (!ip) continue;
        LOG("%s bases %x %x %x %x %x %x\n", k.name, ip->count > 0 ? ip->base[0] : 0, ip->count > 1 ? ip->base[1] : 0,
            ip->count > 2 ? ip->base[2] : 0, ip->count > 3 ? ip->base[3] : 0, ip->count > 4 ? ip->base[4] : 0,
            ip->count > 5 ? ip->base[5] : 0);
    }
    if (!chip->bringUp) {
        // the two mailboxes the table describes, each asked for its firmware version and nothing else
        smuInit();
        int32_t disp = dispSmuSend(2, 0);
        LOG("%s: display pmfw 0x%x\n", chip->name, disp);
        LOG("%s: discovery read, bring-up not supported yet, leaving it alone\n", chip->name);
        return true;
    }
    LOG("%s\n", chip->name);

    // the teardown's mode 2 reset clears the posted bit, so the next start posts before the psp, as amdgpu does at load
    asicPost(false);
    smuInit();
    probePsp();
    return true;
}

// amdgpu_pci_shutdown: suspend without a reset, so the next driver finds the display phy usable
void PDAmdGPU::systemWillShutdown(IOOptionBits specifier)
{
    if (running) {
        LOG("shutdown: tearing down\n");
        teardown(false);
        running = false;
    }
    super::systemWillShutdown(specifier);
}

void PDAmdGPU::stop(IOService *provider)
{
    PMstop();
    if (vbios) IOFreeData(vbios, vbiosSize);
    OSSafeReleaseNULL(doorbellMap);
    OSSafeReleaseNULL(vramMap);
    OSSafeReleaseNULL(regMap);
    super::stop(provider);
}
