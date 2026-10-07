#ifndef _PDSERIALHID_H
#define _PDSERIALHID_H

#include "IOHIDDevice.h"	// the family's own header, IOKit/hid/ pulls in DriverKit

// a keyboard and an absolute pointer fed by the serial console: host lines that start with SOH
// ("m X Y BUTTONS", "k MODS K1..K6") become HID reports, for boards with no USB to plug into
class PDSerialHID : public IOHIDDevice
{
	OSDeclareDefaultStructors(PDSerialHID);

public:
	virtual bool handleStart(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void handleStop(IOService *provider) APPLE_KEXT_OVERRIDE;

	virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const APPLE_KEXT_OVERRIDE;
	virtual OSString *newTransportString() const APPLE_KEXT_OVERRIDE;
	virtual OSString *newProductString() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newVendorIDNumber() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newProductIDNumber() const APPLE_KEXT_OVERRIDE;
	virtual OSNumber *newLocationIDNumber() const APPLE_KEXT_OVERRIDE;

private:
	static void lineHandler(void *ctx, const char *line);
	void sendReport(const uint8_t *bytes, size_t len);
};

#endif
