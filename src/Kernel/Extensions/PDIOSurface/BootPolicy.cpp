// BootPolicy, the SEP boot policy stand-in

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
