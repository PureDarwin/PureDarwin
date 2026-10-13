/*
 * Copyright (C) 2026, PureDarwin Project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef _PDEFIVARIABLES_H
#define _PDEFIVARIABLES_H

#include <stdint.h>

// what pdbless sends and gets back, one variable at a time, pdbless keeps the same layout
#define kPDEFIVarNameMax  128
// keeps a request under 4096 bytes, past which IOKit passes structs as memory descriptors instead
#define kPDEFIVarDataMax  3840

enum {
	kPDEFIVarGet = 0,   // in: guid, name. out: attrs, size, data
	kPDEFIVarSet = 1,   // in: guid, name, attrs, size, data. size 0 deletes
	kPDEFIVarNext = 2,  // in: guid, name ("" to start). out: the next guid and name, kIOReturnNotFound at the end
	kPDEFIVarCount
};

struct pd_efivar_request {
	uint8_t  guid[16];
	char     name[kPDEFIVarNameMax];
	uint32_t attrs;
	uint32_t size;
	uint8_t  data[kPDEFIVarDataMax];
};

#ifdef KERNEL
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <kern/thread_call.h>

// root-only access to firmware variables, for boot entries
class PDEFIVariables : public IOService {
	OSDeclareDefaultStructors(PDEFIVariables)

private:
	IOLock       *fLock;
	thread_call_t fCall;
	bool          fBusy;
	void        (*fFn)(void *);
	void         *fArg;

	static void runOp(thread_call_param_t self, thread_call_param_t);

public:
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual void free(void) APPLE_KEXT_OVERRIDE;

	IOReturn request(uint32_t selector, const pd_efivar_request *in, pd_efivar_request *out);
};

class PDEFIVariablesUserClient : public IOUserClient {
	OSDeclareDefaultStructors(PDEFIVariablesUserClient)

private:
	PDEFIVariables *fOwner;

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) APPLE_KEXT_OVERRIDE;
	virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	virtual IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;
};
#endif

#endif
