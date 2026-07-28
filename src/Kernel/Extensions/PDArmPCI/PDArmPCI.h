#ifndef _PDARMPCI_H
#define _PDARMPCI_H

#include <IOKit/pci/IOPCIBridge.h>

class PDArmPCI : public IOPCIBridge
{
    OSDeclareDefaultStructors(PDArmPCI)

    IODeviceMemory *ecamMemory;
    IOMemoryMap *ecamMap;

    volatile UInt8 *configAddress(IOPCIAddressSpace space, UInt8 offset) const;

public:
    IOService *probe(IOService *provider, SInt32 *score) APPLE_KEXT_OVERRIDE;
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
    void free(void) APPLE_KEXT_OVERRIDE;
    IODeviceMemory *ioDeviceMemory(void) APPLE_KEXT_OVERRIDE;

    UInt8 firstBusNum(void) APPLE_KEXT_OVERRIDE;
    UInt8 lastBusNum(void) APPLE_KEXT_OVERRIDE;
    IOPCIAddressSpace getBridgeSpace(void) APPLE_KEXT_OVERRIDE;

    UInt32 configRead32(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite32(IOPCIAddressSpace space, UInt8 offset, UInt32 data) APPLE_KEXT_OVERRIDE;
    UInt16 configRead16(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite16(IOPCIAddressSpace space, UInt8 offset, UInt16 data) APPLE_KEXT_OVERRIDE;
    UInt8 configRead8(IOPCIAddressSpace space, UInt8 offset) APPLE_KEXT_OVERRIDE;
    void configWrite8(IOPCIAddressSpace space, UInt8 offset, UInt8 data) APPLE_KEXT_OVERRIDE;
};

#endif
