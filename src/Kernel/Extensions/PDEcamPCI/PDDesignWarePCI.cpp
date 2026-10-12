#include "PDDesignWarePCI.h"

#include <IOKit/IOLib.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#define super PDEcamPCI
OSDefineMetaClassAndStructors(PDDesignWarePCI, PDEcamPCI)

// one device's config space. the least the config window can be
#define CONFIG_SPACE_SIZE           0x10000

// in a pci express capability: link capabilities and link status
#define PCIE_LINK_CAPS              0x0c
#define PCIE_LINK_STATUS            0x12
#define PCIE_LINK_CAPS_DLLLA        (1u << 20)
#define PCIE_LINK_STATUS_DLLLA      (1u << 13)
#define PCIE_LINK_STATUS_WIDTH(s)   (((s) >> 4) & 0x3f)

bool PDDesignWarePCI::mapConfigSpace(IOService *provider)
{
    IODeviceMemory *dbi = provider->getDeviceMemoryWithIndex(0);
    IODeviceMemory *config = provider->getDeviceMemoryWithIndex(1);

    if (!dbi || !config || dbi->getLength() < 0x100 || config->getLength() < CONFIG_SPACE_SIZE) {
        IOLog("PDDesignWarePCI: %s has no usable dbi and config in reg\n", provider->getName());
        return false;
    }

    readBusRange(provider);

    dbiMap = dbi->map(kIOMapInhibitCache);
    ecamMap = config->map(kIOMapInhibitCache);
    if (!dbiMap || !ecamMap) {
        IOLog("PDDesignWarePCI: could not map dbi or the config window\n");
        return false;
    }
    dbiBase = (volatile UInt8 *)dbiMap->getVirtualAddress();
    dbiLength = dbi->getLength();
    ecamBase = (volatile UInt8 *)ecamMap->getVirtualAddress();

    // the pci express capability for the link state every access below the port checks
    UInt8 cap = dbiBase[kIOPCIConfigCapabilitiesPtr] & 0xfc;
    for (unsigned i = 0; cap >= 0x40 && i < 48; i++) {
        if (dbiBase[cap] == kIOPCICapabilityIDPCIExpress) {
            expressCap = cap;
            break;
        }
        cap = dbiBase[cap + 1] & 0xfc;
    }
    if (!expressCap)
        IOLog("PDDesignWarePCI: the root port has no pci express capability, nothing below it is reached\n");

    logFirmwareState();
    return true;
}

// data link layer active when the port can report it. if it can't, a negotiated width
bool PDDesignWarePCI::linkUp(void) const
{
    if (!expressCap)
        return false;

    UInt32 caps = OSReadLittleInt32((volatile void *)dbiBase, expressCap + PCIE_LINK_CAPS);
    UInt16 status = OSReadLittleInt16((volatile void *)dbiBase, expressCap + PCIE_LINK_STATUS);
    if (caps & PCIE_LINK_CAPS_DLLLA)
        return (status & PCIE_LINK_STATUS_DLLLA) != 0;
    return PCIE_LINK_STATUS_WIDTH(status) != 0;
}

// what firmware left before the configurator changes any of it (root port, bus #s, link, device below)
void PDDesignWarePCI::logFirmwareState(void)
{
    UInt32 id = OSReadLittleInt32((volatile void *)dbiBase, kIOPCIConfigVendorID);
    UInt32 classCode = OSReadLittleInt32((volatile void *)dbiBase, kIOPCIConfigRevisionID) >> 8;
    UInt16 status = expressCap ?
        OSReadLittleInt16((volatile void *)dbiBase, expressCap + PCIE_LINK_STATUS) : 0;
    UInt8 secondary = dbiBase[kPCI2PCISecondaryBus];
    bool up = linkUp();

    IOLog("PDDesignWarePCI: root port %04x:%04x class %06x header 0x%02x buses %u-%u link 0x%04x %s\n",
          id & 0xffff, id >> 16, classCode, dbiBase[kIOPCIConfigHeaderType], secondary,
          dbiBase[kPCI2PCISubordinateBus], status, up ? "up" : "down");
    if (!up || secondary <= busFirst)
        return;
    id = OSReadLittleInt32((volatile void *)ecamBase, kIOPCIConfigVendorID);
    classCode = OSReadLittleInt32((volatile void *)ecamBase, kIOPCIConfigRevisionID) >> 8;
    IOLog("PDDesignWarePCI: bus %u device 0 is %04x:%04x class %06x\n", secondary, id & 0xffff,
          id >> 16, classCode);
}

// the root port is device 0 on the first bus and its config space is dbi
// below it only device 0 on its secondary bus is reached. The config window is
// one device's, so any other slot would alias it, and an access while the link
// is down may abort instead of reading ones
volatile UInt8 *PDDesignWarePCI::configAddress(IOPCIAddressSpace space, UInt8 offset) const
{
    // the extended reg bits carry offsets past the first 256 bytes
    IOByteCount reg = ((IOByteCount)space.es.registerNumExtended << 8) | offset;

    if (!dbiBase || space.s.deviceNum != 0 || space.s.functionNum != 0)
        return NULL;
    if (space.s.busNum == busFirst)
        return reg < (dbiLength & ~(IOByteCount)3) ? dbiBase + reg : NULL;

    UInt8 secondary = dbiBase[kPCI2PCISecondaryBus];
    if (secondary <= busFirst || space.s.busNum != secondary || space.s.busNum > busLast ||
        !linkUp())
        return NULL;
    return ecamBase + reg;
}

void PDDesignWarePCI::free(void)
{
    OSSafeReleaseNULL(dbiMap);
    dbiBase = NULL;
    super::free();
}
