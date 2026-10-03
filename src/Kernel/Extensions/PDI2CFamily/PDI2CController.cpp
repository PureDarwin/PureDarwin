#include "PDI2CController.h"
#include "IOKit/IOReturn.h"
#include "libkern/OSTypes.h"
#include "libkern/c++/OSMetaClass.h"

#include <IOKit/IOLib.h>
#include <IOKit/IODeviceTreeSupport.h>

OSDefineMetaClassAndAbstractStructors(PDI2CController, IOService);
OSDefineMetaClassAndStructors(PDI2CDevice, IOService);

bool
PDI2CController::start(IOService *provider)
{
    if (!IOService::start(provider)) {
        return false;
    }
    fBusLock = IOLockAlloc();
    fWorkLoop = IOWorkLoop::workLoop();
    return fBusLock != NULL && fWorkLoop != NULL;
}

void
PDI2CController::free(void)
{
    OSSafeReleaseNULL(fWorkLoop);
    if (fBusLock != NULL) {
        IOLockFree(fBusLock);
        fBusLock = NULL;
    }
    IOService::free();
}

IOWorkLoop *
PDI2CController::getWorkLoop(void) const
{
    return fWorkLoop;
}

IOReturn
PDI2CController::transfer(UInt8 addr, const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen,
                          IOOptionBits options)
{
    UInt32 timeoutUs = 10000 + 50 * (txLen > rxLen ? txLen : rxLen);
    IOReturn ret;

    if (fBusLock == NULL) {
        return kIOReturnNotReady;
    }
    IOLockLock(fBusLock);
    ret = transferLocked(addr, tx, txLen, rx, rxLen, timeoutUs, options);
    IOLockUnlock(fBusLock);
    return ret;
}

UInt32
PDI2CController::publishDevices(IORegistryEntry *node)
{
    OSIterator *iter = node->getChildIterator(gIODTPlane);
    IORegistryEntry *child;
    UInt32 count = 0;

    while (iter != NULL && (child = (IORegistryEntry *)iter->getNextObject()) != NULL) {
        OSData *reg = OSDynamicCast(OSData, child->getProperty("reg"));
        OSData *status = OSDynamicCast(OSData, child->getProperty("status"));

        if (reg == NULL || reg->getLength() < sizeof(UInt32)) {
            continue;
        }
        if (status != NULL && strncmp((const char *)status->getBytesNoCopy(), "okay", status->getLength()) != 0 &&
            strncmp((const char *)status->getBytesNoCopy(), "ok", status->getLength()) != 0) {
            continue;
        }
        UInt32 raw = *(const UInt32 *)reg->getBytesNoCopy();
        // the i2c binding's reg is a 7-bit address
        if (raw > 0x7f) {
            IOLog("PDI2CFamily: %s reg 0x%x is not a 7-bit address, skipped\n",
                  child->getName(gIODTPlane), raw);
            continue;
        }
        UInt8 addr = (UInt8)raw;
        PDI2CDevice *dev = PDI2CDevice::withNode(child, addr);
        if (dev == NULL) {
            continue;
        }
        if (dev->attach(this)) {
            dev->registerService();
            count++;
        }
        dev->release();
    }
    OSSafeReleaseNULL(iter);
    return count;
}

PDI2CDevice *
PDI2CDevice::withNode(IORegistryEntry *node, UInt8 addr)
{
    PDI2CDevice *dev = OSTypeAlloc(PDI2CDevice);
    OSDictionary *props = node->dictionaryWithProperties();
    char location[4];

    if (dev == NULL || props == NULL || !dev->init(props)) {
        OSSafeReleaseNULL(dev);
        OSSafeReleaseNULL(props);
        return NULL;
    }
    props->release();
    dev->fAddr = addr;
    dev->setName(node->getName(gIODTPlane));
    snprintf(location, sizeof(location), "%x", addr);
    dev->setLocation(location);
    return dev;
}

bool
PDI2CDevice::compareName(OSString *name, OSString **matched) const
{
    return IODTCompareNubName(this, name, matched) || IOService::compareName(name, matched);
}

IOReturn
PDI2CDevice::transfer(const UInt8 *tx, UInt32 txLen, UInt8 *rx, UInt32 rxLen, IOOptionBits options)
{
    PDI2CController *bus = OSDynamicCast(PDI2CController, getProvider());
    return bus != NULL ? bus->transfer(fAddr, tx, txLen, rx, rxLen, options) : kIOReturnNotAttached;
}
