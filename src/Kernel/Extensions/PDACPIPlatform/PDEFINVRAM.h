/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _PDEFINVRAM_H
#define _PDEFINVRAM_H

#include <IOKit/nvram/IONVRAMController.h>
#include <kern/thread_call.h>

// IODTNVRAM's one CHRP bank, kept in firmware variables through EFI runtime services
class PDEFINVRAM : public IONVRAMController {
	OSDeclareDefaultStructors(PDEFINVRAM)

private:
	uint8_t      *fImage;
	uint8_t      *fStored;
	uint32_t      fSize;
	bool          fRuntime;
	bool          fWarned;
	bool          fFlushed;
	uint32_t      fPresent;

	IOLock       *fLock;
	thread_call_t fCall;
	bool          fBusy;
	int           fOp;

	static void runOp(thread_call_param_t self, thread_call_param_t);
	void perform(int op);
	bool loadChunks(void);
	void flushChunks(void);
	void warnOnce(const char *what, uint64_t status);

public:
	static PDEFINVRAM *withBank(const void *blank, uint32_t size);

	// the firmware copy, when it holds a valid bank, else the loader's blank one
	bool load(void);
	const void *image(void) const { return fImage; }

	virtual void sync(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn select(uint32_t bank) APPLE_KEXT_OVERRIDE;
	virtual IOReturn eraseBank(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn read(IOByteCount offset, UInt8 *buffer, IOByteCount length) APPLE_KEXT_OVERRIDE;
	virtual IOReturn write(IOByteCount offset, UInt8 *buffer, IOByteCount length) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;
};

#endif // _PDEFINVRAM_H
