#ifndef PD_I2C_HID_DEVICE_H
#define PD_I2C_HID_DEVICE_H

#include "IOHIDDevice.h"
#include "PDI2CController.h"

#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>

// A hid-over-i2c device (Microsoft HID over I2C Protocol Specification 1.0) on any
// PDI2CController bus. Matches a PDI2CDevice whose node is compatible with hid-over-i2c
// and reads hid-descr-addr from it. Input is interrupt-driven when the nub has an
// interrupt and polled otherwise (or if pdi2chid_poll boot-arg is set)
class PDI2CHIDDevice : public IOHIDDevice {
    OSDeclareDefaultStructors(PDI2CHIDDevice);

public:
    virtual bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free(void) APPLE_KEXT_OVERRIDE;
    virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const APPLE_KEXT_OVERRIDE;
    virtual OSString *newTransportString(void) const APPLE_KEXT_OVERRIDE;
    virtual OSNumber *newVendorIDNumber(void) const APPLE_KEXT_OVERRIDE;
    virtual OSNumber *newProductIDNumber(void) const APPLE_KEXT_OVERRIDE;
    virtual OSNumber *newVersionNumber(void) const APPLE_KEXT_OVERRIDE;
    virtual OSNumber *newLocationIDNumber(void) const APPLE_KEXT_OVERRIDE;
    virtual IOReturn getReport(IOMemoryDescriptor *report, IOHIDReportType reportType,
                               IOOptionBits options) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setReport(IOMemoryDescriptor *report, IOHIDReportType reportType,
                               IOOptionBits options) APPLE_KEXT_OVERRIDE;

private:
    enum { kMaxReport = 256, kMaxReportDescriptor = 4096 };

    IOReturn command(UInt8 opcode, UInt8 arg);
    UInt32 reportCommand(UInt8 *buf, UInt8 opcode, IOHIDReportType type, UInt8 id) const;
    bool readInput(bool polled);
    static void interruptAction(OSObject *owner, IOInterruptEventSource *sender, int count);
    static void timerAction(OSObject *owner, IOTimerEventSource *sender);

    PDI2CDevice *fBus;
    OSData *fDescriptor;
    IOInterruptEventSource *fInterrupt;
    IOTimerEventSource *fTimer;
    UInt16 fCommandRegister, fDataRegister, fMaxInput;
    UInt16 fVendorID, fProductID, fVersionID;
    UInt32 fReports, fLastLength;
    UInt8 fBuffer[kMaxReport], fLast[kMaxReport];
};

#endif
