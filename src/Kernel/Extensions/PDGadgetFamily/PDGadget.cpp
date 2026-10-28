// SPDX-License-Identifier: MIT
// usb device framework: the configuration, ep0's three stages and the standard requests, for any controller
#include "PDGadget.h"
#include "PDGadgetACM.h"
#include "PDGadgetPipe.h"

#include <IOKit/IOLib.h>
#include <pexpert/pexpert.h>

OSDefineMetaClassAndAbstractStructors(PDGadgetFunction, OSObject);
OSDefineMetaClassAndAbstractStructors(PDGadgetController, IOService);

#define USB_VENDOR  0x1209	// pid.codes test range, as the Rockchip gadgets use
#define USB_PRODUCT 0x0001

static uint32_t
minU32(uint32_t a, uint32_t b)
{
	return a < b ? a : b;
}

bool
PDGadgetController::initGadget(uint8_t *ep0Buffer, const char *product)
{
	fEp0Buf = ep0Buffer;
	fProduct = product;
	fHighSpeed = true;
	fEp0Out.slot = kMaxEndpoints;
	fEp0In.slot = kMaxEndpoints + 1;
	fEp0Out.in = false;
	fEp0In.in = true;
	fEp0Out.num = fEp0In.num = 0;
	fEp0Out.maxPacket = fEp0In.maxPacket = kEp0MaxPacket;
	fEp0Out.buf = fEp0In.buf = ep0Buffer;
	fEp0Out.bufferSize = fEp0In.bufferSize = kEp0Buffer;
	fEp0Out.enabled = fEp0In.enabled = true;
	for (uint8_t i = 0; i < kMaxEndpoints; i++) {
		fEps[i].slot = i;
	}
	PE_parse_boot_argn("pdusbtrace", &fTrace, sizeof(fTrace));
	return fEp0Buf != NULL;
}

bool
PDGadgetController::addStandardFunctions(bool streamDefault)
{
	PDGadgetACM *acm = OSTypeAlloc(PDGadgetACM);
	if (acm == NULL || !acm->init()) {
		OSSafeReleaseNULL(acm);
		return false;
	}
	addFunction(acm);
	// pdusbadb=0 leaves the adb interface out
	uint32_t wantADB = 1;
	PE_parse_boot_argn("pdusbadb", &wantADB, sizeof(wantADB));
	if (wantADB) {
		PDGadgetPipe *adb = OSTypeAlloc(PDGadgetPipe);
		if (adb != NULL && adb->initPipe("adb", 0xff, 0x42, 0x01)) {
			addFunction(adb);
			// adb only looks at devices whose class is defined per interface
			fPerInterfaceClass = true;
		} else {
			OSSafeReleaseNULL(adb);
		}
	}
	// the display stream: a vendor interface the host viewer claims
	uint32_t wantFB = streamDefault ? 1 : 0;
	PE_parse_boot_argn("pdusbfb", &wantFB, sizeof(wantFB));
	if (wantFB) {
		PDGadgetPipe *fb = OSTypeAlloc(PDGadgetPipe);
		// pdusbfbkb sizes each of its two in slots, 16 to 128 KB
		uint32_t fbKB = 64;
		PE_parse_boot_argn("pdusbfbkb", &fbKB, sizeof(fbKB));
		fbKB = fbKB < 16 ? 16 : fbKB > 128 ? 128 : fbKB;
		if (fb != NULL && fb->initPipe("pdfb", 0xff, 0x50, 0x44, fbKB * 1024)) {
			addFunction(fb);
			fPerInterfaceClass = true;
		} else {
			OSSafeReleaseNULL(fb);
		}
	}
	return true;
}

void
PDGadgetController::addFunction(PDGadgetFunction *fn)
{
	if (fFunctionCount < kMaxFunctions) {
		fFunctions[fFunctionCount++] = fn;
		fInterfaceCount += fn->interfaceCount();
	}
}

void
PDGadgetController::free(void)
{
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		OSSafeReleaseNULL(fFunctions[f]);
	}
	IOService::free();
}

