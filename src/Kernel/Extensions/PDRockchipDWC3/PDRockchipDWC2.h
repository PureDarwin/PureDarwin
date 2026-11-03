#ifndef _PUREDARWIN_PDROCKCHIPDWC2_H
#define _PUREDARWIN_PDROCKCHIPDWC2_H

#include "PDRockchipDWC3.h"

// rk3506 usb otg port (a synopsys dwc2 core) run as a usb device, polled and in slave mode:
// every packet goes through the core's fifos, so endpoint buffers are plain kernel memory
class PDRockchipDWC2 : public PDGadgetController
{
	OSDeclareDefaultStructors(PDRockchipDWC2);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;

	uint8_t *buffer(PDGadgetFunction *fn, uint8_t index) APPLE_KEXT_OVERRIDE;
	bool busy(PDGadgetFunction *fn, uint8_t index) APPLE_KEXT_OVERRIDE;
	bool queue(PDGadgetFunction *fn, uint8_t index, uint32_t length) APPLE_KEXT_OVERRIDE;
	void rebootToMaskrom(void) APPLE_KEXT_OVERRIDE;

private:
	enum { kMaxEndpoints = 8, kMaxFunctions = 4, kEp0Buffer = 512 };

	struct Ep {
		PDGadgetFunction *fn;
		uint8_t index, type, num;
		bool in, enabled, busy;
		uint16_t maxPacket;
		uint32_t bufferSize, requested, done;
		uint8_t *buf;
	};
	enum Ep0State { kSetup, kDataIn, kDataOut, kStatusIn, kStatusOut };

	static void pollThread(void *arg, wait_result_t wr);
	void pollLoop(void);
	bool initCore(void);
	void sizeFifos(void);
	void flushFifos(void);
	void busReset(void);
	void enumDone(void);
	void readPacket(void);
	void inEndpointEvents(void);
	void outEndpointEvents(void);
	void fillTxFifo(Ep *ep);
	bool startIn(Ep *ep, uint32_t length);
	bool startOut(Ep *ep, uint32_t length);
	void ep0ArmOut(void);
	void ep0In(const uint8_t *data, uint32_t length);
	void ep0Stall(void);
	void ep0SetupDone(void);
	int standardRequest(const PDUSBSetup *s, uint8_t *data);
	uint32_t configDescriptor(uint8_t *out);
	bool setConfiguration(uint8_t value);
	void clearConfiguration(void);
	void addFunction(PDGadgetFunction *fn);
	Ep *endpointFor(PDGadgetFunction *fn, uint8_t index);

	uint32_t reg(uint32_t off) const { return *(volatile uint32_t *)(fRegs + off); }
	void setReg(uint32_t off, uint32_t v) { *(volatile uint32_t *)(fRegs + off) = v; }

	IOMemoryDescriptor *fRegsMD;
	IOMemoryMap *fRegsMap;
	volatile uint8_t *fRegs;

	// ep0 both ways, then the functions' endpoints; in and out numbers count up separately
	Ep fEp0In, fEp0Out;
	Ep fEps[kMaxEndpoints * 2];
	uint8_t fEpCount;
	PDGadgetFunction *fFunctions[kMaxFunctions];
	bool fPerInterfaceClass;
	uint8_t fFunctionCount, fInterfaceCount;

	Ep0State fEp0State;
	PDUSBSetup fSetup;
	uint8_t fSetupBuf[8];
	uint8_t *fEp0Buf;
	// an ep0 in stage goes out a packet at a time from fEp0Buf
	uint32_t fEp0InLen, fEp0InSent;
	bool fEp0InZlp;
	uint8_t fConfiguration;
	bool fHighSpeed, fRunning;
	uint32_t fTrace;	// pdusbtrace=N: log the first N events and requests
};

#endif /* _PUREDARWIN_PDROCKCHIPDWC2_H */
