/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "PDEFIVariables.h"
#include "PDEFINVRAM.h"

#include <IOKit/IOLib.h>
#include <pexpert/i386/efi.h>

extern "C" {
extern void *gPEEFIRuntimeServices;
}

#define kEFIErrorBit        (1ULL << 63)
#define kEFIBufferTooSmall  (kEFIErrorBit | 5)
#define kEFINotFound        (kEFIErrorBit | 14)
#define kEFIWriteProtected  (kEFIErrorBit | 8)

OSDefineMetaClassAndStructors(PDEFIVariables, IOService);
OSDefineMetaClassAndStructors(PDEFIVariablesUserClient, IOUserClient);

bool
PDEFIVariables::start(IOService *provider)
{
	if (gPEEFIRuntimeServices == NULL || !IOService::start(provider)) return false;

	fLock = IOLockAlloc();
	fCall = thread_call_allocate(&PDEFIVariables::runOp, this);
	if (fLock == NULL || fCall == NULL) return false;

	setProperty("IOUserClientClass", "PDEFIVariablesUserClient");
	registerService();
	return true;
}

void
PDEFIVariables::free(void)
{
	if (fCall != NULL) {
		thread_call_cancel_wait(fCall);
		thread_call_free(fCall);
	}
	if (fLock != NULL) IOLockFree(fLock);
	IOService::free();
}

// firmware calls run on a thread-call worker, as in PDEFINVRAM: the firmware may use vector registers
void
PDEFIVariables::runOp(thread_call_param_t self, thread_call_param_t)
{
	PDEFIVariables *vars = (PDEFIVariables *)self;

	vars->fFn(vars->fArg);
	IOLockLock(vars->fLock);
	vars->fBusy = false;
	IOLockWakeup(vars->fLock, &vars->fBusy, false);
	IOLockUnlock(vars->fLock);
}

struct pd_efivar_op {
	uint32_t                  selector;
	const pd_efivar_request  *in;
	pd_efivar_request        *out;
	uint64_t                  status;
	kern_return_t             kr;
};

// names are ASCII both ways, which is all boot entries use
static bool
toUCS2(const char *in, EFI_CHAR16 *out, size_t max)
{
	size_t i;

	for (i = 0; i + 1 < max && in[i]; i++) {
		if ((uint8_t)in[i] > 0x7F) return false;
		out[i] = (EFI_CHAR16)in[i];
	}
	out[i] = 0;
	return in[i] == 0;
}

static void
fromUCS2(const EFI_CHAR16 *in, char *out, size_t max)
{
	size_t i;

	for (i = 0; i + 1 < max && in[i]; i++) {
		out[i] = in[i] < 0x80 ? (char)in[i] : '?';
	}
	out[i] = 0;
}

static void
doOp(void *arg)
{
	pd_efivar_op *op = (pd_efivar_op *)arg;
	EFI_RUNTIME_SERVICES_64 *rt = (EFI_RUNTIME_SERVICES_64 *)gPEEFIRuntimeServices;
	EFI_CHAR16 name[kPDEFIVarNameMax];
	EFI_GUID guid;

	bcopy(op->in->guid, &guid, sizeof(guid));
	if (!toUCS2(op->in->name, name, kPDEFIVarNameMax)) {
		op->kr = KERN_INVALID_ARGUMENT;
		return;
	}

	switch (op->selector) {
	case kPDEFIVarGet: {
		uint32_t attrs = 0;
		uint64_t size = kPDEFIVarDataMax;

		op->kr = efiCall5(rt->GetVariable, (uint64_t)name, (uint64_t)&guid, (uint64_t)&attrs,
		    (uint64_t)&size, (uint64_t)op->out->data, &op->status);
		op->out->attrs = attrs;
		op->out->size = (uint32_t)size;
		break;
	}
	case kPDEFIVarSet:
		op->kr = efiCall5(rt->SetVariable, (uint64_t)name, (uint64_t)&guid, op->in->attrs,
		    op->in->size, (uint64_t)op->in->data, &op->status);
		break;
	case kPDEFIVarNext: {
		uint64_t size = sizeof(name);

		op->kr = efiCall5(rt->GetNextVariableName, (uint64_t)&size, (uint64_t)name, (uint64_t)&guid,
		    0, 0, &op->status);
		bcopy(&guid, op->out->guid, sizeof(guid));
		fromUCS2(name, op->out->name, kPDEFIVarNameMax);
		break;
	}
	}
}

IOReturn
PDEFIVariables::request(uint32_t selector, const pd_efivar_request *in, pd_efivar_request *out)
{
	pd_efivar_op op = { selector, in, out, 0, KERN_SUCCESS };

	if (selector >= kPDEFIVarCount) return kIOReturnBadArgument;
	if (selector == kPDEFIVarSet && in->size > kPDEFIVarDataMax) return kIOReturnBadArgument;

	IOLockLock(fLock);
	while (fBusy) {
		IOLockSleep(fLock, &fBusy, THREAD_UNINT);
	}
	fBusy = true;
	fFn = doOp;
	fArg = &op;
	thread_call_enter(fCall);
	while (fBusy) {
		IOLockSleep(fLock, &fBusy, THREAD_UNINT);
	}
	IOLockUnlock(fLock);

	if (op.kr != KERN_SUCCESS) return op.kr == KERN_INVALID_ARGUMENT ? kIOReturnBadArgument : kIOReturnError;
	if (!(op.status & kEFIErrorBit)) return kIOReturnSuccess;
	if (op.status == kEFINotFound) return kIOReturnNotFound;
	if (op.status == kEFIBufferTooSmall) return kIOReturnNoSpace;
	if (op.status == kEFIWriteProtected) return kIOReturnNotWritable;
	return kIOReturnError;
}

// boot entries change what the machine runs, so only an administrator gets a connection
bool
PDEFIVariablesUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	if (clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator) != kIOReturnSuccess) return false;
	return IOUserClient::initWithTask(owningTask, securityID, type);
}

bool
PDEFIVariablesUserClient::start(IOService *provider)
{
	fOwner = OSDynamicCast(PDEFIVariables, provider);
	return fOwner != NULL && IOUserClient::start(provider);
}

IOReturn
PDEFIVariablesUserClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

IOReturn
PDEFIVariablesUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *, OSObject *, void *)
{
	pd_efivar_request *out;
	IOReturn ret;

	if (selector >= kPDEFIVarCount || args->structureInputSize != sizeof(pd_efivar_request)) return kIOReturnBadArgument;
	if (selector != kPDEFIVarSet && args->structureOutputSize != sizeof(pd_efivar_request)) return kIOReturnBadArgument;

	out = (pd_efivar_request *)IOMallocZero(sizeof(*out));
	if (out == NULL) return kIOReturnNoMemory;
	ret = fOwner->request(selector, (const pd_efivar_request *)args->structureInput, out);
	if (ret == kIOReturnSuccess && selector != kPDEFIVarSet)
		bcopy(out, args->structureOutput, sizeof(*out));
	IOFree(out, sizeof(*out));
	return ret;
}
