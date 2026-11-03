#ifndef _PUREDARWIN_PDGADGETADB_H
#define _PUREDARWIN_PDGADGETADB_H

#include "PDRockchipDWC3.h"

#include <IOKit/IOLocks.h>
#include <sys/select.h>

/*
 * The ADB interface (vendor class ff/42/01, one bulk pair) handed to userland as
 * /dev/adb: each read returns what one OUT transfer carried, each write becomes one IN
 * transfer, so adbd's header and payload writes arrive as the host expects them. Reads
 * and writes fail with ENXIO while the host is not configured, which tells adbd to
 * start over.
 */
class PDGadgetADB : public PDGadgetFunction
{
	OSDeclareDefaultStructors(PDGadgetADB);

public:
	enum { kDataIn = 0, kDataOut = 1, kBufferSize = 16384 };

	bool init(void) APPLE_KEXT_OVERRIDE;
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

	// /dev/adb, called from the BSD side
	int devRead(struct uio *uio, int flags);
	int devWrite(struct uio *uio, int flags);
	int devSelect(int which, void *wql, struct proc *p);

private:
	// /dev/adb, made once devfs takes nodes: a controller can start ahead of it
	void publishNode(void);
	void *fNode;
	PDGadgetController *fUSB;
	IOLock *fLock;
	int fMajor;
	bool fOnline;
	// OUT: bytes of the last transfer still unread; the endpoint is requeued once drained
	uint32_t fOutLen, fOutOff;
	bool fOutRequeue;
	// IN: a write waiting for (or in) its transfer
	uint32_t fInLen;
	bool fInPending, fInQueued;
	struct selinfo fReadSel, fWriteSel;
};

#endif /* _PUREDARWIN_PDGADGETADB_H */
