#ifndef _PUREDARWIN_PDGADGETACM_H
#define _PUREDARWIN_PDGADGETACM_H

#include "PDRockchipDWC3.h"

/*
 * CDC-ACM serial function carrying the system console: what the UART console prints is
 * mirrored to the bulk IN endpoint (the kernel message buffer first, once the host
 * raises DTR), and bytes from the host are typed into the console tty. A break from the
 * host reboots the board into the boot ROM's USB download mode.
 */
class PDGadgetACM : public PDGadgetFunction
{
	OSDeclareDefaultStructors(PDGadgetACM);

public:
	enum { kNotify = 0, kDataIn = 1, kDataOut = 2 };

	bool init(void) APPLE_KEXT_OVERRIDE { return PDGadgetFunction::init(); }

	uint8_t interfaceCount(void) APPLE_KEXT_OVERRIDE { return 2; }
	uint8_t endpointCount(void) APPLE_KEXT_OVERRIDE { return 3; }
	Endpoint endpoint(uint8_t index) APPLE_KEXT_OVERRIDE;
	uint32_t descriptors(uint8_t *out, uint8_t firstInterface, const uint8_t *epAddr, bool highSpeed) APPLE_KEXT_OVERRIDE;
	int setup(const PDUSBSetup *setup, uint8_t *data) APPLE_KEXT_OVERRIDE;
	void setupData(const PDUSBSetup *setup, const uint8_t *data, uint32_t length) APPLE_KEXT_OVERRIDE;
	void configured(PDGadgetController *usb, uint8_t firstEndpoint) APPLE_KEXT_OVERRIDE;
	void deconfigured(void) APPLE_KEXT_OVERRIDE;
	void transferDone(uint8_t index, uint32_t actual) APPLE_KEXT_OVERRIDE;
	void poll(void) APPLE_KEXT_OVERRIDE;

private:
	void startMirror(void);
	void rebootToMaskrom(void);

	PDGadgetController *fUSB;
	uint8_t fLineCoding[7];
	bool fDTR, fMirroring, fBreak;
	uint64_t fDTRTime, fBreakTime;
};

#endif /* _PUREDARWIN_PDGADGETACM_H */
