#include "PDArmPCI.h"

#include <IOKit/IOLib.h>
#include <libkern/OSByteOrder.h>

#define super IOPCIBridge
OSDefineMetaClassAndStructors(PDArmPCI, IOPCIBridge)

/* QEMU's ARM virt machine exposes PCIe ECAM at this fixed address. */
static const IOPhysicalAddress kECAMBase = 0x4010000000ULL;
static const IOPhysicalLength kECAMLength = 0x10000000ULL;

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
    ecamMemory = IODeviceMemory::withRange(kECAMBase, kECAMLength);
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
    /* QEMU virt's non-prefetchable PCI window is 0x10000000..0x3fffffff. */
    addBridgeMemoryRange(0x10000000ULL, 0x30000000ULL, true);
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
    if (!ecamMap || space.s.busNum > 255 || space.s.deviceNum > 31 ||
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
