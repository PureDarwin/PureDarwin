// PDEventFDService.cpp - starts /dev/eventfd once BSD is up, from an IOResources match in the boot KC
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>

extern "C" kern_return_t pd_eventfd_start(kmod_info_t *ki, void *data);
extern "C" kern_return_t pd_eventfd_stop(kmod_info_t *ki, void *data);

class PDEventFD : public IOService
{
	OSDeclareDefaultStructors(PDEventFD);

public:
	bool start(IOService *provider) override;
	void stop(IOService *provider) override;
};

OSDefineMetaClassAndStructors(PDEventFD, IOService);

bool
PDEventFD::start(IOService *provider)
{
	if (!IOService::start(provider) || pd_eventfd_start(NULL, NULL) != KERN_SUCCESS)
		return false;
	registerService();
	return true;
}

void
PDEventFD::stop(IOService *provider)
{
	pd_eventfd_stop(NULL, NULL);
	IOService::stop(provider);
}
