// PDShaderBridge, the transport for the userspace shader executor

#include "PDIOSurface.h"

OSDefineMetaClassAndStructors(PDShaderBridge, IOService);

void
PDShaderBridge::publish(IOService *owner)
{
	PDShaderBridge *bridge = OSTypeAlloc(PDShaderBridge);

	if (bridge != NULL && bridge->init() && bridge->attach(owner)) {
		if (!bridge->start(owner)) bridge->detach(owner);
	}
	OSSafeReleaseNULL(bridge);
}

// the doorbells' lock, separate from the guard that serializes clients on the bridge
IOLock *sBridgeBell;

bool
PDShaderBridge::start(IOService *provider)
{
	if (!IOService::start(provider)) return false;
	setName("PDShaderBridge");
	if (sBridgeBell == NULL)
		sBridgeBell = IOLockAlloc();
	registerService();
	return true;
}

IOReturn
PDShaderBridge::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOSurfaceRootUserClient::open(this, owningTask, securityID, type, handler);
}
