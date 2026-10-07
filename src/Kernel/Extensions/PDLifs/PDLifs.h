// the kernel half of FSKit (lifs): fskitd hands it the port LiveFS requests go to and extensions
// get their volume port from it, but the LiveFS VFS is not here, so mounts are refused

#ifndef _PD_LIFS_H
#define _PD_LIFS_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOLocks.h>

class com_apple_filesystems_lifs : public IOService
{
	OSDeclareDefaultStructors(com_apple_filesystems_lifs);

public:
	virtual bool start(IOService *provider) override;
};

// one per fskitd and per FSKit extension, all opened with type 0
class AppleLIFSUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(AppleLIFSUserClient);

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) override;
	virtual void free(void) override;
	virtual IOReturn clientClose(void) override;
	virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon) override;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;

private:
	void dropMainPort(void);
	IOReturn createVolumePort(IOExternalMethodArguments *args);
	IOReturn configureUserClient(IOExternalMethodArguments *args);

	task_t fTask;
	int fPid;
	char fName[32];
	mach_port_t fMainPort;
	IOLock *fLock;
};

#endif
