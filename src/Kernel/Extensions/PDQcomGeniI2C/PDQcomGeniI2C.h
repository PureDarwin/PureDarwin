#ifndef PD_QCOM_GENI_I2C_H
#define PD_QCOM_GENI_I2C_H

#include "PDI2CController.h"

#include <IOKit/IOCommandGate.h>
#include <IOKit/IOInterruptEventSource.h>

// A Qualcomm GENI serial engine running the I2C protocol
// Interrupt driven when its node has an interrupt otherwise polled
class PDQcomGeniI2C : public PDI2CController {
    OSDeclareDefaultStructors(PDQcomGeniI2C);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;

protected:
    virtual IOReturn transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx,
                                    UInt32 rxLen, UInt32 timeoutUs, IOOptionBits options) APPLE_KEXT_OVERRIDE;

private:
    struct Transfer {
        UInt8 addr;
        const UInt8 *tx;
        UInt32 txLen;
        UInt8 *rx;
        UInt32 rxLen;
        UInt32 timeoutUs;
        IOOptionBits options;
    };

    bool setupInterrupt(IOService *provider);
    void teardownInterrupt(void);
    void interruptOccurred(IOInterruptEventSource *sender, int count);
    IOReturn gatedTransfer(void *arg0, void *arg1, void *arg2, void *arg3);
    IOReturn runTransfer(const Transfer *t);

    void armEvents(void);
    UInt32 waitEvents(UInt32 timeoutUs);
    UInt32 pollEvents(UInt32 timeoutUs);
    UInt32 sleepEvents(UInt32 timeoutUs);
    void drainRx(void);
    void abortCommand(void);

    IOMemoryMap *fMap;
    volatile UInt8 *fRegs;

    // the engines work loop. NULL when polled
    IOWorkLoop *fEngineLoop;
    IOInterruptEventSource *fInterrupt;
    IOCommandGate *fGate;

    UInt8 *fRx;
    UInt32 fRxLen, fRxGot;
    UInt32 fEvents;
    UInt32 fMissed;
};

#endif

