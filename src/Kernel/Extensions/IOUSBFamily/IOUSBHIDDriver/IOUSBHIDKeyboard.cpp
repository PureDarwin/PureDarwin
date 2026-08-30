#include "IOUSBHIDKeyboard.h"
#include "PDHIDEventQueue.h"

#include <IOKit/IOLib.h>
#include <IOKit/hidsystem/IOHIDParameter.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <kern/thread.h>
#include "PDHIDKeyboardMap.h"

#define super IOHIKeyboard
OSDefineMetaClassAndStructors(IOUSBHIDKeyboard, IOHIKeyboard);

/* ADB translation lives in PDHIDEventQueue, shared with the PS/2 driver. */
#define DEADKEY kPDHIDNoADBCode

enum {
    kUSBHIDReqTypeSet = 0x21,
    kUSBHIDReqSetIdle = 0x0A,
    kUSBHIDReqSetProtocol = 0x0B,
    kUSBHIDProtocolBoot = 0
};

bool IOUSBHIDKeyboard::init(OSDictionary *dict)
{
    if (!super::init(dict)) return false;
    fInterface = NULL;
    fInterruptPipe = NULL;
    fReportMem = NULL;
    fRunning = false;
    fConsoleGrabbed = false;
    bzero(fLastReport, sizeof(fLastReport));
    setName("IOUSBHIDKeyboard");
    setProperty("HIDKeyboardKeysDefined", kOSBooleanTrue);
    setProperty(kIOHIDVirtualHIDevice, kOSBooleanFalse);
    setProperty(kIOHIDKindKey, kHIKeyboardDevice, 32);
    setProperty(kIOHIDInterfaceIDKey, NX_EVS_DEVICE_INTERFACE_ADB, 32);
    setProperty(kIOHIDSubinterfaceIDKey, NX_EVS_DEVICE_TYPE_KEYBOARD, 32);
    setProperty("Transport", "USB");
    setProperty("USB Product Name", "USB HID Keyboard");
    return true;
}

bool IOUSBHIDKeyboard::start(IOService *provider)
{
    IOUSBFindEndpointRequest request;

    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if (!fInterface || !super::start(provider)) return false;

    if (!fInterface->open(this)) {
        IOLog("IOUSBHIDKeyboard: failed to open interface\n");
        return false;
    }

    bzero(&request, sizeof(request));
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    fInterruptPipe = fInterface->FindNextPipe(NULL, &request, true);
    if (!fInterruptPipe) {
        IOLog("IOUSBHIDKeyboard: no interrupt-IN pipe\n");
        fInterface->close(this);
        return false;
    }

    fReportMem = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, kIODirectionInOut, 8);
    if (!fReportMem) {
        fInterruptPipe->release();
        fInterruptPipe = NULL;
        fInterface->close(this);
        return false;
    }

    IOLog("IOUSBHIDKeyboard: using existing HID protocol\n");
    PDHIDPublishKeyboardDevice();

    fRunning = true;
    thread_t thread = THREAD_NULL;
    if (kernel_thread_start((thread_continue_t)&IOUSBHIDKeyboard::pollThread, this, &thread) != KERN_SUCCESS) {
        fRunning = false;
        IOLog("IOUSBHIDKeyboard: failed to start poll thread\n");
        return false;
    }
    thread_deallocate(thread);
    IOLog("IOUSBHIDKeyboard: started\n");
    return true;
}

void IOUSBHIDKeyboard::stop(IOService *provider)
{
    fRunning = false;
    if (fInterface) fInterface->close(this);
    super::stop(provider);
}

void IOUSBHIDKeyboard::free()
{
    fRunning = false;
    if (fReportMem) { fReportMem->release(); fReportMem = NULL; }
    if (fInterruptPipe) { fInterruptPipe->release(); fInterruptPipe = NULL; }
    super::free();
}

bool IOUSBHIDKeyboard::setBootProtocol()
{
    IOUSBDevRequest req;
    bzero(&req, sizeof(req));
    req.bmRequestType = kUSBHIDReqTypeSet;
    req.bRequest = kUSBHIDReqSetProtocol;
    req.wValue = kUSBHIDProtocolBoot;
    req.wIndex = fInterface->GetInterfaceNumber();
    req.wLength = 0;
    fInterface->DeviceRequest(&req);

    bzero(&req, sizeof(req));
    req.bmRequestType = kUSBHIDReqTypeSet;
    req.bRequest = kUSBHIDReqSetIdle;
    req.wValue = 0;
    req.wIndex = fInterface->GetInterfaceNumber();
    req.wLength = 0;
    fInterface->DeviceRequest(&req);
    return true;
}

