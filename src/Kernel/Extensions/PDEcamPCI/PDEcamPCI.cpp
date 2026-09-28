#include "PDEcamPCI.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOKitKeys.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>

#define super IOPCIBridge
OSDefineMetaClassAndStructors(PDEcamPCI, IOPCIBridge)

// each bus takes 1 MiB of ecam, 32 devices of 8 functions of 4 KiB
#define ECAM_BUS_SHIFT          20
#define ECAM_DEVICE_SHIFT       15
#define ECAM_FUNCTION_SHIFT     12

// the space code in bits 24-25 of a pci address's first cell, from the pci bus binding
#define PCI_ADDR_SPACE(hi)      (((hi) >> 24) & 3)
#define PCI_ADDR_PREFETCH       0x40000000u
#define PCI_SPACE_CONFIG        0
#define PCI_SPACE_IO            1
#define PCI_SPACE_MEM64         3

// what the pci binding gives a host bridge node, 3 address cells and 1 interrupt cell
#define PCI_ADDR_CELLS          3
#define PCI_INT_CELLS           1
#define MAX_PARENT_CELLS        4

static const char kResolvePCIInterrupt[] = "ResolvePCIInterrupt";
static const char kSetDeviceInterrupts[] = "SetDeviceInterrupts";

// the loader stores 1 cell as a u32 and n > 1 cells with their low 64 bits as one u64 at the end
static UInt64 readCells(const UInt8 *p, UInt32 cells)
{
    UInt32 v32;
    UInt64 v64;

    if (cells == 0)
        return 0;
    if (cells == 1) {
        memcpy(&v32, p, sizeof(v32));
        return v32;
    }
    memcpy(&v64, p + (cells - 2) * sizeof(UInt32), sizeof(v64));
    return v64;
}

static bool getU32(IORegistryEntry *entry, const char *name, UInt32 *value)
{
    OSData *data = OSDynamicCast(OSData, entry->getProperty(name));

    if (!data || data->getLength() < sizeof(UInt32))
        return false;
    memcpy(value, data->getBytesNoCopy(), sizeof(*value));
    return true;
}

static UInt32 cellsOf(IORegistryEntry *entry, const char *name, UInt32 fallback)
{
    UInt32 v;

    if (!entry || !getU32(entry, name, &v) || v > MAX_PARENT_CELLS)
        return fallback;
    return v;
}

// the device tree node with this phandle, retained
static IORegistryEntry *nodeForPHandle(UInt32 phandle)
{
    IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
    IORegistryIterator *iter;
    IORegistryEntry *found = NULL;

    if (!root)
        return NULL;
    iter = IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively);
    while (iter && !found) {
        IORegistryEntry *next = iter->getNextObject();
        UInt32 ph;

        if (!next)
            break;
        if ((getU32(next, "AAPL,phandle", &ph) || getU32(next, "phandle", &ph)) && ph == phandle) {
            found = next;
            found->retain();
        }
    }
    OSSafeReleaseNULL(iter);
    root->release();
    return found;
}

bool PDEcamPCI::mapECAM(IOService *provider)
{
    IODeviceMemory *mem = provider->getDeviceMemoryWithIndex(0);
    UInt32 range[2];
    OSData *busRange = OSDynamicCast(OSData, provider->getProperty("bus-range"));

    if (!mem || mem->getLength() < (1u << ECAM_BUS_SHIFT)) {
        IOLog("PDEcamPCI: %s has no usable reg for the ecam window\n", provider->getName());
        return false;
    }

    busFirst = 0;
    busLast = 255;
    if (busRange && busRange->getLength() >= sizeof(range)) {
        memcpy(range, busRange->getBytesNoCopy(), sizeof(range));
        if (range[0] <= range[1] && range[1] <= 255) {
            busFirst = (UInt8)range[0];
            busLast = (UInt8)range[1];
        }
    }

    // the window starts at the first bus and may cover fewer buses than bus-range names
    UInt64 buses = mem->getLength() >> ECAM_BUS_SHIFT;
    if ((UInt64)busLast - busFirst + 1 > buses)
        busLast = (UInt8)(busFirst + buses - 1);

    ecamMap = mem->map(kIOMapInhibitCache);
    if (!ecamMap) {
        IOLog("PDEcamPCI: could not map the ecam window\n");
        return false;
    }
    ecamBase = (volatile UInt8 *)ecamMap->getVirtualAddress();
    return true;
}

