#ifndef _PUREDARWIN_PDPOWERVRUSERCLIENT_H
#define _PUREDARWIN_PDPOWERVRUSERCLIENT_H

#include <IOKit/IOUserClient.h>

// one open of the gpu, the drm file: its handle tables live in a pvr_file

class PDPowerVR;
struct pvr_file;
struct pvr_gem_object;

class PDPowerVRUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(PDPowerVRUserClient)

public:
	bool initWithTask(task_t owningTask, void *securityToken, UInt32 type) APPLE_KEXT_OVERRIDE;
	bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
	IOReturn clientClose(void) APPLE_KEXT_OVERRIDE;
	void free(void) APPLE_KEXT_OVERRIDE;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) APPLE_KEXT_OVERRIDE;
	IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
	    IOMemoryDescriptor **memory) APPLE_KEXT_OVERRIDE;

private:
	PDPowerVR       *fGPU;
	struct pvr_file *fFile;
	task_t           fTask;
	// the buffer this client last put on the display, held until it is replaced
	struct pvr_gem_object *fScanout;

	IOReturn scanoutMethod(uint32_t selector, IOExternalMethodArguments *args);
	IOReturn flip(uint32_t handle, uint64_t offset, uint32_t pitch, uint64_t *seq, uint64_t *ns);
};

#endif
