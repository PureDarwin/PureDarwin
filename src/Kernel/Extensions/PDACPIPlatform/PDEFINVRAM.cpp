/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PDEFINVRAM.h"

#include <IOKit/IOLib.h>
#include <i386/pal_routines.h>
#include <pexpert/i386/efi.h>

extern "C" {
extern void *gPEEFIRuntimeServices;
boolean_t ml_set_interrupts_enabled(boolean_t enable);
}

#define super IONVRAMController
OSDefineMetaClassAndStructors(PDEFINVRAM, IONVRAMController);

// firmware variables hold at most a few KB each (OVMF: 8 KB including the name), so the bank is split
#define kChunkSize      0x1000
#define kMaxChunks      16

#define kEFIErrorBit    (1ULL << 63)
#define kEFINotFound    (kEFIErrorBit | 14)

#define kVariableAttrs  (EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS)

enum { kOpLoad, kOpFlush };

static const EFI_GUID kPDNVRAMGuid = { 0x2F6E0C4A, 0x7B1D, 0x4E53, { 0x9C, 0x8A, 0x5D, 0x3E, 0x61, 0xA7, 0xB4, 0x02 } };

// "PDNVRAM0".."PDNVRAMF", one per chunk
static void
chunkName(EFI_CHAR16 name[9], unsigned index)
{
	static const char base[] = "PDNVRAM";

	for (unsigned i = 0; i < 7; i++) {
		name[i] = base[i];
	}
	name[7] = "0123456789ABCDEF"[index & 0xF];
	name[8] = 0;
}

// the five-argument EFI calls: rcx, rdx, r8, r9, then the fifth above the 32-byte shadow space
static kern_return_t
efiCall5(uint64_t func, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t *status)
{
	struct pal_efi_registers regs = { a1, a2, a3, a4, 0 };
	uint64_t stack[6] = { 0, 0, 0, 0, a5, 0 };
	kern_return_t kr;
	boolean_t ints;

	// the firmware is not reentrant and runs on our stack: nothing may interrupt it
	ints = ml_set_interrupts_enabled(FALSE);
	kr = pal_efi_call_in_64bit_mode(func, &regs, stack, sizeof(stack), status);
	ml_set_interrupts_enabled(ints);
	return kr;
}

