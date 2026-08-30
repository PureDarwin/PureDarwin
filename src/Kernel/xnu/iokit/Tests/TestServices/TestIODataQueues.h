#ifndef _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_
#define _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOCircularDataQueue.h>

#if DEVELOPMENT || DEBUG

class TestIODataQueues : public IOService {
	OSDeclareDefaultStructors(TestIODataQueues);
	friend class TestIODataQueuesUserClient;

	IOCircularDataQueue * fCDQueue;

public:
	virtual bool start(IOService *provider) override;
};

class TestIODataQueuesUserClient : public IOUserClient2022 {
	OSDeclareDefaultStructors(TestIODataQueuesUserClient);

	TestIODataQueues * fTestIODataQueues;

public:
	virtual bool start(IOService * provider) override;
	virtual IOReturn clientClose() override;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args) override;
	virtual IOReturn clientMemoryForType(UInt32 type,
	    IOOptionBits * koptions,
	    IOMemoryDescriptor ** kmemory) override;
};

#endif /* DEVELOPMENT || DEBUG */

#endif /* _IOKIT_TESTIOSERVICEUSERNOTIFICATION_H_ */
