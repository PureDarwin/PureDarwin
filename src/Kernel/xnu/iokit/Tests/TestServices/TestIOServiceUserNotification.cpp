#if DEVELOPMENT || DEBUG
#include "TestIOServiceUserNotification.h"
#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOKitServer.h>
#include <kern/ipc_kobject.h>
#include "../../Kernel/IOServicePrivate.h"

OSDefineMetaClassAndStructors(TestIOServiceUserNotification, IOService);

OSDefineMetaClassAndStructors(TestIOServiceUserNotificationUserClient, IOUserClient);

bool
TestIOServiceUserNotification::start(IOService * provider)
{
	OSString * str = OSString::withCStringNoCopy("TestIOServiceUserNotificationUserClient");
	bool ret = IOService::start(provider);
	if (ret && str != NULL) {
		setProperty(gIOUserClientClassKey, str);
		fUserNotifications = OSArray::withCapacity(1);
		fLock = IOLockAlloc();
		registerService();
	}
	OSSafeReleaseNULL(str);
	return ret;
}

void
TestIOServiceUserNotification::free()
{
	if (fLock) {
		IOLockFree(fLock);
		fLock = NULL;
	}
	OSSafeReleaseNULL(fUserNotifications);
	IOService::free();
}

void
TestIOServiceUserNotification::registerUserNotification(OSObject * notification)
{
	IOLockLock(fLock);
	// Proactively trim the list to avoid holding too many objects
	trimUserNotificationsLocked();
	assert(fUserNotifications->getNextIndexOfObject(notification, 0) == -1);
	fUserNotifications->setObject(notification);
	IOLockUnlock(fLock);
}

void
TestIOServiceUserNotification::trimUserNotificationsLocked()
{
	OSArray * remaining = OSArray::withCapacity(1);
	if (!remaining) {
		return;
	}
	fUserNotifications->iterateObjects(^(OSObject * obj) {
		if (obj->getRetainCount() != 1) {
		        remaining->setObject(obj);
		}
		return false;
	});
	fUserNotifications->release();
	fUserNotifications = remaining;
}

size_t
TestIOServiceUserNotification::getUserNotificationLeakCount()
{
	size_t count = 0;
	IOLockLock(fLock);
	trimUserNotificationsLocked();
	count = fUserNotifications->getCount();
	IOLockUnlock(fLock);
	return count;
}

bool
TestIOServiceUserNotificationUserClient::start(IOService * provider)
{
	if (!IOUserClient::start(provider)) {
		return false;
	}
	fProvider = OSDynamicCast(TestIOServiceUserNotification, provider);
	assert(fProvider);
	return true;
}

IONotifier *
TestIOServiceUserNotificationUserClient::registerInterest(const OSSymbol * typeOfInterest,
    IOServiceInterestHandler handler,
    void * target,
    void * ref)
{
	IONotifier * notify = IOService::registerInterest(typeOfInterest, handler, target, ref);

	// No straightforward way to make sure registerInterest is called from the test app
	// Could check if handler is _ZN32IOServiceMessageUserNotification8_handlerEPvS0_jP9IOServiceS0_m
	// But still cannot rule out other user process regisering interest
	OSObject * obj = (OSObject *)target;
	// Just panic the system if target isn't OSObject
	fProvider->registerUserNotification(obj);

	return notify;
}

IOReturn
TestIOServiceUserNotificationUserClient::clientClose()
{
	if (!isInactive()) {
		terminate();
	}
	return kIOReturnSuccess;
}

IOReturn
TestIOServiceUserNotificationUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments * args,
    IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
	if (selector == 0) {
		registerService();
	} else if (selector == 1 && args->scalarOutputCount >= 1) {
		args->scalarOutput[0] = fProvider->getUserNotificationLeakCount();
	}
	return kIOReturnSuccess;
}

#endif /* DEVELOPMENT || DEBUG */