static uint32_t
adler32(const uint8_t *buf, uint32_t len)
{
	uint32_t a = 1, b = 0;

	for (uint32_t i = 0; i < len; i++) {
		a = (a + buf[i]) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

// the checks IONVRAMCHRPHandler skips: a bank that fails them unserializes past its own end
static bool
validBank(const uint8_t *bank, uint32_t size)
{
	uint32_t sum;
	uint32_t stored;

	if (size < 0x20 || bank[0] != 0x5A) return false;

	sum = bank[0];
	for (unsigned i = 2; i < 16; i++) {
		sum += bank[i];
	}
	while (sum > 0xFF) {
		sum = (sum & 0xFF) + (sum >> 8);
	}
	if (sum != bank[1]) return false;

	stored = bank[16] | (bank[17] << 8) | (bank[18] << 16) | ((uint32_t)bank[19] << 24);
	return stored == adler32(bank + 20, size - 20);
}

PDEFINVRAM *
PDEFINVRAM::withBank(const void *blank, uint32_t size)
{
	PDEFINVRAM *nvram = new PDEFINVRAM;

	if (nvram == NULL) return NULL;
	if (!nvram->init() || size == 0 || size > kChunkSize * kMaxChunks) goto fail;

	nvram->fSize = size;
	nvram->fImage = (uint8_t *)IOMallocZero(size);
	nvram->fStored = (uint8_t *)IOMallocZero(size);
	nvram->fLock = IOLockAlloc();
	nvram->fCall = thread_call_allocate(&PDEFINVRAM::runOp, nvram);
	if (nvram->fImage == NULL || nvram->fStored == NULL || nvram->fLock == NULL || nvram->fCall == NULL) goto fail;

	bcopy(blank, nvram->fImage, size);
	return nvram;

fail:
	nvram->release();
	return NULL;
}

void
PDEFINVRAM::free(void)
{
	if (fCall != NULL) {
		thread_call_cancel_wait(fCall);
		thread_call_free(fCall);
	}
	if (fLock != NULL) IOLockFree(fLock);
	if (fImage != NULL) IOFree(fImage, fSize);
	if (fStored != NULL) IOFree(fStored, fSize);
	super::free();
}

void
PDEFINVRAM::warnOnce(const char *what, uint64_t status)
{
	if (fWarned) return;
	fWarned = true;
	IOLog("PDEFINVRAM: %s (0x%llx), NVRAM changes last until restart\n", what, status);
}

// firmware calls run on a thread-call worker: the firmware may use vector registers, which belong to
// whatever thread is current, and a worker's are nobody's
void
PDEFINVRAM::runOp(thread_call_param_t self, thread_call_param_t)
{
	PDEFINVRAM *nvram = (PDEFINVRAM *)self;

	if (nvram->fOp == kOpLoad) {
		nvram->fRuntime = nvram->loadChunks();
	} else {
		nvram->flushChunks();
	}

	IOLockLock(nvram->fLock);
	nvram->fBusy = false;
	IOLockWakeup(nvram->fLock, &nvram->fBusy, false);
	IOLockUnlock(nvram->fLock);
}

void
PDEFINVRAM::perform(int op)
{
	IOLockLock(fLock);
	while (fBusy) {
		IOLockSleep(fLock, &fBusy, THREAD_UNINT);
	}
	fBusy = true;
	fOp = op;
	thread_call_enter(fCall);
	while (fBusy) {
		IOLockSleep(fLock, &fBusy, THREAD_UNINT);
	}
	IOLockUnlock(fLock);
}

// true when the variable services answer, whether or not a bank was stored
bool
PDEFINVRAM::loadChunks(void)
{
	EFI_RUNTIME_SERVICES_64 *rt = (EFI_RUNTIME_SERVICES_64 *)gPEEFIRuntimeServices;
	uint8_t *copy;
	uint32_t found = 0;
	bool answered = false;

	if (rt == NULL) {
		warnOnce("no EFI runtime services", 0);
		return false;
	}

	copy = (uint8_t *)IOMallocZero(fSize);
	if (copy == NULL) return false;

	for (uint32_t off = 0, i = 0; off < fSize; off += kChunkSize, i++) {
		EFI_CHAR16 name[9];
		uint64_t len = fSize - off < kChunkSize ? fSize - off : kChunkSize;
		uint64_t want = len;
		uint32_t attrs = 0;
		uint64_t status = 0;
		kern_return_t kr;

		chunkName(name, i);
		kr = efiCall5(rt->GetVariable, (uint64_t)name, (uint64_t)&kPDNVRAMGuid, (uint64_t)&attrs,
		    (uint64_t)&len, (uint64_t)(copy + off), &status);
		if (kr != KERN_SUCCESS) {
			warnOnce("EFI GetVariable is not callable", kr);
			break;
		}
		answered = true;
		if (status == kEFINotFound) break;
		if ((status & kEFIErrorBit) || len != want) {
			IOLog("PDEFINVRAM: chunk %u unreadable (0x%llx, %llu bytes)\n", i, status, len);
			break;
		}
		found += (uint32_t)len;
		fPresent |= 1U << i;
	}

	if (found == fSize && validBank(copy, fSize)) {
		bcopy(copy, fImage, fSize);
		bcopy(copy, fStored, fSize);
		IOLog("PDEFINVRAM: loaded %u-byte bank from firmware variables\n", fSize);
	} else if (found != 0) {
		// the first sync rewrites every chunk
		fPresent = 0;
		IOLog("PDEFINVRAM: stored bank invalid, starting blank\n");
	}

	IOFree(copy, fSize);
	return answered;
}

void
PDEFINVRAM::flushChunks(void)
{
	EFI_RUNTIME_SERVICES_64 *rt = (EFI_RUNTIME_SERVICES_64 *)gPEEFIRuntimeServices;
	uint32_t written = 0;

	for (uint32_t off = 0, i = 0; off < fSize; off += kChunkSize, i++) {
		EFI_CHAR16 name[9];
		uint32_t len = fSize - off < kChunkSize ? fSize - off : kChunkSize;
		uint64_t status = 0;
		kern_return_t kr;

		// a chunk the firmware doesn't hold yet goes out even when it matches the zeroed shadow
		if ((fPresent & (1U << i)) && bcmp(fImage + off, fStored + off, len) == 0) continue;

		chunkName(name, i);
		kr = efiCall5(rt->SetVariable, (uint64_t)name, (uint64_t)&kPDNVRAMGuid, kVariableAttrs, len,
		    (uint64_t)(fImage + off), &status);
		if (kr != KERN_SUCCESS || (status & kEFIErrorBit)) {
			warnOnce("EFI SetVariable failed", kr != KERN_SUCCESS ? kr : status);
			continue;
		}
		bcopy(fImage + off, fStored + off, len);
		fPresent |= 1U << i;
		written += len;
	}
	if (written != 0 && !fFlushed) {
		fFlushed = true;
		IOLog("PDEFINVRAM: stored %u bytes in firmware variables\n", written);
	}
}

bool
PDEFINVRAM::load(void)
{
	perform(kOpLoad);
	return fRuntime && bcmp(fImage, fStored, fSize) == 0;
}

void
PDEFINVRAM::sync(void)
{
	if (fRuntime) perform(kOpFlush);
}

IOReturn
PDEFINVRAM::select(uint32_t bank)
{
	return bank == 0 ? kIOReturnSuccess : kIOReturnBadArgument;
}

IOReturn
PDEFINVRAM::eraseBank(void)
{
	bzero(fImage, fSize);
	return kIOReturnSuccess;
}

IOReturn
PDEFINVRAM::read(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	if (offset > fSize || length > fSize - offset) return kIOReturnBadArgument;
	bcopy(fImage + offset, buffer, length);
	return kIOReturnSuccess;
}

IOReturn
PDEFINVRAM::write(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	if (offset > fSize || length > fSize - offset) return kIOReturnBadArgument;
	bcopy(buffer, fImage + offset, length);
	return kIOReturnSuccess;
}
