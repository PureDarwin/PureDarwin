#ifndef PD_QCOM_GENI_I2C_H
#define PD_QCOM_GENI_I2C_H

#include "PDI2CController.h"

// A Qualcomm GENI serial engine running the I2C protocol
class PDQcomGeniI2C : public PDI2CController {
    OSDeclareDefaultStructors(PDQcomGeniI2C);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;

protected:
    virtual IOReturn transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx,
                                    UInt32 rxLen, UInt32 timeoutUs, IOOptionBits options) APPLE_KEXT_OVERRIDE;

private:
    void abortCommand(void);
    UInt32 waitTerminal(UInt32 timeoutUs);

    IOMemoryMap *fMap;
    volatile UInt8 *fRegs;
};

#endif