bool PDEcamPCI::start(IOService *provider)
{
    IORegistryEntry *node = provider;
    OSData *mask;

    if (!mapECAM(provider))
        return false;

    addrCells = cellsOf(node, "#address-cells", PCI_ADDR_CELLS);
    intCells = cellsOf(node, "#interrupt-cells", PCI_INT_CELLS);
    if (addrCells != PCI_ADDR_CELLS || intCells != PCI_INT_CELLS) {
        IOLog("PDEcamPCI: unexpected #address-cells %u #interrupt-cells %u, no intx routing\n",
              addrCells, intCells);
    } else {
        intMap = OSDynamicCast(OSData, node->getProperty("interrupt-map"));
        if (intMap)
            intMap->retain();
        // a missing mask compares every bit
        for (unsigned i = 0; i < 4; i++)
            intMapMask[i] = 0xffffffff;
        mask = OSDynamicCast(OSData, node->getProperty("interrupt-map-mask"));
        if (mask && mask->getLength() >= sizeof(intMapMask))
            memcpy(intMapMask, mask->getBytesNoCopy(), sizeof(intMapMask));
    }
    if (!intMap)
        IOLog("PDEcamPCI: no interrupt-map, pci devices get no legacy interrupts\n");

    // the platform sends devices on the root bus here through these resources
    publishResource(kResolvePCIInterrupt, this);
    publishResource(kSetDeviceInterrupts, this);

    return super::start(provider);
}

// every ranges entry is a pci address, a cpu address and a size
void PDEcamPCI::addWindows(IORegistryEntry *node)
{
    OSData *ranges = OSDynamicCast(OSData, node->getProperty("ranges"));
    IORegistryEntry *parent = node->getParentEntry(gIODTPlane);
    UInt32 parentCells = cellsOf(parent, "#address-cells", 2);
    UInt32 sizeCells = cellsOf(node, "#size-cells", 2);
    UInt32 stride = (PCI_ADDR_CELLS + parentCells + sizeCells) * sizeof(UInt32);

    if (!ranges || ranges->getLength() < stride) {
        IOLog("PDEcamPCI: no ranges, nothing to place bars in\n");
        return;
    }

    const UInt8 *p = (const UInt8 *)ranges->getBytesNoCopy();
    for (UInt32 at = 0; at + stride <= ranges->getLength(); at += stride) {
        UInt32 hi;
        memcpy(&hi, p + at, sizeof(hi));
        UInt64 pci = readCells(p + at, PCI_ADDR_CELLS);
        UInt64 cpu = readCells(p + at + PCI_ADDR_CELLS * sizeof(UInt32), parentCells);
        UInt64 size = readCells(p + at + (PCI_ADDR_CELLS + parentCells) * sizeof(UInt32), sizeCells);
        UInt32 space = PCI_ADDR_SPACE(hi);
        bool ok = false;

        // a config space entry maps nothing a bar can use
        if (size == 0 || space == PCI_SPACE_CONFIG)
            continue;
        if (space == PCI_SPACE_IO) {
            // io bars are pci port numbers, the window maps port 0 onwards into cpu space
            if (!ioSpace && pci <= cpu && pci + size <= 0x100000000ULL) {
                ioSpace = IODeviceMemory::withRange(cpu - pci, pci + size);
                ok = ioSpace && addBridgeIORange((IOByteCount)pci, (IOByteCount)size);
            }
        } else if (pci != cpu) {
            // the configurator programs bars with cpu addresses, a translated window would break them
            IOLog("PDEcamPCI: skipping memory window pci 0x%llx at cpu 0x%llx\n",
                  (unsigned long long)pci, (unsigned long long)cpu);
            continue;
        } else if (cpu + size > 0x100000000ULL) {
            // the configurator keeps every bar below 4 GiB off x86, a window up there is never used
            ok = false;
        } else if (hi & PCI_ADDR_PREFETCH) {
            ok = addBridgePrefetchableMemoryRange((addr64_t)cpu, (addr64_t)size);
        } else {
            ok = addBridgeMemoryRange((IOPhysicalAddress)cpu, (IOPhysicalLength)size, true);
        }
        if (!ok)
            IOLog("PDEcamPCI: window pci 0x%llx cpu 0x%llx size 0x%llx not used\n",
                  (unsigned long long)pci, (unsigned long long)cpu, (unsigned long long)size);
    }
}

bool PDEcamPCI::configure(IOService *provider)
{
    addWindows(provider);
    return super::configure(provider);
}

