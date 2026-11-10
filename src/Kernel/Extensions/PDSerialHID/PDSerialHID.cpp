#include "PDSerialHID.h"
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <libkern/libkern.h>
#include "PDHIDEventQueue.h"

extern "C" void pd_serial_line_register(void (*handler)(void *ctx, const char *line), void *ctx);

#define super IOHIDDevice
OSDefineMetaClassAndStructors(PDSerialHID, IOHIDDevice);

// report 1 is a boot keyboard (modifiers, reserved, six keys), report 2 a boot mouse with three
// buttons and relative X/Y, since the /dev/usb_hid_mouse queue only carries motion
static const uint8_t kPDSerialHIDDescriptor[] = {
	0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01,
	0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,
	0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
	0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65, 0x05, 0x07, 0x19, 0x00, 0x29, 0x65, 0x81, 0x00,
	0xc0,
	0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xa1, 0x00,
	0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03, 0x75, 0x01, 0x81, 0x02,
	0x95, 0x01, 0x75, 0x05, 0x81, 0x01,
	0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08, 0x95, 0x02, 0x81, 0x06,
	0xc0, 0xc0,
};

bool
PDSerialHID::handleStart(IOService *provider)
{
	if (!super::handleStart(provider)) return false;
	// IOHIDEventDriver personalities only match transports that publish this
	setProperty("HIDDefaultBehavior", kOSBooleanTrue);
	pd_serial_line_register(lineHandler, this);
	PDHIDPublishKeyboardDevice();
	PDHIDPublishMouseDevice();
	IOLog("PDSerialHID: listening for SOH lines on the serial console\n");
	return true;
}

void
PDSerialHID::handleStop(IOService *provider)
{
	pd_serial_line_register(NULL, NULL);
	super::handleStop(provider);
}

void
PDSerialHID::sendReport(const uint8_t *bytes, size_t len)
{
	IOBufferMemoryDescriptor *md = IOBufferMemoryDescriptor::withBytes(bytes, len, kIODirectionNone);

	if (md == NULL) return;
	handleReport(md, kIOHIDReportTypeInput);
	md->release();
}

// a relative pointer for mouse-look tests, it goes straight to the raw mouse queue
static UInt8 relativeMouseIndex = 0xff;

static void
pushRelative(long dx, long dy, unsigned buttons)
{
	if (relativeMouseIndex == 0xff) relativeMouseIndex = PDHIDAllocateMouseIndex();
	do {
		long sx = dx > 127 ? 127 : dx < -127 ? -127 : dx;
		long sy = dy > 127 ? 127 : dy < -127 ? -127 : dy;

		PDHIDPushMouseEvent(relativeMouseIndex, buttons & 7, (SInt8)sx, (SInt8)sy, 0);
		dx -= sx;
		dy -= sy;
	} while (dx != 0 || dy != 0);
}

// "m X Y BUTTONS", "r DX DY BUTTONS" or "k MODS K1 K2 K3 K4 K5 K6", numbers in decimal
void
PDSerialHID::lineHandler(void *ctx, const char *line)
{
	PDSerialHID *self = (PDSerialHID *)ctx;
	long v[8] = {};
	const char *p = line + 1;
	char *end;
	int n = 0;

	if (self == NULL) return;
	if (line[0] == 'r') {
		long r[3] = {};

		for (n = 0; n < 3; n++) {
			while (*p == ' ') p++;
			if (*p == 0) break;
			r[n] = strtol(p, &end, 10);
			if (end == p) break;
			p = end;
		}
		if (n >= 2) pushRelative(r[0], r[1], (unsigned)r[2]);
		return;
	}
	if (line[0] != 'm' && line[0] != 'k') return;
	while (n < 8) {
		while (*p == ' ') p++;
		if (*p == 0) break;
		v[n++] = strtol(p, &end, 10);
		if (end == p) break;
		p = end;
	}
	if (line[0] == 'm' && n >= 2) {
		long x = v[0] > 127 ? 127 : v[0] < -127 ? -127 : v[0], y = v[1] > 127 ? 127 : v[1] < -127 ? -127 : v[1];
		uint8_t r[4] = { 2, (uint8_t)(v[2] & 7), (uint8_t)(int8_t)x, (uint8_t)(int8_t)y };

		self->sendReport(r, sizeof(r));
	} else if (line[0] == 'k') {
		uint8_t r[9] = { 1, (uint8_t)v[0], 0, (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3], (uint8_t)v[4], (uint8_t)v[5], (uint8_t)v[6] };

		self->sendReport(r, sizeof(r));
	}
}

IOReturn
PDSerialHID::newReportDescriptor(IOMemoryDescriptor **descriptor) const
{
	*descriptor = IOBufferMemoryDescriptor::withBytes(kPDSerialHIDDescriptor, sizeof(kPDSerialHIDDescriptor),
	    kIODirectionNone);
	return *descriptor ? kIOReturnSuccess : kIOReturnNoMemory;
}

OSString *
PDSerialHID::newTransportString() const
{
	return OSString::withCString("Virtual");
}

OSString *
PDSerialHID::newProductString() const
{
	return OSString::withCString("PD Serial HID");
}

OSNumber *
PDSerialHID::newVendorIDNumber() const
{
	return OSNumber::withNumber(0x0627, 32);
}

OSNumber *
PDSerialHID::newProductIDNumber() const
{
	return OSNumber::withNumber(0x5048, 32);
}

OSNumber *
PDSerialHID::newLocationIDNumber() const
{
	return OSNumber::withNumber(0x50444800, 32);
}
