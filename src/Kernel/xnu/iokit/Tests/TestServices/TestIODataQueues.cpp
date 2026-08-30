#include <IOKit/IOKitKeys.h>
#include "TestIODataQueues.h"

#if DEVELOPMENT || DEBUG

OSDefineMetaClassAndStructors(TestIODataQueues, IOService);

OSDefineMetaClassAndStructors(TestIODataQueuesUserClient, IOUserClient2022);

bool
TestIODataQueues::start(IOService * provider)
{
	OSString * str = OSString::withCStringNoCopy("TestIODataQueuesUserClient");
	bool ok = IOService::start(provider);
	if (ok && str != NULL) {
		IOReturn ret;
		ret = IOCircularDataQueueCreateWithEntries(kIOCircularDataQueueCreateProducer, 128, 16, &fCDQueue);
		assert(kIOReturnSuccess == ret);
		ret = IOCircularDataQueueEnqueue(fCDQueue, "hello", sizeof("hello"));
		assert(kIOReturnSuccess == ret);

		setProperty(gIOUserClientClassKey, str);
		registerService();
	}
	OSSafeReleaseNULL(str);
	return ok;
}


IOReturn
TestIODataQueuesUserClient::clientClose()
{
	if (!isInactive()) {
		terminate();
	}
	return kIOReturnSuccess;
}

bool
TestIODataQueuesUserClient::start(IOService * provider)
{
	bool ok = IOUserClient2022::start(provider);
	if (!ok) {
		return false;
	}
	fTestIODataQueues = OSRequiredCast(TestIODataQueues, provider);

	setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
	setProperty(kIOUserClientDefaultLockingSetPropertiesKey, kOSBooleanTrue);
	setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanTrue);
	setProperty(kIOUserClientEntitlementsKey, "com.apple.iokit.TestIODataQueues");

	return true;
}

IOReturn
TestIODataQueuesUserClient::clientMemoryForType(UInt32 type,
    IOOptionBits * koptions,
    IOMemoryDescriptor ** kmemory)
{
	IOReturn ret = kIOReturnSuccess;

	*kmemory = IOCircularDataQueueCopyMemoryDescriptor(fTestIODataQueues->fCDQueue);
	assert(*kmemory);
	*koptions = kIOMapReadOnly;

	return ret;
}

IOReturn
TestIODataQueuesUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
	static const IOExternalMethodDispatch2022 dispatchArray[] = {
	};

	return dispatchExternalMethod(selector, args, dispatchArray, sizeof(dispatchArray) / sizeof(dispatchArray[0]), this, NULL);
}

#endif /* DEVELOPMENT || DEBUG */
