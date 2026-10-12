#ifndef _PDECAMPCI_H
#define _PDECAMPCI_H

#include <IOKit/pci/IOPCIBridge.h>

// a pci-host-ecam-generic host bridge, everything it needs comes from its device tree node
class PDEcamPCI : public IOPCIBridge
{
    OSDeclareDefaultStructors(PDEcamPCI)

protected:
    // the config window, ecam here and the buses below the host
    IOMemoryMap *ecamMap;
    volatile UInt8 *ecamBase;
    UInt8 busFirst;
    UInt8 busLast;

    void readBusRange(IOService *provider);
    // maps the config space the provider's reg describes. false when there isn't any
    virtual bool mapConfigSpace(IOService *provider);
    // a config register's address, NULL when nothing answers there
    virtual volatile UInt8 *configAddress(IOPCIAddressSpace space, UInt8 offset) const;

private:
    IODeviceMemory *ioSpace;

    // host bridge node properties the interrupt routing reads
    OSData *intMap;
    UInt32 intMapMask[4];
    UInt32 addrCells;
    UInt32 intCells;

    void addWindows(IORegistryEntry *node);
    bool routeINTx(UInt32 bus, UInt32 device, UInt32 pin, UInt32 *phandle,
                   UInt32 *spec, UInt32 *specCells);
    static PDEcamPCI *hostFor(IOService *busProvider);
    static bool routeFromBus(IOService *busProvider, UInt32 device, UInt32 pin,
                             UInt32 *phandle, UInt32 *spec, UInt32 *specCells);
    static IOReturn setNubInterrupts(IOService *nub);

public:
    bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    bool configure(IOService *provider) APPLE_KEXT_OVERRIDE;
    void free(void) APPLE_KEXT_OVERRIDE;
    IODeviceMemory *ioDeviceMemory(void) APPLE_KEXT_OVERRIDE;

    IOReturn callPlatformFunction(const OSSymbol *functionName, bool waitForFunction,
                                  void *param1, void *param2,
                                  void *param3, void *param4) APPLE_KEXT_OVERRIDE;

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
