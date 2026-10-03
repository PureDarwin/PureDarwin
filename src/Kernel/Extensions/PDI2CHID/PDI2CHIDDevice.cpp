// HID over I2C, from the Microsoft HID over I2C Protocol Specification 1.0
#include "PDI2CHIDDevice.h"
#include "IOKit/IOReturn.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <pexpert/pexpert.h>

#define super IOHIDDevice
OSDefineMetaClassAndStructors(PDI2CHIDDevice, IOHIDDevice);

#define HID_DESC_LENGTH         30
#define HID_OP_RESET            0x01
#define HID_OP_GET_REPORT       0x02
#define HID_OP_SET_REPORT       0x03
#define HID_OP_SET_POWER        0x08
#define HID_POWER_ON            0x00
#define POLL_INTERVAL_MS        8       // 125 Hz
#define REPORTS_LOGGED          8       // then quiet. the console may be the only readout
#define DRAIN_MAX               8       // input reads per interrupt
#define CONTROLLER_WAIT_NS      (5ULL * 1000 * 1000 * 1000)

// Wait for IOInterruptControllers with a deadline (CONTROLLER_WAIT_NS)
static bool
interruptControllerReady(IOService *provider)
{
    OSArray *names = OSDynamicCast(OSArray, provider->getProperty("IOInterruptControllers"));
    OSString *name = names != NULL ? OSDynamicCast(OSString, names->getObject(0)) : NULL;
    OSDictionary *match;
    IOService *found = NULL;

    if (name == NULL) {
        return false;
    }
    match = IOService::resourceMatching(name);
    if (match != NULL) {
        found = IOService::waitForMatchingService(match, CONTROLLER_WAIT_NS);
        match->release();
    }
    if (found == NULL) {
        IOLog("PDI2CHID: interrupt controller %s never appeared\n", name->getCStringNoCopy());
        return false;
    }
    found->release();
    return true;
}

static UInt32
nodeU32(IORegistryEntry *entry, const char *key, UInt32 dflt)
{
    OSData *d = OSDynamicCast(OSData, entry->getProperty(key));
    return (d != NULL && d->getLength() >= sizeof(UInt32)) ? *(const UInt32 *)d->getBytesNoCopy() : dflt;
}

IOReturn
PDI2CHIDDevice::command(UInt8 opcode, UInt8 arg)
{
    UInt8 cmd[4] = { (UInt8)fCommandRegister, (UInt8)(fCommandRegister >> 8), arg, opcode };
    return fBus->transfer(cmd, sizeof(cmd), NULL, 0);
}

// Runs before IOHIDDevice::start() asks for the report descriptor. Read both then power
// the device on and reset it
bool
PDI2CHIDDevice::handleStart(IOService *provider)
{
    UInt8 d[HID_DESC_LENGTH], reg[2];
    UInt16 descRegister, rdescLength, rdescRegister;
    IOReturn r;

    if (!super::handleStart(provider)) {
        return false;
    }
    fBus = OSDynamicCast(PDI2CDevice, provider);
    if (fBus == NULL) {
        return false;
    }

    descRegister = (UInt16)nodeU32(fBus, "hid-descr-addr", 1);
    reg[0] = (UInt8)descRegister;
    reg[1] = (UInt8)(descRegister >> 8);
    r = fBus->transfer(reg, sizeof(reg), d, sizeof(d), kPDI2CTransferQuiet);
    if (r == kIOReturnNotResponding) {
        // a declared device that is not fitted
        return false;
    }
    if (r != kIOReturnSuccess) {
        IOLog("PDI2CHID: 0x%02x HID descriptor read failed (0x%x)\n", fBus->address(), r);
        return false;
    }

#define LE16(i) ((UInt16)(d[(i)] | (d[(i) + 1] << 8)))
    rdescLength = LE16(4);
    rdescRegister = LE16(6);
    fMaxInput = LE16(10);
    fCommandRegister = LE16(16);
    fDataRegister = LE16(18);
    fVendorID = LE16(20);
    fProductID = LE16(22);
    fVersionID = LE16(24);
    // something that ACKs and returns anything else is not an I2C-HID device
    if (LE16(0) != HID_DESC_LENGTH || rdescLength == 0 || rdescLength > kMaxReportDescriptor) {
        return false;
    }
#undef LE16

    UInt8 *buf = (UInt8 *)IOMalloc(rdescLength);
    if (buf == NULL) {
        return false;
    }
    reg[0] = (UInt8)rdescRegister;
    reg[1] = (UInt8)(rdescRegister >> 8);
    r = fBus->transfer(reg, sizeof(reg), buf, rdescLength);
    if (r == kIOReturnSuccess) {
        fDescriptor = OSData::withBytes(buf, rdescLength);
    }
    IOFree(buf, rdescLength);
    if (fDescriptor == NULL) {
        IOLog("PDI2CHID: 0x%02x report descriptor read failed (0x%x)\n", fBus->address(), r);
        return false;
    }

    // the end of the reset is a zero-length input report, skipped like any idle read
    IOReturn rp = command(HID_OP_SET_POWER, HID_POWER_ON);
    IOSleep(2);
    IOReturn rr = command(HID_OP_RESET, 0);
    IOSleep(20);
    if (rp != kIOReturnSuccess || rr != kIOReturnSuccess) {
        IOLog("PDI2CHID: 0x%02x power/reset 0x%x/0x%x\n", fBus->address(), rp, rr);
    }

    // IOHIDEventDriver personalities only match transports that publish this
    setProperty("HIDDefaultBehavior", kOSBooleanTrue);
    return true;
}

