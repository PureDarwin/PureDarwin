// SPDX-License-Identifier: MIT
// cdc-acm function carrying the system console over usb
#ifndef _PUREDARWIN_PDGADGETACM_H
#define _PUREDARWIN_PDGADGETACM_H

#include "PDGadget.h"

// console output is mirrored to bulk in (msgbuf first once dtr rises), host bytes go to the console tty
// a break from the host reboots into the boot rom download mode
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

#endif
