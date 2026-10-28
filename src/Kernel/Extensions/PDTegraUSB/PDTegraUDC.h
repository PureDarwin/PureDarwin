// SPDX-License-Identifier: MIT
// tegra210 legacy usb1 otg port (chipidea, nvidia,tegra210-udc) as a polled PDGadgetController backend
#ifndef _PUREDARWIN_PDTEGRAUDC_H
#define _PUREDARWIN_PDTEGRAUDC_H

#include "PDGadget.h"

// u-boot's own gadget leaves clocks and the utmi phy up, so the core only needs a reset into
// device mode. descriptors and buffers come from one contiguous pool mapped uncached
class PDTegraUDC : public PDGadgetController
{
	OSDeclareDefaultStructors(PDTegraUDC);

public:
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
	void rebootToMaskrom(void) APPLE_KEXT_OVERRIDE;
	void kick(void) APPLE_KEXT_OVERRIDE;

protected:
	bool hwAllocEndpoint(Ep *ep, uint32_t bufferSize) APPLE_KEXT_OVERRIDE;
	void hwEnableEndpoint(Ep *ep) APPLE_KEXT_OVERRIDE;
	void hwDisableEndpoint(Ep *ep) APPLE_KEXT_OVERRIDE;
	bool hwQueue(Ep *ep, uint32_t offset, uint32_t length) APPLE_KEXT_OVERRIDE;
	uint32_t hwQueueAlign(void) APPLE_KEXT_OVERRIDE { return kTdBytes; }
	void hwEp0Stall(void) APPLE_KEXT_OVERRIDE;
	void hwSetAddress(uint8_t address) APPLE_KEXT_OVERRIDE;
	void hwClearHalt(uint8_t num, bool in) APPLE_KEXT_OVERRIDE;

private:
	enum { kHwEndpoints = 8, kPoolSize = 512 * 1024, kTdBytes = 16384 };

	// device queue head, 64 bytes, one per endpoint and direction (ep0 out, ep0 in, ep1 out, ...)
	struct Qh {
		uint32_t config;
		uint32_t current;
		uint32_t next;		// the overlay: the descriptor the core works on
		uint32_t token;
		uint32_t page[5];
		uint32_t reserved;
		uint8_t setup[8];
		uint32_t pad[4];
	};
	// device transfer descriptor, 32 bytes aligned
	struct Td {
		uint32_t next;
		uint32_t token;
		uint32_t page[5];
		uint32_t pad;
	};
	// what the core needs per framework endpoint slot
	struct Hw {
		uint32_t bufPhys;
		volatile Td *td;	// ntd descriptors back to back, ins above 16 KB chain them
		uint32_t tdPhys;
		uint8_t ntd, tdUsed;
	};

	static void pollThread(void *arg, wait_result_t wr);
	void pollLoop(void);
	bool clockUp(void);
	void utmipDump(const char *when);
	void utmipPowerUp(void);
	void watchPort(void);
	bool initCore(void);
	void *dmaAlloc(uint32_t size, uint32_t align, uint32_t *phys);
	volatile Qh *qh(uint8_t num, bool in) { return &fQh[num * 2 + (in ? 1 : 0)]; }
	uint32_t bit(uint8_t num, bool in) const { return 1u << (num + (in ? 16 : 0)); }
	Hw *hw(Ep *ep) { return &fHw[ep->slot]; }
	volatile Td *lastTd(Ep *ep) { Hw *h = hw(ep); return &h->td[h->tdUsed > 1 ? h->tdUsed - 1 : 0]; }
	uint32_t leftBytes(Ep *ep);
	void flush(uint32_t bits);
	void coreBusReset(void);
	void portChange(void);
	void readSetup(void);
	void completions(void);

	uint32_t reg(uint32_t off) const { return *(volatile uint32_t *)(fRegs + off); }
	void setReg(uint32_t off, uint32_t v) { *(volatile uint32_t *)(fRegs + off) = v; }

	IOMemoryMap *fRegsMap;
	uint64_t fWatchLast = 0;
	uint32_t fWatchLeft = 120, fWatchKey = 0;
	volatile uint8_t *fRegs;
	IOBufferMemoryDescriptor *fDMA;
	IOMemoryDescriptor *fDMAPhysMD;
	IOMemoryMap *fDMAMap;
	uint8_t *fDMABase;
	uint32_t fDMAPhys, fDMAUsed;

	volatile Qh *fQh;
	uint32_t fQhPhys;
	Hw fHw[kMaxEndpoints + 2];
	bool fRunning;
	uint32_t fKicks;	// event the idle poll thread waits on, bumped by kick()
	uint64_t fLastXfer;	// mach time of the last prime or completion, bounds the busy spin
};

#endif