void PDEcamPCI::free(void)
{
    OSSafeReleaseNULL(ecamMap);
    OSSafeReleaseNULL(ioSpace);
    OSSafeReleaseNULL(intMap);
    ecamBase = NULL;
    super::free();
}

IODeviceMemory *PDEcamPCI::ioDeviceMemory(void)
{
    return ioSpace;
}

UInt8 PDEcamPCI::firstBusNum(void) { return busFirst; }
UInt8 PDEcamPCI::lastBusNum(void) { return busLast; }

IOPCIAddressSpace PDEcamPCI::getBridgeSpace(void)
{
    IOPCIAddressSpace space;
    space.bits = 0;
    space.s.busNum = busFirst;
    return space;
}

// look up a root bus slot's pin in interrupt-map, the pin counts from 1 as the binding has it
bool PDEcamPCI::routeINTx(UInt32 bus, UInt32 device, UInt32 pin, UInt32 *phandle,
                          UInt32 *spec, UInt32 *specCells)
{
    UInt32 child[PCI_ADDR_CELLS + PCI_INT_CELLS] = { (bus << 16) | (device << 11), 0, 0, pin };

    if (!intMap)
        return false;
    for (unsigned i = 0; i < 4; i++)
        child[i] &= intMapMask[i];

    const UInt32 *cells = (const UInt32 *)intMap->getBytesNoCopy();
    UInt32 count = intMap->getLength() / sizeof(UInt32);

    for (UInt32 at = 0; at + 5 <= count;) {
        UInt32 ph = cells[at + 4];
        IORegistryEntry *parent = nodeForPHandle(ph);
        UInt32 pAddr, pInt;

        if (!parent) {
            IOLog("PDEcamPCI: interrupt-map names unknown phandle 0x%x\n", ph);
            return false;
        }
        // a parent without #address-cells takes none in the map
        pAddr = cellsOf(parent, "#address-cells", 0);
        pInt = cellsOf(parent, "#interrupt-cells", 1);
        bool controller = parent->getProperty("interrupt-controller") != NULL;
        parent->release();

        UInt32 next = at + 5 + pAddr + pInt;
        if (next > count || pInt == 0)
            return false;

        bool match = true;
        for (unsigned i = 0; i < 4 && match; i++)
            match = (cells[at + i] & intMapMask[i]) == child[i];
        if (match) {
            if (!controller) {
                IOLog("PDEcamPCI: interrupt-map parent 0x%x is a nexus, not followed\n", ph);
                return false;
            }
            *phandle = ph;
            *specCells = pInt;
            memcpy(spec, &cells[at + 5 + pAddr], pInt * sizeof(UInt32));
            return true;
        }
        at = next;
    }
    return false;
}

// the host bridge whose root bus this provider stands for
PDEcamPCI *PDEcamPCI::hostFor(IOService *busProvider)
{
    PDEcamPCI *host = OSDynamicCast(PDEcamPCI, busProvider);
    OSIterator *kids;

    if (host || !busProvider)
        return host;
    kids = busProvider->getClientIterator();
    while (kids && !host) {
        OSObject *next = kids->getNextObject();
        if (!next)
            break;
        host = OSDynamicCast(PDEcamPCI, next);
    }
    OSSafeReleaseNULL(kids);
    return host;
}

// behind a pci bridge the pin rotates by slot on the way up, the standard swizzle
bool PDEcamPCI::routeFromBus(IOService *busProvider, UInt32 device, UInt32 pin,
                             UInt32 *phandle, UInt32 *spec, UInt32 *specCells)
{
    while (IOPCIDevice *bridgeDevice = OSDynamicCast(IOPCIDevice, busProvider)) {
        pin = (pin + device) % 4;
        device = bridgeDevice->getDeviceNumber();
        busProvider = bridgeDevice->getProvider();
        if (!busProvider)
            return false;
        // a pci bridge's provider is the bridge driver above, the host bridge for the root bus
        if (!OSDynamicCast(PDEcamPCI, busProvider))
            busProvider = busProvider->getProvider();
    }

    PDEcamPCI *host = hostFor(busProvider);
    if (!host)
        return false;
    return host->routeINTx(host->busFirst, device, pin + 1, phandle, spec, specCells);
}