PDGadgetController::Ep *
PDGadgetController::endpointFor(PDGadgetFunction *fn, uint8_t index)
{
	for (uint8_t i = 0; i < fEpCount; i++) {
		if (fEps[i].fn == fn && fEps[i].index == index) {
			return &fEps[i];
		}
	}
	return NULL;
}

PDGadgetFunction *
PDGadgetController::functionForInterface(uint8_t interface)
{
	for (uint8_t f = 0, first = 0; f < fFunctionCount; f++) {
		uint8_t count = fFunctions[f]->interfaceCount();

		if (interface >= first && interface < first + count) {
			return fFunctions[f];
		}
		first += count;
	}
	return NULL;
}

uint8_t *
PDGadgetController::buffer(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index);

	return ep != NULL ? ep->buf : NULL;
}

bool
PDGadgetController::busy(PDGadgetFunction *fn, uint8_t index)
{
	Ep *ep = endpointFor(fn, index);

	return ep == NULL || !ep->enabled || ep->busy;
}

bool
PDGadgetController::queue(PDGadgetFunction *fn, uint8_t index, uint32_t length)
{
	return queueAt(fn, index, 0, length);
}

bool
PDGadgetController::queueAt(PDGadgetFunction *fn, uint8_t index, uint32_t offset, uint32_t length)
{
	Ep *ep = endpointFor(fn, index);

	if (ep == NULL || !ep->enabled || ep->busy || (offset % hwQueueAlign()) != 0 ||
	    offset + length > ep->bufferSize) {
		return false;
	}
	ep->requested = length;
	ep->busy = true;
	if (!hwQueue(ep, offset, length)) {
		ep->busy = false;
		return false;
	}
	return true;
}

bool
PDGadgetController::startEp0(Ep *ep, uint32_t length)
{
	ep->requested = length;
	ep->busy = true;
	if (!hwQueue(ep, 0, length)) {
		ep->busy = false;
		return false;
	}
	return true;
}

bool
PDGadgetController::inFlight(void)
{
	if (fEp0In.busy || fEp0Out.busy) {
		return true;
	}
	// an out waits on the host, which can take any time: only ins count
	for (uint8_t i = 0; i < fEpCount; i++) {
		if (fEps[i].busy && fEps[i].in) {
			return true;
		}
	}
	return false;
}

void
PDGadgetController::pollFunctions(void)
{
	if (fConfiguration == 0) {
		return;
	}
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		fFunctions[f]->poll();
	}
}

// device, configuration and interface descriptors, laid out as the Rockchip gadgets do
static const uint8_t sDeviceDescriptor[18] = {
	18, 1, 0x00, 0x02, 0xef, 0x02, 0x01, 64,
	USB_VENDOR & 0xff, USB_VENDOR >> 8, USB_PRODUCT & 0xff, USB_PRODUCT >> 8,
	0x00, 0x01, 1, 2, 3, 1,
};

uint32_t
PDGadgetController::configDescriptor(uint8_t *out)
{
	uint32_t len = 9;
	uint8_t iface = 0, in = 1, outEp = 1;

	for (uint8_t f = 0; f < fFunctionCount; f++) {
		PDGadgetFunction *fn = fFunctions[f];
		uint8_t addr[kMaxEndpoints];

		for (uint8_t i = 0; i < fn->endpointCount() && i < kMaxEndpoints; i++) {
			addr[i] = fn->endpoint(i).in ? (0x80 | in++) : outEp++;
		}
		len += fn->descriptors(out + len, iface, addr, fHighSpeed);
		iface += fn->interfaceCount();
	}
	out[0] = 9;
	out[1] = 2;
	out[2] = len & 0xff;
	out[3] = len >> 8;
	out[4] = iface;
	out[5] = 1;
	out[6] = 0;
	out[7] = 0x80;		// bus powered
	out[8] = 250;		// 500 mA
	return len;
}

