// SPDX-License-Identifier: MIT
// a bulk pipe interface handed to userland as /dev/<name>: adb (ff/42/01), the display stream (ff/50/44)
#ifndef _PUREDARWIN_PDGADGETPIPE_H
#define _PUREDARWIN_PDGADGETPIPE_H

#include "PDGadget.h"

#include <IOKit/IOLocks.h>
#include <sys/select.h>

// one read per out transfer, one in transfer per write, enxio while unconfigured makes the user start over
class PDGadgetPipe : public PDGadgetFunction
{
	OSDeclareDefaultStructors(PDGadgetPipe);

public:
	enum { kDataIn = 0, kDataOut = 1, kBufferSize = 16384, kMaxPipes = 4, kInSlots = 2 };

	// node is the /dev name, cls, sub and proto the interface's class triple
	// inSize: the largest single write sent as one transfer (a multiple of 16 KB, up to 64 KB)
	bool initPipe(const char *node, uint8_t cls, uint8_t sub, uint8_t proto, uint32_t inSize = kBufferSize);
	void free(void) APPLE_KEXT_OVERRIDE;

	uint8_t interfaceCount(void) APPLE_KEXT_OVERRIDE { return 1; }
	uint8_t endpointCount(void) APPLE_KEXT_OVERRIDE { return 2; }
	Endpoint endpoint(uint8_t index) APPLE_KEXT_OVERRIDE;
	uint32_t descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed) APPLE_KEXT_OVERRIDE;
	int setup(const PDUSBSetup *setup, uint8_t *data) APPLE_KEXT_OVERRIDE { return -1; }
	void configured(PDGadgetController *usb, uint8_t firstEndpoint) APPLE_KEXT_OVERRIDE;
	void deconfigured(void) APPLE_KEXT_OVERRIDE;
	void transferDone(uint8_t index, uint32_t actual) APPLE_KEXT_OVERRIDE;
	void poll(void) APPLE_KEXT_OVERRIDE;

	// the /dev node, called from the BSD side
	int devRead(struct uio *uio, int flags);
	int devWrite(struct uio *uio, int flags);
	int devSelect(int which, void *wql, struct proc *p);

private:
	// the node, made once devfs takes nodes: a controller can start ahead of it
	void publishNode(void);
	void resetSlots(void);
	char fName[16];
	uint8_t fClass, fSubClass, fProtocol;
	int fMinor;
	uint32_t fInSize;
	void *fNode;
	uint64_t fNextPublish;	// devfs says why it refused on every try: retry once a second
	PDGadgetController *fUSB;
	IOLock *fLock;
	int fMajor;
	bool fOnline;
	// OUT: bytes of the last transfer still unread, the endpoint is requeued once drained
	uint32_t fOutLen, fOutOff;
	bool fOutRequeue;
	// IN: two slots of fInSize in the endpoint buffer, filled by writes and sent in order,
	// so a write copies into one while the other is on the bus
	enum SlotState { kFree, kFilled, kInFlight };
	uint32_t fSlotLen[kInSlots];
	SlotState fSlot[kInSlots];
	uint8_t fNextFill, fNextSend;
	struct selinfo fReadSel, fWriteSel;
};

#endif