// the device's interrupt as the controller named after the map's parent phandle sees it
IOReturn PDEcamPCI::setNubInterrupts(IOService *service)
{
    IOPCIDevice *nub = OSDynamicCast(IOPCIDevice, service);
    UInt32 phandle, spec[MAX_PARENT_CELLS], specCells;
    char name[48];

    if (!nub)
        return kIOReturnBadArgument;
    UInt32 pin = nub->configRead8(kIOPCIConfigInterruptPin);
    if (pin == 0 || pin > 4)
        return kIOReturnUnsupported;
    IOService *bridge = nub->getProvider();
    IOService *busProvider = OSDynamicCast(PDEcamPCI, bridge) ? bridge : (bridge ? bridge->getProvider() : NULL);
    if (!routeFromBus(busProvider, nub->getDeviceNumber(), pin - 1, &phandle, spec, &specCells)) {
        IOLog("PDEcamPCI: no interrupt route for %s pin %u\n", nub->getName(), pin);
        return kIOReturnNoInterrupt;
    }

    snprintf(name, sizeof(name), "IOInterruptController%08X", phandle);
    const OSSymbol *controller = OSSymbol::withCString(name);
    OSData *specifier = OSData::withBytes(spec, specCells * sizeof(UInt32));
    OSArray *controllers = controller ? OSArray::withObjects((const OSObject **)&controller, 1) : NULL;
    OSArray *specifiers = specifier ? OSArray::withObjects((const OSObject **)&specifier, 1) : NULL;
    IOReturn ret = kIOReturnNoMemory;

    if (controllers && specifiers) {
        nub->setProperty(gIOInterruptControllersKey, controllers);
        nub->setProperty(gIOInterruptSpecifiersKey, specifiers);
        ret = kIOReturnSuccess;
    }
    OSSafeReleaseNULL(controller);
    OSSafeReleaseNULL(specifier);
    OSSafeReleaseNULL(controllers);
    OSSafeReleaseNULL(specifiers);
    return ret;
}

// ResolvePCIInterrupt gives the bus provider, slot and 0-based pin, SetDeviceInterrupts the nub
IOReturn PDEcamPCI::callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                         void *param1, void *param2,
                                         void *param3, void *param4)
{
    if (functionName->isEqualTo(kResolvePCIInterrupt)) {
        UInt32 phandle, spec[MAX_PARENT_CELLS], specCells;
        UInt32 device = (UInt32)(uintptr_t)param2;
        UInt32 pin = (UInt32)(uintptr_t)param3;

        if (pin > 3 || !param4 ||
            !routeFromBus((IOService *)param1, device, pin, &phandle, spec, &specCells))
            return kIOReturnUnsupported;
        *(UInt32 *)param4 = spec[0];
        return kIOReturnSuccess;
    }
    if (functionName->isEqualTo(kSetDeviceInterrupts))
        return setNubInterrupts((IOService *)param1);

    return super::callPlatformFunction(functionName, waitForFunction, param1, param2, param3, param4);
}

volatile UInt8 *PDEcamPCI::configAddress(IOPCIAddressSpace space, UInt8 offset) const
{
    if (!ecamBase || space.s.busNum < busFirst || space.s.busNum > busLast)
        return NULL;

    // the extended register bits carry offsets past the first 256 bytes
    IOByteCount address = ((IOByteCount)(space.s.busNum - busFirst) << ECAM_BUS_SHIFT) |
                          ((IOByteCount)space.s.deviceNum << ECAM_DEVICE_SHIFT) |
                          ((IOByteCount)space.s.functionNum << ECAM_FUNCTION_SHIFT) |
                          ((IOByteCount)space.es.registerNumExtended << 8) | offset;
    return ecamBase + address;
}

UInt32 PDEcamPCI::configRead32(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset & ~3);
    if (!address)
        return 0xffffffff;
    UInt32 value = OSReadLittleInt32((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDEcamPCI::configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data)
{
    volatile UInt8 *address = configAddress(space, offset & ~3);
    if (!address)
        return;
    OSWriteLittleInt32((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt16 PDEcamPCI::configRead16(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset & ~1);
    if (!address)
        return 0xffff;
    UInt16 value = OSReadLittleInt16((volatile void *)address, 0);
    OSSynchronizeIO();
    return value;
}

void PDEcamPCI::configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data)
{
    volatile UInt8 *address = configAddress(space, offset & ~1);
    if (!address)
        return;
    OSWriteLittleInt16((volatile void *)address, 0, data);
    OSSynchronizeIO();
}

UInt8 PDEcamPCI::configRead8(IOPCIAddressSpace space, UInt8 offset)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address)
        return 0xff;
    UInt8 value = *address;
    OSSynchronizeIO();
    return value;
}

void PDEcamPCI::configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data)
{
    volatile UInt8 *address = configAddress(space, offset);
    if (!address)
        return;
    *address = data;
    OSSynchronizeIO();
}
