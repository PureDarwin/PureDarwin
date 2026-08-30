#include "IOUSBHIDMouse.h"
#include "PDHIDEventQueue.h"

#include <IOKit/IOLib.h>
#include <kern/thread.h>

#define super IOService
OSDefineMetaClassAndStructors(IOUSBHIDMouse, IOService);

enum {
    kUSBHIDReqTypeSet = 0x21,
    kUSBHIDReqSetIdle = 0x0A,
    kUSBHIDReqSetProtocol = 0x0B,
    kUSBHIDProtocolBoot = 0
};

bool IOUSBHIDMouse::init(OSDictionary *dict)
{
    if (!super::init(dict)) return false;
    fInterface = NULL;
    fInterruptPipe = NULL;
    fReportMem = NULL;
    fRunning = false;
    fLastButtons = 0;
    fMouseIndex = PDHIDAllocateMouseIndex();
    setName("IOUSBHIDMouse");
    setProperty("Transport", "USB");
    return true;
}

bool IOUSBHIDMouse::start(IOService *provider)
{
    IOUSBFindEndpointRequest request;

    fInterface = OSDynamicCast(IOUSBInterface, provider);
    if (!fInterface || !super::start(provider)) return false;

    if (!fInterface->open(this)) {
        IOLog("IOUSBHIDMouse: failed to open interface\n");
        return false;
    }

    bzero(&request, sizeof(request));
    request.type = kUSBInterrupt;
    request.direction = kUSBIn;
    fInterruptPipe = fInterface->FindNextPipe(NULL, &request, true);
    if (!fInterruptPipe) {
        IOLog("IOUSBHIDMouse: no interrupt-IN pipe\n");
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

    IOLog("IOUSBHIDMouse: using existing HID protocol\n");
    PDHIDPublishMouseDevice();

    fRunning = true;
    thread_t thread = THREAD_NULL;
    if (kernel_thread_start((thread_continue_t)&IOUSBHIDMouse::pollThread, this, &thread) != KERN_SUCCESS) {
        fRunning = false;
        IOLog("IOUSBHIDMouse: failed to start poll thread\n");
        return false;
    }
    thread_deallocate(thread);
    IOLog("IOUSBHIDMouse: started usbmouse%u\n", fMouseIndex);
    return true;
}

void IOUSBHIDMouse::stop(IOService *provider)
{
    fRunning = false;
    if (fInterface) fInterface->close(this);
    super::stop(provider);
}

void IOUSBHIDMouse::free()
{
    fRunning = false;
    if (fReportMem) { fReportMem->release(); fReportMem = NULL; }
    if (fInterruptPipe) { fInterruptPipe->release(); fInterruptPipe = NULL; }
    super::free();
}

bool IOUSBHIDMouse::setBootProtocol()
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

void IOUSBHIDMouse::pollThread(void *arg, wait_result_t)
{
    ((IOUSBHIDMouse *)arg)->pollLoop();
    thread_terminate(current_thread());
}

void IOUSBHIDMouse::pollLoop()
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

void IOUSBHIDMouse::handleReport(const UInt8 report[8])
{
    UInt8 buttons = report[0];
    SInt8 dx = (SInt8)report[1];
    SInt8 dy = (SInt8)report[2];
    SInt8 wheel = (SInt8)report[3];

    if (buttons != fLastButtons || dx || dy || wheel) {
        PDHIDPushMouseEvent(fMouseIndex, buttons, dx, dy, wheel);
        fLastButtons = buttons;
    }
}
