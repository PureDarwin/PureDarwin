#ifndef PD_DESIGNWARE_I2C_H
#define PD_DESIGNWARE_I2C_H

#include "PDI2CController.h"

// A Synopsys DesignWare I2C master, as AMD (AMDI0010) and Intel LPSS ship it, polled
class PDDesignWareI2C : public PDI2CController {
    OSDeclareDefaultStructors(PDDesignWareI2C);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;

protected:
    virtual IOReturn transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx,
                                    UInt32 rxLen, UInt32 timeoutUs, IOOptionBits options) APPLE_KEXT_OVERRIDE;

private:
    UInt32 rd(UInt32 off) const { return *(volatile UInt32 *)(fRegs + off); }
    void wr(UInt32 off, UInt32 v) { *(volatile UInt32 *)(fRegs + off) = v; }
    bool setEnabled(bool on);
    void setupTiming(UInt32 hz);

    IOMemoryMap *fMap;
    volatile UInt8 *fRegs;
    UInt32 fTxDepth;
    UInt32 fRxDepth;
};

#endif
