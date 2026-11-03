#ifndef _PUREDARWIN_PDROCKCHIPDWC3_H
#define _PUREDARWIN_PDROCKCHIPDWC3_H

#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

/*
 * RV1106 USB OTG port (a Synopsys DWC_usb3 core limited to high speed) run as a USB
 * device, polled. Functions (CDC-ACM now) supply interfaces and endpoints; the
 * controller owns ep0, the standard requests and the configuration descriptor.
 */
struct PDUSBSetup {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;
} __attribute__((packed));

enum { kPDEpControl = 0, kPDEpBulk = 2, kPDEpInterrupt = 3 };

class PDGadgetController;

class PDGadgetFunction : public OSObject
{
	OSDeclareAbstractStructors(PDGadgetFunction);

public:
	struct Endpoint {
		uint8_t type;
		bool in;
		uint16_t maxPacket;	// high speed; bulk drops to 64 at full speed
		uint8_t interval;
		uint32_t bufferSize;
	};

	virtual uint8_t interfaceCount(void) = 0;
	virtual uint8_t endpointCount(void) = 0;
	virtual Endpoint endpoint(uint8_t index) = 0;
	// Interface and endpoint descriptors; epAddr[i] is the address given to endpoint(i)
	virtual uint32_t descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed) = 0;
	// Class request to one of its interfaces: IN length written to data, or -1 to stall
	virtual int setup(const PDUSBSetup *setup, uint8_t *data) = 0;
	virtual void setupData(const PDUSBSetup *setup, const uint8_t *data, uint32_t length) {}
	virtual void configured(PDGadgetController *usb, uint8_t firstEndpoint) = 0;
	virtual void deconfigured(void) = 0;
	virtual void transferDone(uint8_t index, uint32_t actual) = 0;
	virtual void poll(void) = 0;
};

// what a function sees of the device controller running it: the dwc3 here, the dwc2 on the rk3506
class PDGadgetController : public IOService
{
	OSDeclareAbstractStructors(PDGadgetController);

public:
	// Endpoint API for functions, by the function's endpoint index
	virtual uint8_t *buffer(PDGadgetFunction *fn, uint8_t index) = 0;
	virtual bool busy(PDGadgetFunction *fn, uint8_t index) = 0;
	virtual bool queue(PDGadgetFunction *fn, uint8_t index, uint32_t length) = 0;
	// the boot rom's usb download mode (Maskrom) on the next reset, then that reset
	virtual void rebootToMaskrom(void) = 0;

protected:
	// osreg is the reboot-mode register the boot rom reads, cru the cru with the global resets
	static void romDownloadReset(uint32_t osreg, uint32_t cru);
};

class PDRockchipDWC3 : public PDGadgetController
{
	OSDeclareDefaultStructors(PDRockchipDWC3);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;

	uint8_t *buffer(PDGadgetFunction *fn, uint8_t index) APPLE_KEXT_OVERRIDE;
	bool busy(PDGadgetFunction *fn, uint8_t index) APPLE_KEXT_OVERRIDE;
	bool queue(PDGadgetFunction *fn, uint8_t index, uint32_t length) APPLE_KEXT_OVERRIDE;
	void rebootToMaskrom(void) APPLE_KEXT_OVERRIDE;

private:
	enum { kMaxEndpoints = 8, kMaxFunctions = 4, kEp0Buffer = 512, kEventBuffer = 4096 };

	struct Trb {
		uint32_t bpl, bph, size, ctrl;
	};
	struct Ep {
		PDGadgetFunction *fn;
		uint8_t index, type, interval, resource;
		uint16_t maxPacket;
		bool enabled, busy;
		uint32_t requested, bufferSize;
		uint8_t *buf;
		uint32_t bufPhys;
		volatile Trb *trb;
		uint32_t trbPhys;
	};
	enum Ep0State { kSetup, kDataIn, kDataOut, kStatusIn, kStatusOut };

	static void pollThread(void *arg, wait_result_t wr);
	void pollLoop(void);
	void setupClocksEarly(void);
	void setupClocks(void);
	bool initCore(void);
	void *dmaAlloc(uint32_t size, uint32_t align, uint32_t *phys);
	bool epCommand(uint8_t phys, uint32_t cmd, uint32_t p0, uint32_t p1, uint32_t p2);
	bool epConfigure(uint8_t phys, uint32_t action);
	bool startTransfer(uint8_t phys, uint32_t bufPhys, uint32_t length, uint32_t trbType);
	void endTransfer(uint8_t phys);
	void handleEvent(uint32_t ev);
	void deviceEvent(uint32_t ev);
	void ep0Event(uint8_t phys, uint32_t type, uint32_t status);
	void ep0Setup(void);
	void ep0Stall(void);
	void ep0Status(uint8_t phys);
	int standardRequest(const PDUSBSetup *s, uint8_t *data);
	uint32_t configDescriptor(uint8_t *out);
	bool setConfiguration(uint8_t value);
	void clearConfiguration(void);
	void addFunction(PDGadgetFunction *fn);
	Ep *endpointFor(PDGadgetFunction *fn, uint8_t index, uint8_t *phys);

	uint32_t reg(uint32_t off) const { return *(volatile uint32_t *)(fRegs + off); }
	void setReg(uint32_t off, uint32_t v) { *(volatile uint32_t *)(fRegs + off) = v; }

	IOMemoryDescriptor *fRegsMD;
	IOMemoryMap *fRegsMap;
	volatile uint8_t *fRegs;
	IOBufferMemoryDescriptor *fDMA;
	IOMemoryDescriptor *fDMAPhysMD;
	IOMemoryMap *fDMAMap;
	uint8_t *fDMABase;
	uint32_t fDMAPhys, fDMAUsed;

	volatile uint32_t *fEvents;
	uint32_t fEventPos;

	Ep fEps[kMaxEndpoints * 2 + 2];	// by physical number: OUT even, IN odd
	PDGadgetFunction *fFunctions[kMaxFunctions];
	bool fPerInterfaceClass;
	uint8_t fFunctionCount, fInterfaceCount, fNextIn, fNextOut;

	Ep0State fEp0State;
	PDUSBSetup fSetup;
	uint8_t *fSetupBuf, *fEp0Buf;
	uint32_t fSetupPhys, fEp0Phys;
	bool fStatusReady[2];
	uint8_t fPendingAddress, fConfiguration;
	bool fHighSpeed, fRunning;
	uint32_t fBootPhyCfg, fBootGuctl1;
	uint32_t fTrace;	// pdusbtrace=N: log the first N events and requests
};

#endif /* _PUREDARWIN_PDROCKCHIPDWC3_H */
