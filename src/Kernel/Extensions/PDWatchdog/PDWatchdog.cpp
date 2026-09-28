// IOWatchdog service for watchdogd. The client interface is being learned by observation:
// every external method call is logged and answered with success

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLib.h>

class IOWatchdog : public IOService
{
	OSDeclareDefaultStructors(IOWatchdog);

public:
	virtual bool start(IOService *provider) override;
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

// watchdogd also matches an IOService with IOPMUPrimary=1 (a PMU on real hardware)
class PDPMUPrimary : public IOService
{
	OSDeclareDefaultStructors(PDPMUPrimary);

public:
	virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
	    IOUserClient **handler) override;
};

class IOWatchdogUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(IOWatchdogUserClient);

public:
	static IOReturn open(IOService *provider, task_t owningTask, void *securityID,
	    UInt32 type, IOUserClient **handler);

	virtual IOReturn clientClose(void) override;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
};

OSDefineMetaClassAndStructors(IOWatchdog, IOService);
OSDefineMetaClassAndStructors(PDPMUPrimary, IOService);
OSDefineMetaClassAndStructors(IOWatchdogUserClient, IOUserClient);

bool
IOWatchdog::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	registerService();

	PDPMUPrimary *pmu = OSTypeAlloc(PDPMUPrimary);
	if (pmu != NULL && pmu->init()) {
		pmu->setProperty("IOPMUPrimary", 1ULL, 32);
		if (pmu->attach(this) && pmu->start(this))
			pmu->registerService();
	}
	OSSafeReleaseNULL(pmu);
	return true;
}

IOReturn
IOWatchdog::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOWatchdogUserClient::open(this, owningTask, securityID, type, handler);
}

IOReturn
PDPMUPrimary::newUserClient(task_t owningTask, void *securityID, UInt32 type,
    IOUserClient **handler)
{
	return IOWatchdogUserClient::open(this, owningTask, securityID, type, handler);
}

IOReturn
IOWatchdogUserClient::open(IOService *provider, task_t owningTask, void *securityID,
    UInt32 type, IOUserClient **handler)
{
	IOWatchdogUserClient *uc = OSTypeAlloc(IOWatchdogUserClient);

	if (uc == NULL || !uc->initWithTask(owningTask, securityID, type)) {
		OSSafeReleaseNULL(uc);
		return kIOReturnNoMemory;
	}
	if (!uc->attach(provider)) {
		uc->release();
		return kIOReturnError;
	}
	if (!uc->start(provider)) {
		uc->detach(provider);
		uc->release();
		return kIOReturnError;
	}
	*handler = uc;
	return kIOReturnSuccess;
}

IOReturn
IOWatchdogUserClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

IOReturn
IOWatchdogUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	(void)selector;
	(void)dispatch;
	(void)target;
	(void)reference;
	// every watchdog call succeeds with zeroed outputs
	for (uint32_t i = 0; i < args->scalarOutputCount; i++) {
		args->scalarOutput[i] = 0;
	}
	if (args->structureOutput != NULL && args->structureOutputSize > 0) {
		bzero(args->structureOutput, args->structureOutputSize);
	}
	return kIOReturnSuccess;
}
