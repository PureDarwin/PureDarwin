#ifndef _PD_IOUSB_HID_KEYBOARD_H
#define _PD_IOUSB_HID_KEYBOARD_H

#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>

class IOUSBHIDKeyboard : public IOHIKeyboard
{
    OSDeclareDefaultStructors(IOUSBHIDKeyboard);

public:
    bool init(OSDictionary *dict = NULL) override;
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;
    bool willTerminate(IOService *provider, IOOptionBits options) override;
    bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) override;

    void setAlphaLockFeedback(bool state) override;
    const unsigned char *defaultKeymapOfLength(UInt32 *length) override;
    UInt32 maxKeyCodes() override;
    UInt32 deviceType() override;
    UInt32 interfaceID() override;

private:
    static void pollThread(void *arg, wait_result_t);
    void pollLoop();
    void handleReport(const UInt8 report[8]);
    void dispatchUSBUsage(UInt8 usage, bool down);
    void dispatchModifier(UInt8 bit, bool down);
    bool setBootProtocol();

    IOUSBInterface *fInterface;
    IOUSBPipe *fInterruptPipe;
    IOBufferMemoryDescriptor *fReportMem;
    volatile bool fRunning;
    bool fConsoleGrabbed;
    UInt8 fLastReport[8];
};

#endif