// input starts once IOHIDDevice is up, so no report races the parse of the descriptor
bool
PDI2CHIDDevice::start(IOService *provider)
{
    IOWorkLoop *wl;
    UInt32 forcePoll = 0;

    if (!super::start(provider)) {
        if (fDescriptor != NULL) {
            IOLog("PDI2CHID: 0x%02x IOHIDDevice start failed\n", fBus->address());
        }
        return false;
    }
    wl = getWorkLoop();
    if (wl == NULL) {
        return false;
    }
    PE_parse_boot_argn("pdi2chid_poll", &forcePoll, sizeof(forcePoll));

    // read when the device pulls its interrupt line. The nub has one
    // only when its node has interrupts and an interrupt controller resolves them
    if (!forcePoll && interruptControllerReady(provider)) {
        fInterrupt = IOInterruptEventSource::interruptEventSource(this, &PDI2CHIDDevice::interruptAction,
                                                                  provider, 0);
    }
    if (fInterrupt != NULL && wl->addEventSource(fInterrupt) == kIOReturnSuccess) {
        fInterrupt->enable();
        return true;
    }
    OSSafeReleaseNULL(fInterrupt);

    // Fallback to polling only works if the device answers an idle read with a zero-length
    // report, which the spec does not promise (observed x13s elan devices do)
    fTimer = IOTimerEventSource::timerEventSource(this, &PDI2CHIDDevice::timerAction);
    if (fTimer == NULL || wl->addEventSource(fTimer) != kIOReturnSuccess) {
        OSSafeReleaseNULL(fTimer);
        return false;
    }
    fTimer->setTimeoutMS(POLL_INTERVAL_MS);
    IOLog("PDI2CHID: Interrupt setup failed. Fallback to polling every %u ms\n", POLL_INTERVAL_MS);
    return true;
}

void
PDI2CHIDDevice::stop(IOService *provider)
{
    IOWorkLoop *wl = getWorkLoop();

    if (fTimer != NULL) {
        fTimer->cancelTimeout();
        if (wl != NULL) {
            wl->removeEventSource(fTimer);
        }
    }
    if (fInterrupt != NULL) {
        fInterrupt->disable();
        if (wl != NULL) {
            wl->removeEventSource(fInterrupt);
        }
    }
    super::stop(provider);
}

void PDI2CHIDDevice::free(void)
{
    OSSafeReleaseNULL(fTimer);
    OSSafeReleaseNULL(fInterrupt);
    OSSafeReleaseNULL(fDescriptor);
    super::free();
}

// the device answers {length lo, length hi, report}.
// A length of 2 or less is nothing pending. Returns whether a report came in.
bool
PDI2CHIDDevice::readInput(bool polled)
{
    UInt32 want = fMaxInput, n;

    if (want < 2 || want > sizeof(fBuffer)) {
        want = sizeof(fBuffer);
    }
    if (fBus->transfer(NULL, 0, fBuffer, want, kPDI2CTransferQuiet) != kIOReturnSuccess) {
        return false;
    }
    n = (UInt32)(fBuffer[0] | (fBuffer[1] << 8));
    if (n <= 2) {
        return false;
    }
    if (n > want) {
        n = want;
    }
    // when polled the same report again is the device repeating itself
    if (polled && n == fLastLength && memcmp(fBuffer, fLast, n) == 0) {
        return false;
    }
    memcpy(fLast, fBuffer, n);
    fLastLength = n;

    // report ID first with 2 byte length stripped
    IOBufferMemoryDescriptor *md = IOBufferMemoryDescriptor::withBytes(fBuffer + 2, n - 2, kIODirectionNone);
    if (md != NULL) {
        handleReport(md, kIOHIDReportTypeInput);
        md->release();
    }
    return true;
}

