#ifndef _PDDESIGNWAREPCI_H
#define _PDDESIGNWAREPCI_H

#include "PDEcamPCI.h"

// a DesignWare root port. The port's own config space is its dbi (reg 0) and the one device
// below it answers in the config window (reg 1). Interrupt routing and windows are PDEcamPCI's.
class PDDesignWarePCI : public PDEcamPCI
{
    OSDeclareDefaultStructors(PDDesignWarePCI)

    IOMemoryMap *dbiMap;
    volatile UInt8 *dbiBase;
    IOByteCount dbiLength;
    UInt8 expressCap;

    bool linkUp(void) const;
    void logFirmwareState(void);

protected:
    bool mapConfigSpace(IOService *provider) APPLE_KEXT_OVERRIDE;
    volatile UInt8 *configAddress(IOPCIAddressSpace space, UInt8 offset) const APPLE_KEXT_OVERRIDE;

public:
    void free(void) APPLE_KEXT_OVERRIDE;
};

#endif
