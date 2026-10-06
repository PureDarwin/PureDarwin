#include "PDArmPCI.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#define super IOPCIBridge
OSDefineMetaClassAndStructors(PDArmPCI, IOPCIBridge)

/* QEMU virt: 256-bus ECAM up high when the guest has >= 40-bit physical addresses,
 * else the 16-bus low ECAM (HVF on M1 gives 36 bits) */
static IOPhysicalAddress sECAMBase = 0x4010000000ULL;
static IOPhysicalLength sECAMLength = 0x10000000ULL;
static UInt32 sECAMBuses = 256;

static void pd_pci_pick_ecam(void)
{
    uint64_t mmfr0 = __builtin_arm_rsr64("ID_AA64MMFR0_EL1");
    static const UInt8 kPABits[] = { 32, 36, 40, 42, 44, 48, 52 };
    UInt32 pa = (mmfr0 & 0xf) < sizeof(kPABits) ? kPABits[mmfr0 & 0xf] : 52;

    if (pa < 40) {
        sECAMBase = 0x3f000000ULL;
        sECAMLength = 0x1000000ULL;
        sECAMBuses = 16;
    }
    IOLog("PureDarwin PDArmPCI: %u-bit PA, ECAM 0x%llx, %u buses\n", (unsigned)pa,
          (unsigned long long)sECAMBase, (unsigned)sECAMBuses);
}

IOService *PDArmPCI::probe(IOService *provider, SInt32 *score)
{
    IOLog("PureDarwin PDArmPCI: probe provider=%s\n",
          provider ? provider->getName() : "(null)");
    IOService *result = super::probe(provider, score);
    IOLog("PureDarwin PDArmPCI: probe result=%p score=%ld\n",
          result, score ? (long)*score : 0L);
    return result;
}

bool PDArmPCI::start(IOService *provider)
{
    IOLog("PureDarwin PDArmPCI: start provider=%s\n",
          provider ? provider->getName() : "(null)");
    pd_pci_pick_ecam();
    ecamMemory = IODeviceMemory::withRange(sECAMBase, sECAMLength);
    if (!ecamMemory)
        return false;

    ecamMap = ecamMemory->map();
    if (!ecamMap) {
        ecamMemory->release();
        ecamMemory = 0;
        return false;
    }

    return super::start(provider);
}

bool PDArmPCI::configure(IOService *provider)
{
    IOLog("PureDarwin PDArmPCI: configure\n");
    /* QEMU virt's low MMIO window is 0x10000000..0x3efeffff; PIO and the low ECAM follow it */
    addBridgeMemoryRange(0x10000000ULL, 0x2eff0000ULL, true);
    return super::configure(provider);
}

void PDArmPCI::free(void)
{
    if (ecamMap) {
        ecamMap->release();
        ecamMap = 0;
    }
    if (ecamMemory) {
        ecamMemory->release();
        ecamMemory = 0;
    }
    super::free();
}

IODeviceMemory *PDArmPCI::ioDeviceMemory(void)
{
    return ecamMemory;
}

UInt8 PDArmPCI::firstBusNum(void) { return 0; }
UInt8 PDArmPCI::lastBusNum(void) { return 255; }

IOPCIAddressSpace PDArmPCI::getBridgeSpace(void)
{
    IOPCIAddressSpace space;
    space.bits = 0;
    return space;
}

volatile UInt8 *PDArmPCI::configAddress(IOPCIAddressSpace space, UInt8 offset) const
{
    if (!ecamMap || space.s.busNum >= sECAMBuses || space.s.deviceNum > 31 ||
        space.s.functionNum > 7)
        return 0;

    /*
     * ECAM address: bus/device/function select the 4 KB config window, and the
     * full byte offset selects the register.  The offset must NOT be rounded to
     * a dword boundary here: configRead8/16 need the exact byte, and a dword
     * mask (offset & 0xfc) makes e.g. the header-type register at 0x0e read
     * 0x0c instead, so every device is misparsed and rejected during probe.
     */
    IOByteCount address = ((IOByteCount)space.s.busNum << 20) |
                          ((IOByteCount)space.s.deviceNum << 15) |
                          ((IOByteCount)space.s.functionNum << 12) |
                          (offset & 0xfff);
    return (volatile UInt8 *)(ecamMap->getVirtualAddress() + address);
}

