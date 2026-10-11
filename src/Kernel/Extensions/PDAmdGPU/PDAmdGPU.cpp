#include "PDAmdGPU.h"

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

bool PDAmdGPU::start(IOService *provider)
{
    if (!super::start(provider)) return false;

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
    if (mmMP0_SMN_C2PMSG_33 < regCount) LOG("MP0 C2PMSG_33 0x%08x\n", rreg(mmMP0_SMN_C2PMSG_33));

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
    smuInit();
    probePsp();
    return true;
}

void PDAmdGPU::stop(IOService *provider)
{
    OSSafeReleaseNULL(doorbellMap);
    OSSafeReleaseNULL(regMap);
    super::stop(provider);
}