void IOUSBHIDKeyboard::pollThread(void *arg, wait_result_t)
{
    ((IOUSBHIDKeyboard *)arg)->pollLoop();
    thread_terminate(current_thread());
}

void IOUSBHIDKeyboard::pollLoop()
{
    while (fRunning) {
        IOByteCount bytesRead = 8;
        bzero(fReportMem->getBytesNoCopy(), 8);
        IOReturn ret = fInterruptPipe->Read(fReportMem, 0, 0, 8,
                                            (IOUSBCompletion *)NULL, &bytesRead);
        if (ret == kIOReturnSuccess && bytesRead > 0) {
            UInt8 report[8];
            bzero(report, sizeof(report));
            bcopy(fReportMem->getBytesNoCopy(), report, bytesRead > 8 ? 8 : bytesRead);
            handleReport(report);
        } else {
            IOSleep(10);
        }
    }
}

static bool reportHasUsage(const UInt8 report[8], UInt8 usage)
{
    for (int i = 2; i < 8; i++)
        if (report[i] == usage) return true;
    return false;
}

void IOUSBHIDKeyboard::handleReport(const UInt8 report[8])
{
    bool grabbed = PDHIDKeyboardIsGrabbed();
    if (grabbed && !fConsoleGrabbed) {
        /* Anything held down when the grab started has already been reported
         * to the console; without a matching release it stays latched there
         * for the lifetime of the compositor. */
        AbsoluteTime now;
        clock_get_uptime((uint64_t *)&now);
        for (UInt8 bit = 0; bit < 8; bit++)
            if (fLastReport[0] & (1U << bit))
                dispatchKeyboardEvent(PDHIDUsageToADB((UInt8)(0xE0 + bit)), false, now);
        for (int i = 2; i < 8; i++) {
            UInt8 usage = fLastReport[i];
            if (usage >= 4 && usage <= 0x75 && PDHIDUsageToADB(usage) != DEADKEY)
                dispatchKeyboardEvent(PDHIDUsageToADB(usage), false, now);
        }
    }
    fConsoleGrabbed = grabbed;

    if (report[2] == 1) {
        bcopy(report, fLastReport, sizeof(fLastReport));
        return;
    }

    UInt8 changedMods = report[0] ^ fLastReport[0];
    for (UInt8 bit = 0; bit < 8; bit++)
        if (changedMods & (1U << bit))
            dispatchModifier(bit, !!(report[0] & (1U << bit)));

    for (int i = 2; i < 8; i++) {
        UInt8 usage = fLastReport[i];
        if (usage >= 4 && !reportHasUsage(report, usage))
            dispatchUSBUsage(usage, false);
    }
    for (int i = 2; i < 8; i++) {
        UInt8 usage = report[i];
        if (usage >= 4 && !reportHasUsage(fLastReport, usage))
            dispatchUSBUsage(usage, true);
    }
    bcopy(report, fLastReport, sizeof(fLastReport));
}

void IOUSBHIDKeyboard::dispatchUSBUsage(UInt8 usage, bool down)
{
    if (usage > 0x75) return;
    PDHIDPushKeyboardEvent(usage, down);

    if (fConsoleGrabbed) return;

    UInt8 adb = PDHIDUsageToADB(usage);
    if (adb == DEADKEY) return;

    AbsoluteTime now;
    clock_get_uptime((uint64_t *)&now);
    dispatchKeyboardEvent(adb, down, now);
}

void IOUSBHIDKeyboard::dispatchModifier(UInt8 bit, bool down)
{
    if (bit >= 8) return;
    PDHIDPushKeyboardEvent((UInt8)(0xE0 + bit), down);

    if (fConsoleGrabbed) return;

    AbsoluteTime now;
    clock_get_uptime((uint64_t *)&now);
    dispatchKeyboardEvent(PDHIDUsageToADB((UInt8)(0xE0 + bit)), down, now);
}

void IOUSBHIDKeyboard::setAlphaLockFeedback(bool)
{
}

const unsigned char *IOUSBHIDKeyboard::defaultKeymapOfLength(UInt32 *length)
{
    *length = sizeof(gPDHIDUSAKeyMap);
    return gPDHIDUSAKeyMap;
}

UInt32 IOUSBHIDKeyboard::maxKeyCodes()
{
    return NX_NUMKEYCODES;
}

UInt32 IOUSBHIDKeyboard::deviceType()
{
    return NX_EVS_DEVICE_TYPE_KEYBOARD;
}

UInt32 IOUSBHIDKeyboard::interfaceID()
{
    return NX_EVS_DEVICE_INTERFACE_ADB;
}
