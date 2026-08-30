#ifndef _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_
#define _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>

#if DEVELOPMENT || DEBUG

class TestIOServiceUserNotification : public IOService {
	OSDeclareDefaultStructors(TestIOServiceUserNotification);

public:
	virtual bool start(IOService *provider) override;
	virtual void free() override;

	void registerUserNotification(OSObject * notification);
	void trimUserNotificationsLocked(void);
	size_t getUserNotificationLeakCount(void);

private:
	OSArray * fUserNotifications;
	IOLock * fLock;
};

class TestIOServiceUserNotificationUserClient : public IOUserClient {
	OSDeclareDefaultStructors(TestIOServiceUserNotificationUserClient);

public:
	bool start(IOService * provider) override;
	IONotifier * registerInterest(const OSSymbol * typeOfInterest,
	    IOServiceInterestHandler handler,
	    void * target, void * ref = NULL) override;
	virtual IOReturn clientClose() override;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments * args,
	    IOExternalMethodDispatch * dispatch, OSObject * target, void * reference) override;
private:
	TestIOServiceUserNotification * fProvider;
};

#endif /* DEVELOPMENT || DEBUG */

#endif /* _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_ */