bool
PDGadgetController::setConfiguration(uint8_t value)
{
	uint8_t in = 1, out = 1;

	clearConfiguration();
	if (value == 0) {
		return true;
	}
	if (value != 1) {
		return false;
	}
	for (uint8_t f = 0; f < fFunctionCount; f++) {
		PDGadgetFunction *fn = fFunctions[f];

		for (uint8_t i = 0; i < fn->endpointCount(); i++) {
			PDGadgetFunction::Endpoint d = fn->endpoint(i);

			if (fEpCount >= kMaxEndpoints) {
				return false;
			}
			Ep *ep = &fEps[fEpCount++];
			// slots keep their buffers across configurations, the functions never change.
			// direction first: a backend may lay out memory by it
			ep->in = d.in;
			if (!hwAllocEndpoint(ep, d.bufferSize)) {
				return false;
			}
			ep->bufferSize = d.bufferSize;
			ep->fn = fn;
			ep->index = i;
			ep->type = d.type;
			ep->num = d.in ? in++ : out++;
			ep->maxPacket = (!fHighSpeed && d.type == kPDEpBulk && d.maxPacket > 64) ? 64 : d.maxPacket;
			ep->busy = false;
			hwEnableEndpoint(ep);
			ep->enabled = true;
		}
	}
	fConfiguration = value;
	for (uint8_t f = 0, first = 0; f < fFunctionCount; f++) {
		fFunctions[f]->configured(this, first);
		first += fFunctions[f]->endpointCount();
	}
	IOLog("%s: configured (%s speed)\n", getName(), fHighSpeed ? "high" : "full");
	return true;
}

void
PDGadgetController::clearConfiguration(void)
{
	if (fConfiguration != 0) {
		for (uint8_t f = 0; f < fFunctionCount; f++) {
			fFunctions[f]->deconfigured();
		}
	}
	for (uint8_t i = 0; i < fEpCount; i++) {
		if (fEps[i].enabled) {
			hwDisableEndpoint(&fEps[i]);
		}
		fEps[i].enabled = false;
		fEps[i].busy = false;
	}
	fEpCount = 0;
	fConfiguration = 0;
}

// the backend saw a bus reset and cleared its own address: back to the default state, ep0 waiting for a setup
void
PDGadgetController::busReset(void)
{
	clearConfiguration();
	fEp0In.busy = fEp0Out.busy = false;
	fEp0State = kSetup;
}

int
PDGadgetController::standardRequest(const PDUSBSetup *s, uint8_t *data)
{
	switch (s->bRequest) {
	case 0x00:	// GET_STATUS
		data[0] = data[1] = 0;
		return 2;
	case 0x01:	// CLEAR_FEATURE (endpoint halt): unstall and reset the data toggle
		if ((s->bmRequestType & 0x1f) == 2 && s->wValue == 0) {
			uint8_t addr = s->wIndex & 0xff, num = addr & 0x7f;

			if (num != 0) {
				hwClearHalt(num, (addr & 0x80) != 0);
			}
		}
		return 0;
	case 0x03:	// SET_FEATURE
		return 0;
	case 0x05:	// SET_ADDRESS, the backend applies it after the status stage
		hwSetAddress(s->wValue & 0x7f);
		return 0;
	case 0x06: {	// GET_DESCRIPTOR
		uint8_t type = s->wValue >> 8, index = s->wValue & 0xff;

		switch (type) {
		case 1:
			bcopy(sDeviceDescriptor, data, sizeof(sDeviceDescriptor));
			if (fPerInterfaceClass) {
				data[4] = data[5] = data[6] = 0;
			}
			return sizeof(sDeviceDescriptor);
		case 2:
			return configDescriptor(data);
		case 3: {
			const char *strings[] = { NULL, "PureDarwin", fProduct, "0001" };

			if (index == 0) {
				data[0] = 4;
				data[1] = 3;
				data[2] = 0x09;
				data[3] = 0x04;
				return 4;
			}
			if (index < sizeof(strings) / sizeof(strings[0]) && strings[index] != NULL) {
				const char *str = strings[index];
				uint32_t n = 0;

				while (str[n] != 0 && n < 100) {
					data[2 + 2 * n] = str[n];
					data[3 + 2 * n] = 0;
					n++;
				}
				data[0] = 2 + 2 * n;
				data[1] = 3;
				return data[0];
			}
			return -1;
		}
		case 6: {	// device qualifier
			static const uint8_t q[10] = { 10, 6, 0x00, 0x02, 0xef, 0x02, 0x01, 64, 1, 0 };

			bcopy(q, data, sizeof(q));
			if (fPerInterfaceClass) {
				data[4] = data[5] = data[6] = 0;
			}
			return sizeof(q);
		}
		default:
			return -1;
		}
	}
	case 0x08:	// GET_CONFIGURATION
		data[0] = fConfiguration;
		return 1;
	case 0x09:	// SET_CONFIGURATION
		return setConfiguration(s->wValue & 0xff) ? 0 : -1;
	case 0x0a:	// GET_INTERFACE
		data[0] = 0;
		return 1;
	case 0x0b:	// SET_INTERFACE, alternate 0 only
		return s->wValue == 0 ? 0 : -1;
	default:
		return -1;
	}
}

