// BootPolicy.cpp - BootPolicy, the SEP boot policy stand-in
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include "PDIOSurface.h"

OSDefineMetaClassAndStructors(BootPolicy, IOService);

bool
BootPolicy::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	setName("BootPolicy");
	setProperty("IOUserClientClass", "BootPolicyUserClient");
	registerService();
	return true;
}

IOReturn
BootPolicy::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}
