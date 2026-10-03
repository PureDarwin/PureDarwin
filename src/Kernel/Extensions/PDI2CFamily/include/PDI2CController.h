#ifndef PD_I2C_CONTROLLER_H
#define PD_I2C_CONTROLLER_H

#include "libkern/c++/OSMetaClass.h"
#include <IOKit/IOService.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOLocks.h>

class PDI2CDevice;

// transfer() options
enum {
    kPDI2CTransferQuiet = 1u << 0, // a failure is expected (probing, polling)
};

// An I2C host controller driver subclasses this, implements transferLocked(),
// and calls publishDevices() with its device-tree node once the bus works. PDI2CDevice nubs are
// created by client drivers which match on compat (hid-over-i2c, ...)
class PDI2CController : public IOService {
    OSDeclareAbstractStructors(PDI2CController);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;
    virtual IOWorkLoop *getWorkLoop(void) const APPLE_KEXT_OVERRIDE;

    // write tx, then read rx after a repeated START. either may be empty. addr is 7-bit
    IOReturn transfer(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                      IOOptionBits options = 0);

protected:
    // one transfer called with the bus lock held. timeoutUs bounds each phase
    virtual IOReturn transferLocked(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx,
                                    UInt32 rxLen, UInt32 timeoutUs, IOOptionBits options) = 0;

    // a nub for every child of node that has a reg
    UInt32 publishDevices(IORegistryEntry *node);

private:
    IOWorkLoop *fWorkLoop;
    IOLock *fBusLock;
};

// the properties of some device on an I2C bus and a way to talk to it
class PDI2CDevice : public IOService {
    OSDeclareDefaultStructors(PDI2CDevice);

public:
    static PDI2CDevice *withNode(IORegistryEntry *node, UInt8 addr);

    virtual bool compareName(OSString *name, OSString **matched = NULL) const APPLE_KEXT_OVERRIDE;

    UInt8 address(void) const { return fAddr; }
    IOReturn transfer(const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                      IOOptionBits options = 0);

private:
    UInt8 fAddr;
};

#endif