// a setup packet the backend read off ep0, anything ep0 still had queued already dropped
void
PDGadgetController::setupReceived(const PDUSBSetup *setup)
{
	const PDUSBSetup *s = &fSetup;
	int len = -1;

	fSetup = *setup;
	fEp0In.busy = fEp0Out.busy = false;
	if ((s->bmRequestType & 0x60) == 0) {
		len = standardRequest(s, fEp0Buf);
	} else if ((s->bmRequestType & 0x1f) == 1) {
		PDGadgetFunction *fn = functionForInterface(s->wIndex & 0xff);

		if (fn != NULL) {
			len = fn->setup(s, fEp0Buf);
		}
	}
	if (fTrace > 0) {
		fTrace--;
		IOLog("%s: setup %02x %02x %04x %04x %u -> %d\n", getName(), s->bmRequestType, s->bRequest,
		    s->wValue, s->wIndex, s->wLength, len);
	}
	if (len < 0) {
		IOLog("%s: stall %02x %02x %04x %04x %u\n", getName(), s->bmRequestType, s->bRequest,
		    s->wValue, s->wIndex, s->wLength);
		hwEp0Stall();
		fEp0State = kSetup;
		return;
	}
	if (s->wLength == 0) {
		fEp0State = kStatusIn;
		startEp0(&fEp0In, 0);
	} else if (s->bmRequestType & 0x80) {
		fEp0State = kDataIn;
		fEp0InLen = minU32(minU32((uint32_t)len, s->wLength), kEp0Buffer);
		// a short answer ending on a packet boundary needs a zero length packet to end it
		fEp0InZlp = fEp0InLen < s->wLength && fEp0InLen != 0 && (fEp0InLen % kEp0MaxPacket) == 0;
		startEp0(&fEp0In, fEp0InLen);
	} else {
		fEp0State = kDataOut;
		startEp0(&fEp0Out, minU32(s->wLength, kEp0Buffer));
	}
}

// an ep0 transfer finished: the next stage of the control transfer
void
PDGadgetController::ep0Done(bool in, uint32_t actual)
{
	Ep *ep = in ? &fEp0In : &fEp0Out;

	ep->busy = false;
	switch (fEp0State) {
	case kDataIn:
		if (!in) {
			break;
		}
		if (fEp0InZlp) {
			fEp0InZlp = false;
			startEp0(&fEp0In, 0);
			break;
		}
		fEp0State = kStatusOut;		// the host's zero length out ends it
		startEp0(&fEp0Out, 0);
		break;
	case kDataOut:
		if (in) {
			break;
		}
		if (PDGadgetFunction *fn = functionForInterface(fSetup.wIndex & 0xff)) {
			fn->setupData(&fSetup, fEp0Buf, actual);
		}
		fEp0State = kStatusIn;
		startEp0(&fEp0In, 0);
		break;
	case kStatusIn:
	case kStatusOut:
		fEp0State = kSetup;
		break;
	default:
		break;
	}
}

void
PDGadgetController::transferDone(Ep *ep, uint32_t actual)
{
	ep->busy = false;
	ep->fn->transferDone(ep->index, actual);
}