void
PDI2CHIDDevice::interruptAction(OSObject *owner, IOInterruptEventSource *sender, int count)
{
    PDI2CHIDDevice *self = (PDI2CHIDDevice *)owner;

    (void)sender;
    (void)count;
    // a level interrupt stays asserted while reports are pending
    for (int i = 0; i < DRAIN_MAX && self->readInput(false); i++) {
    }
}

void
PDI2CHIDDevice::timerAction(OSObject *owner, IOTimerEventSource *sender)
{
    PDI2CHIDDevice *self = OSDynamicCast(PDI2CHIDDevice, owner);
    self->readInput(true);
    sender->setTimeoutMS(POLL_INTERVAL_MS);
}

IOReturn
PDI2CHIDDevice::newReportDescriptor(IOMemoryDescriptor **descriptor) const
{
    IOBufferMemoryDescriptor *md;

    if (fDescriptor == NULL) {
        return kIOReturnNotReady;
    }
    md = IOBufferMemoryDescriptor::withBytes(fDescriptor->getBytesNoCopy(), fDescriptor->getLength(),
                                             kIODirectionNone);
    if (md == NULL) {
        return kIOReturnNoMemory;
    }
    *descriptor = md;
    return kIOReturnSuccess;
}

OSString *
PDI2CHIDDevice::newTransportString(void) const
{
    return OSString::withCString("I2C");
}

OSNumber *
PDI2CHIDDevice::newVendorIDNumber(void) const
{
    return OSNumber::withNumber(fVendorID, 32);
}

OSNumber *
PDI2CHIDDevice::newProductIDNumber(void) const
{
    return OSNumber::withNumber(fProductID, 32);
}

OSNumber *
PDI2CHIDDevice::newVersionNumber(void) const
{
    return OSNumber::withNumber(fVersionID, 32);
}

// unique per device on a bus. one bus per machine is all there is so far
OSNumber *
PDI2CHIDDevice::newLocationIDNumber(void) const
{
    return OSNumber::withNumber(0x12c00000u | (fBus != NULL ? fBus->address() : 0), 32);
}

UInt32
PDI2CHIDDevice::reportCommand(UInt8 *buf, UInt8 opcode, IOHIDReportType type, UInt8 id) const
{
    UInt32 n = 0;

    buf[n++] = (UInt8)fCommandRegister;
    buf[n++] = (UInt8)(fCommandRegister >> 8);
    buf[n++] = (UInt8)(((type + 1) << 4) | (id < 15 ? id : 15));
    buf[n++] = opcode;
    if (id >= 15) {
        buf[n++] = id;
    }
    buf[n++] = (UInt8)fDataRegister;
    buf[n++] = (UInt8)(fDataRegister >> 8);
    return n;
}

IOReturn
PDI2CHIDDevice::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    UInt8 cmd[8];
    UInt8 id = (UInt8)(options & 0xff);
    IOByteCount want;
    UInt8 *buf;
    IOReturn r;

    if (fBus == NULL || report == NULL || reportType > kIOHIDReportTypeFeature) {
        return kIOReturnBadArgument;
    }
    want = report->getLength();
    if (want == 0 || want > kMaxReportDescriptor) {
        return kIOReturnBadArgument;
    }
    buf = (UInt8 *)IOMalloc(want + 2);
    if (buf == NULL) {
        return kIOReturnNoMemory;
    }
    r = fBus->transfer(cmd, reportCommand(cmd, HID_OP_GET_REPORT, reportType, id), buf, (UInt32)want + 2);
    if (r == kIOReturnSuccess) {
        // the length counts itself. after that is the report, ID first when it has one
        UInt32 n = (UInt32)(buf[0] | (buf[1] << 8));
        n = n > 2 ? n - 2 : 0;
        if (n > want) {
            n = (UInt32)want;
        }
        report->writeBytes(0, buf + 2, n);
    }
    IOFree(buf, want + 2);
    return r;
}

IOReturn
PDI2CHIDDevice::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    UInt8 buf[32];
    UInt8 id = (UInt8)(options & 0xff);
    IOByteCount len;
    UInt32 n;

    if (fBus == NULL || report == NULL || reportType > kIOHIDReportTypeFeature) {
        return kIOReturnBadArgument;
    }
    len = report->getLength();
    n = reportCommand(buf, HID_OP_SET_REPORT, reportType, id);
    // one write of command, data register, length and report
    if (len == 0 || n + 2 + len > sizeof(buf)) {
        return kIOReturnUnsupported;
    }
    buf[n++] = (UInt8)(len + 2);
    buf[n++] = (UInt8)((len + 2) >> 8);
    report->readBytes(0, buf + n, len);
    n += (UInt32)len;
    return fBus->transfer(buf, n, NULL, 0);
}
