// DiskImages2's kernel side for RAM images: diskimagescontroller creates a device through
// AppleDiskImagesController, diskimagesiod hands it the memory that backs the image

#ifndef _PD_DISK_IMAGES2_H
#define _PD_DISK_IMAGES2_H

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/storage/IOBlockStorageDevice.h>

class AppleDiskImagesController : public IOService
{
	OSDeclareDefaultStructors(AppleDiskImagesController);

public:
	virtual bool start(IOService *provider) override;
};

class AppleDiskImageDevice : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(AppleDiskImageDevice);

public:
	bool initWithParams(UInt64 blocks, UInt32 blockSize, bool ram, UInt32 uid, UInt32 gid, UInt32 mode,
	    const uint8_t *instance, const char *url);
	// the client's memory becomes the disk, then the device is published
	IOReturn connectMemory(task_t task, mach_vm_address_t base, mach_vm_size_t length, bool readOnly);
	void disconnectMemory(void);

	virtual void free(void) override;
	virtual IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks,
	    IOStorageAttributes *attributes, IOStorageCompletion *completion) override;
	virtual IOReturn doSynchronize(UInt64 block, UInt64 nblks,
	    IOStorageSynchronizeOptions options = 0) override;
	virtual IOReturn doUnmap(IOBlockStorageDeviceExtent *extents, UInt32 extentsCount,
	    IOStorageUnmapOptions options = 0) override;
	virtual IOReturn doEjectMedia(void) override;
	virtual IOReturn doFormatMedia(UInt64 byteCapacity) override;
	virtual UInt32 doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const override;
	virtual char *getVendorString(void) override;
	virtual char *getProductString(void) override;
	virtual char *getRevisionString(void) override;
	virtual char *getAdditionalDeviceInfoString(void) override;
	virtual IOReturn reportBlockSize(UInt64 *blockSize) override;
	virtual IOReturn reportEjectability(bool *isEjectable) override;
	virtual IOReturn reportMaxValidBlock(UInt64 *maxBlock) override;
	virtual IOReturn reportMediaState(bool *mediaPresent, bool *changedState = 0) override;
	virtual IOReturn reportRemovability(bool *isRemovable) override;
	virtual IOReturn reportWriteProtection(bool *isWriteProtected) override;

private:
	UInt64 fBlocks;
	UInt32 fBlockSize;
	bool fReadOnly;
	IOMemoryDescriptor *fMemory;
	IOMemoryMap *fMap;
	IOLock *fLock;
};

// diskimagescontroller's client on the controller, sel 0 CreateDevice
class DIDeviceCreatorUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(DIDeviceCreatorUserClient);

public:
	virtual IOReturn clientClose(void) override;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
};

// diskimagesiod's client on a device: sel 0 Connect, 1 Complete, 2 AllocPortsArray
class DIDeviceIOUserClient : public IOUserClient
{
	OSDeclareDefaultStructors(DIDeviceIOUserClient);

public:
	virtual bool initWithTask(task_t owningTask, void *securityID, UInt32 type) override;
	virtual IOReturn clientClose(void) override;
	virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon) override;
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;

private:
	task_t fTask;
};

#endif
