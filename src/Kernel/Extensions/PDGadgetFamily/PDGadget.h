// SPDX-License-Identifier: MIT
// usb device (gadget) framework: functions, ep0, standard requests, and the hooks a controller backend fills
#ifndef _PUREDARWIN_PDGADGET_H
#define _PUREDARWIN_PDGADGET_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

struct PDUSBSetup {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__((packed));

enum { kPDEpControl = 0, kPDEpBulk = 2, kPDEpInterrupt = 3 };

class PDGadgetController;

// one usb function (cdc-acm, an adb or vendor pipe): its interfaces, endpoints and class requests
class PDGadgetFunction : public OSObject
{
	OSDeclareAbstractStructors(PDGadgetFunction);

public:
	struct Endpoint {
		uint8_t type;
		bool in;
		uint16_t maxPacket;	// high speed, bulk drops to 64 at full speed
		uint8_t interval;
		uint32_t bufferSize;
	};

	virtual uint8_t interfaceCount(void) = 0;
	virtual uint8_t endpointCount(void) = 0;
	virtual Endpoint endpoint(uint8_t index) = 0;
	// interface and endpoint descriptors, epAddr[i] is the address given to endpoint(i)
	virtual uint32_t descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed) = 0;
	// class request to one of its interfaces: IN length written to data, or -1 to stall
	virtual int setup(const PDUSBSetup *setup, uint8_t *data) = 0;
	virtual void setupData(const PDUSBSetup *setup, const uint8_t *data, uint32_t length) {}
	virtual void configured(PDGadgetController *usb, uint8_t firstEndpoint) = 0;
	virtual void deconfigured(void) = 0;
	virtual void transferDone(uint8_t index, uint32_t actual) = 0;
	virtual void poll(void) = 0;
};

// the device side of usb: everything but the controller hardware, which a subclass drives
// through the hw* hooks and reports back through setupReceived, ep0Done, transferDone and busReset
class PDGadgetController : public IOService
{
	OSDeclareAbstractStructors(PDGadgetController);

public:
	// the endpoint api functions use, by the function's endpoint index
	uint8_t *buffer(PDGadgetFunction *fn, uint8_t index);
	bool busy(PDGadgetFunction *fn, uint8_t index);
	bool queue(PDGadgetFunction *fn, uint8_t index, uint32_t length);
	// the same from offset bytes into the endpoint's buffer, a multiple of hwQueueAlign()
	bool queueAt(PDGadgetFunction *fn, uint8_t index, uint32_t offset, uint32_t length);
	// work is waiting for the controller: wake it now rather than at its next tick
	virtual void kick(void) {}
	// the board's way back to being flashed (Rockchip: Maskrom, msm8916: lk2nd), then that reset
	virtual void rebootToMaskrom(void) = 0;

	void free(void) APPLE_KEXT_OVERRIDE;

protected:
	enum { kMaxFunctions = 4, kMaxEndpoints = 16, kEp0Buffer = 4096, kEp0MaxPacket = 64 };
	enum Ep0State { kSetup, kDataIn, kDataOut, kStatusIn, kStatusOut };

	// one endpoint as the framework sees it, the backend keeps its own state by slot
	struct Ep {
		PDGadgetFunction *fn;
		uint8_t slot;		// fEps index, kMaxEndpoints and +1 for ep0 out and in
		uint8_t index, type, num;
		bool in, enabled, busy;
		uint16_t maxPacket;
		uint32_t bufferSize, requested;
		uint8_t *buf;		// backend's memory, valid once hwAllocEndpoint succeeded
	};

	// what a backend provides
	// memory for an endpoint's buffer and whatever the hardware needs with it, kept across configurations
	virtual bool hwAllocEndpoint(Ep *ep, uint32_t bufferSize) = 0;
	virtual void hwEnableEndpoint(Ep *ep) = 0;
	virtual void hwDisableEndpoint(Ep *ep) = 0;
	// start one transfer of length bytes at offset into ep->buf, ep->busy is set by the caller
	virtual bool hwQueue(Ep *ep, uint32_t offset, uint32_t length) = 0;
	virtual uint32_t hwQueueAlign(void) { return 1; }
	virtual void hwEp0Stall(void) = 0;
	// the address takes effect after the status stage, as usb wants
	virtual void hwSetAddress(uint8_t address) = 0;
	virtual void hwClearHalt(uint8_t num, bool in) = 0;

	// what a backend calls
	// once, with the ep0 buffer it allocated (kEp0Buffer bytes): sets up ep0 and the functions
	bool initGadget(uint8_t *ep0Buffer, const char *product);
	// the functions boot-args ask for: cdc-acm always, adb (pdusbadb=0 drops it), the pdfb
	// stream (pdusbfb=0/1, streamDefault when unset)
	bool addStandardFunctions(bool streamDefault);
	void addFunction(PDGadgetFunction *fn);
	void setupReceived(const PDUSBSetup *setup);
	void ep0Done(bool in, uint32_t actual);
	void transferDone(Ep *ep, uint32_t actual);
	void busReset(void);
	void setHighSpeed(bool hs) { fHighSpeed = hs; }
	// the functions' own work, on the backend's thread while configured
	void pollFunctions(void);
	// an in transfer in flight, the case a polled backend keeps looking for completions
	bool inFlight(void);

	Ep fEps[kMaxEndpoints];
	uint8_t fEpCount;
	Ep fEp0Out, fEp0In;
	uint8_t fConfiguration;
	bool fHighSpeed;
	uint32_t fTrace;	// pdusbtrace=N: log the first N requests

private:
	Ep *endpointFor(PDGadgetFunction *fn, uint8_t index);
	bool startEp0(Ep *ep, uint32_t length);
	uint32_t configDescriptor(uint8_t *out);
	bool setConfiguration(uint8_t value);
	void clearConfiguration(void);
	int standardRequest(const PDUSBSetup *s, uint8_t *data);
	PDGadgetFunction *functionForInterface(uint8_t interface);

	PDGadgetFunction *fFunctions[kMaxFunctions];
	uint8_t fFunctionCount, fInterfaceCount;
	bool fPerInterfaceClass;
	const char *fProduct;
	Ep0State fEp0State;
	PDUSBSetup fSetup;
	uint8_t *fEp0Buf;
	uint32_t fEp0InLen;
	bool fEp0InZlp;
};

#endif