UInt32 PDArmPCI::configRead32(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return 0xffffffff;
    UInt32 value = OSReadLittleInt32((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDArmPCI::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return;
    OSWriteLittleInt32((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt16 PDArmPCI::configRead16(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return 0xffff;
    UInt16 value = OSReadLittleInt16((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDArmPCI::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return;
    OSWriteLittleInt16((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt8 PDArmPCI::configRead8(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return 0xff;
    UInt8 value = *address;
    OSSynchronizeIO();
    return value;
}

void PDArmPCI::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address) return;
    *address = data;
    OSSynchronizeIO();
}

// a tree cell, which the loader may have left big-endian
static UInt32 dtCell(const UInt32 *cells, UInt32 i, bool swapped)
{
    return swapped ? OSSwapInt32(cells[i]) : cells[i];
}

static bool dtU32(IORegistryEntry *entry, const char *name, bool swapped, UInt32 *value)
{
    OSData *data = OSDynamicCast(OSData, entry->getProperty(name));

    if (!data || data->getLength() < sizeof(UInt32))
        return false;
    *value = dtCell((const UInt32 *)data->getBytesNoCopy(), 0, swapped);
    return true;
}

// whether a node's compatible list names this
static bool dtCompatible(IORegistryEntry *entry, const char *name)
{
    OSData *data = OSDynamicCast(OSData, entry->getProperty("compatible"));
    const char *p = data ? (const char *)data->getBytesNoCopy() : NULL;
    const char *end = p ? p + data->getLength() : NULL;

    for (; p && p < end; p += strnlen(p, end - p) + 1) {
        if (strncmp(p, name, end - p) == 0)
            return true;
    }
    return false;
}

// #address-cells and #interrupt-cells of the interrupt parent, the gicv3 node's 2 and 3 by default,
// and whether it is a gic PDArmGIC serves (v3, or virt's v2)
static void parentCells(UInt32 phandle, bool swapped, UInt32 *addrCells, UInt32 *intCells, bool *served)
{
    IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
    IORegistryIterator *iter = root ? IORegistryIterator::iterateOver(root, gIODTPlane,
        kIORegistryIterateRecursively) : NULL;
    IORegistryEntry *next;
    UInt32 ph;

    *addrCells = 2;
    *intCells = 3;
    *served = false;
    while (iter && (next = iter->getNextObject())) {
        if (!(dtU32(next, "phandle", swapped, &ph) || dtU32(next, "AAPL,phandle", swapped, &ph)) || ph != phandle)
            continue;
        if (!dtU32(next, "#address-cells", swapped, addrCells))
            *addrCells = 0;
        dtU32(next, "#interrupt-cells", swapped, intCells);
        *served = dtCompatible(next, "arm,gic-v3") || dtCompatible(next, "arm,cortex-a15-gic");
        break;
    }
    OSSafeReleaseNULL(iter);
    OSSafeReleaseNULL(root);
}

// a root bus slot's pin (1-4) through the host bridge node's interrupt-map, as a gic intid
bool PDArmPCI::routeINTx(UInt32 device, UInt32 pin, UInt32 *intid)
{
    IOService *node = getProvider();
    OSData *map = node ? OSDynamicCast(OSData, node->getProperty("interrupt-map")) : NULL;
    OSData *maskData = node ? OSDynamicCast(OSData, node->getProperty("interrupt-map-mask")) : NULL;
    UInt32 mask[4] = { 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff };
    bool swapped = false;

    if (!map) {
        // the loader's pci node carries no interrupt-map: qemu virt wires slot s pin p (1-4) to spi
        // 3 + (p - 1 + s) % 4. its gic, v2 or v3, is PDArmGIC's
        *intid = 32 + 3 + (pin - 1 + device) % 4;
        return true;
    }
    const UInt32 *cells = (const UInt32 *)map->getBytesNoCopy();
    UInt32 count = map->getLength() / sizeof(UInt32);
    if (maskData && maskData->getLength() >= sizeof(mask)) {
        memcpy(mask, maskData->getBytesNoCopy(), sizeof(mask));
        // the pin mask is 7, so a big value means the cells were left big-endian
        swapped = mask[3] > 0xffff;
    } else if (count > 3) {
        swapped = cells[3] > 0xffff;
    }
    for (unsigned i = 0; swapped && i < 4; i++)
        mask[i] = OSSwapInt32(mask[i]);

    UInt32 child[4] = { device << 11, 0, 0, pin };
    for (UInt32 at = 0; at + 5 <= count;) {
        UInt32 pAddr, pInt;
        bool served;

        parentCells(dtCell(cells, at + 4, swapped), swapped, &pAddr, &pInt, &served);
        UInt32 next = at + 5 + pAddr + pInt;
        if (next > count || pInt < 2)
            return false;
        bool match = true;
        for (unsigned i = 0; i < 4 && match; i++)
            match = (dtCell(cells, at + i, swapped) & mask[i]) == (child[i] & mask[i]);
        if (match) {
            // resolving an interrupt waits for its controller by name, forever where PDArmGIC
            // never registers
            if (!served)
                return false;
            // gic cells: 0 for an spi or 1 for a ppi, then the number
            UInt32 type = dtCell(cells, at + 5 + pAddr, swapped);
            *intid = dtCell(cells, at + 6 + pAddr, swapped) + (type == 0 ? 32 : 16);
            return true;
        }
        at = next;
    }
    return false;
}

// legacy intx as a PDArmGIC intid, for the root bus devices this bridge publishes
void PDArmPCI::setINTx(IOPCIDevice *nub)
{
    UInt32 pin = nub->configRead8(kIOPCIConfigInterruptPin);
    UInt32 device = nub->getDeviceNumber();
    UInt32 intid;

    if (pin == 0 || pin > 4)
        return;
    pin--;
    if (!routeINTx(device, pin + 1, &intid)) {
        IOLog("PureDarwin PDArmPCI: no gic route for %s pin %u, it polls\n", nub->getName(), pin + 1);
        return;
    }

    const OSSymbol *gic = OSSymbol::withCString("PDArmGIC");
    OSData *spec = OSData::withBytes(&intid, sizeof(intid));
    OSArray *controllers = gic ? OSArray::withObjects((const OSObject **)&gic, 1) : NULL;
    OSArray *specifiers = spec ? OSArray::withObjects((const OSObject **)&spec, 1) : NULL;
    if (controllers && specifiers) {
        nub->setProperty(gIOInterruptControllersKey, controllers);
        nub->setProperty(gIOInterruptSpecifiersKey, specifiers);
        IOLog("PureDarwin PDArmPCI: %s intx is gic intid %u\n", nub->getName(), intid);
    }
    OSSafeReleaseNULL(gic);
    OSSafeReleaseNULL(spec);
    OSSafeReleaseNULL(controllers);
    OSSafeReleaseNULL(specifiers);
}

// the family resolves no legacy interrupts here (no ResolvePCIInterrupt on this platform), so name
// them before the nub is registered and matched
bool PDArmPCI::publishNub(IOPCIDevice *nub, UInt32 index)
{
    if (nub && !nub->getProperty(gIOInterruptControllersKey))
        setINTx(nub);
    return super::publishNub(nub, index);
}
