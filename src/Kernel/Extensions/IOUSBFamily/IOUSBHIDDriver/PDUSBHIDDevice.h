#ifndef _PDUSBHIDDEVICE_H
#define _PDUSBHIDDEVICE_H

#include "IOHIDDevice.h"	// the family's own header, IOKit/hid/ pulls in DriverKit
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

// Any USB HID interface as an IOHIDDevice, IOHIDFamily builds the elements and IOHIDEventDriver
// turns its reports into events, so keyboards, mice and QEMU's usb-tablet need nothing specific
class PDUSBHIDDevice : public IOHIDDevice
{
	OSDeclareDefaultStructors(PDUSBHIDDevice);

public:
	virtual bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void handleStop(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;
	virtual bool willTerminate(IOService *provider, IOOptionBits options) APPLE_KEXT_OVERRIDE;
	virtual bool didTerminate(IOService *provider, IOOptionBits options, bool *defer) APPLE_KEXT_OVERRIDE;

	virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const APPLE_KEXT_OVERRIDE;
	virtual OSString *newTransportString() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newProductIDNumber() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newLocationIDNumber() const APPLE_KEXT_OVERRIDE;

	virtual IOReturn getReport(IOMemoryDescriptor *report, IOHIDReportType reportType,
	    IOOptionBits options) APPLE_KEXT_OVERRIDE;
	virtual IOReturn setReport(IOMemoryDescriptor *report, IOHIDReportType reportType,
	    IOOptionBits options) APPLE_KEXT_OVERRIDE;

private:
	static void pollThread(void *arg, wait_result_t wr);
	void pollLoop(void);
	IOReturn classRequest(bool in, UInt8 request, UInt16 value, void *data, UInt16 length, UInt32 *done);

	IOUSBInterface *fInterface;
	IOUSBPipe *fInPipe;
	OSData *fDescriptor;
	IOBufferMemoryDescriptor *fReadBuffer;
	UInt16 fMaxPacket;
	volatile bool fRunning, fPolling;
};

#endif
