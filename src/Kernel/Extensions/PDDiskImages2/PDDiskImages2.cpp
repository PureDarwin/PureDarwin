#include "PDDiskImages2.h"

#include <IOKit/IOKitKeys.h>
#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>

OSDefineMetaClassAndStructors(AppleDiskImagesController, IOService);
OSDefineMetaClassAndStructors(AppleDiskImageDevice, IOBlockStorageDevice);
OSDefineMetaClassAndStructors(DIDeviceCreatorUserClient, IOUserClient);
OSDefineMetaClassAndStructors(DIDeviceIOUserClient, IOUserClient);

// Request layouts (version 9) as DiskImages2 524.160.11 sends them
enum {
	kDICreateSize = 2112,
	kDICreateBlocks = 0x08,
	kDICreateBlockSize = 0x10,
	kDICreateLocation = 0x17,       // 2 RAM, 1 file
	kDICreateMode = 0x18,
	kDICreateUID = 0x1c,
	kDICreateGID = 0x20,
	kDICreateInstance = 0x28,
	kDICreateURL = 0x40,
	kDIConnectSize = 1088,
	kDIConnectBase = 0x08,
	kDIConnectLength = 0x10,
	kDIConnectReadOnly = 0x3e,
	kDIMaxTransfer = 0x200000,
};

bool
AppleDiskImagesController::start(IOService *provider)
{
	if (!IOService::start(provider)) {
		return false;
	}
	registerService();
	return true;
}

bool
AppleDiskImageDevice::initWithParams(UInt64 blocks, UInt32 blockSize, bool ram, UInt32 uid, UInt32 gid,
    UInt32 mode, const uint8_t *instance, const char *url)
{
	char uuid[37];
	OSDictionary *proto;

	if (blocks == 0 || blockSize == 0 || !init(NULL)) {
		return false;
	}
	fBlocks = blocks;
	fBlockSize = blockSize;
	fLock = IOLockAlloc();
	if (fLock == NULL) {
		return false;
	}

	snprintf(uuid, sizeof(uuid), "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
	    instance[0], instance[1], instance[2], instance[3], instance[4], instance[5], instance[6], instance[7],
	    instance[8], instance[9], instance[10], instance[11], instance[12], instance[13], instance[14], instance[15]);
	setProperty("DiskImageURL", url);
	setProperty("InstanceID", uuid);
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
	setProperty("IOUserClientClass", "DIDeviceIOUserClient");
	setProperty("owner-uid", uid, 32);
	setProperty("owner-gid", gid, 32);
	setProperty("owner-mode", mode, 32);
	setProperty("UsingDirectIO", kOSBooleanTrue);
	setProperty("sparse-backend", kOSBooleanFalse);
	setProperty(kIOMaximumByteCountReadKey, (UInt64)kDIMaxTransfer, 64);
	setProperty(kIOMaximumByteCountWriteKey, (UInt64)kDIMaxTransfer, 64);
	setProperty(kIOMaximumBlockCountReadKey, (UInt64)(kDIMaxTransfer / blockSize), 64);
	setProperty(kIOMaximumBlockCountWriteKey, (UInt64)(kDIMaxTransfer / blockSize), 64);
	setProperty(kIOMinimumSegmentAlignmentByteCountKey, 4ULL, 64);

	proto = OSDictionary::withCapacity(2);
	if (proto != NULL) {
		OSString *where = OSString::withCString(ram ? "RAM" : "File");

		proto->setObject(kIOPropertyPhysicalInterconnectTypeKey, OSString::withCStringNoCopy("Virtual Interface"));
		proto->setObject(kIOPropertyPhysicalInterconnectLocationKey, where);
		setProperty(kIOPropertyProtocolCharacteristicsKey, proto);
		OSSafeReleaseNULL(where);
		proto->release();
	}
	setName("AppleDiskImageDevice");
	return true;
}

IOReturn
AppleDiskImageDevice::connectMemory(task_t task, mach_vm_address_t base, mach_vm_size_t length, bool readOnly)
{
	IOMemoryDescriptor *md;
	IOMemoryMap *map;

	if (length < fBlocks * fBlockSize) {
		return kIOReturnBadArgument;
	}
	IOLockLock(fLock);
	if (fMemory != NULL) {
		IOLockUnlock(fLock);
		return kIOReturnStillOpen;
	}
	IOLockUnlock(fLock);

	md = IOMemoryDescriptor::withAddressRange(base, fBlocks * fBlockSize, kIODirectionOutIn, task);
	if (md == NULL) {
		return kIOReturnNoMemory;
	}
	if (md->prepare() != kIOReturnSuccess) {
		md->release();
		return kIOReturnVMError;
	}
	map = md->createMappingInTask(kernel_task, 0, kIOMapAnywhere);
	if (map == NULL) {
		md->complete();
		md->release();
		return kIOReturnVMError;
	}

	IOLockLock(fLock);
	fMemory = md;
	fMap = map;
	fReadOnly = readOnly;
	IOLockUnlock(fLock);

	setProperty("RootDeviceEntryID", getRegistryEntryID(), 64);
	registerService();
	return kIOReturnSuccess;
}

void
AppleDiskImageDevice::disconnectMemory(void)
{
	IOMemoryDescriptor *md;
	IOMemoryMap *map;

	IOLockLock(fLock);
	md = fMemory;
	map = fMap;
	fMemory = NULL;
	fMap = NULL;
	IOLockUnlock(fLock);

	OSSafeReleaseNULL(map);
	if (md != NULL) {
		md->complete();
		md->release();
	}
}

void
AppleDiskImageDevice::free(void)
{
	if (fLock != NULL) {
		disconnectMemory();
		IOLockFree(fLock);
		fLock = NULL;
	}
	IOBlockStorageDevice::free();
}

IOReturn
AppleDiskImageDevice::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks,
    IOStorageAttributes *attributes, IOStorageCompletion *completion)
{
	IOReturn ret = kIOReturnSuccess;
	UInt64 bytes = nblks * fBlockSize;
	IOByteCount done;
	uint8_t *disk;

	(void)attributes;
	IOLockLock(fLock);
	if (fMap == NULL) {
		ret = kIOReturnNotAttached;
	} else if (block > fBlocks || nblks > fBlocks - block) {
		ret = kIOReturnBadArgument;
	} else if ((buffer->getDirection() & kIODirectionOut) && fReadOnly) {
		ret = kIOReturnNotWritable;
	} else if (buffer->prepare() != kIOReturnSuccess) {
		ret = kIOReturnVMError;
	} else {
		disk = (uint8_t *)fMap->getVirtualAddress() + block * fBlockSize;
		// a write's buffer is the source, a read's the destination
		if (buffer->getDirection() & kIODirectionOut) {
			done = buffer->readBytes(0, disk, bytes);
		} else {
			done = buffer->writeBytes(0, disk, bytes);
		}
		buffer->complete();
		if (done != bytes) {
			ret = kIOReturnIOError;
		}
	}
	IOLockUnlock(fLock);

	IOStorage::complete(completion, ret, ret == kIOReturnSuccess ? bytes : 0);
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::doSynchronize(UInt64 block, UInt64 nblks, IOStorageSynchronizeOptions options)
{
	(void)block; (void)nblks; (void)options;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::doUnmap(IOBlockStorageDeviceExtent *extents, UInt32 extentsCount, IOStorageUnmapOptions options)
{
	(void)extents; (void)extentsCount; (void)options;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::doEjectMedia(void)
{
	// diskimagesiod waits for the device to terminate, then closes its client and exits
	terminate();
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
AppleDiskImageDevice::doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount > 0) {
		capacities[0] = fBlocks * fBlockSize;
	}
	return 1;
}

char *AppleDiskImageDevice::getVendorString(void) { return (char *)"Apple"; }
char *AppleDiskImageDevice::getProductString(void) { return (char *)"Disk Image"; }
char *AppleDiskImageDevice::getRevisionString(void) { return (char *)"1.0"; }
char *AppleDiskImageDevice::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
AppleDiskImageDevice::reportBlockSize(UInt64 *blockSize)
{
	*blockSize = fBlockSize;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::reportEjectability(bool *isEjectable)
{
	*isEjectable = true;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::reportMaxValidBlock(UInt64 *maxBlock)
{
	*maxBlock = fBlocks - 1;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = true;
	if (changedState != NULL) {
		*changedState = false;
	}
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::reportRemovability(bool *isRemovable)
{
	*isRemovable = true;
	return kIOReturnSuccess;
}

IOReturn
AppleDiskImageDevice::reportWriteProtection(bool *isWriteProtected)
{
	*isWriteProtected = fReadOnly;
	return kIOReturnSuccess;
}

IOReturn
DIDeviceCreatorUserClient::clientClose(void)
{
	terminate();
	return kIOReturnSuccess;
}

IOReturn
DIDeviceCreatorUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	const uint8_t *in = (const uint8_t *)args->structureInput;
	AppleDiskImageDevice *dev;
	IOService *controller = getProvider();
	char url[1025];
	uint64_t blocks, id;
	uint32_t blockSize, mode, uid, gid, created = 1;

	(void)dispatch; (void)target; (void)reference;
	if (selector != 0) {
		return kIOReturnUnsupported;
	}
	if (in == NULL || args->structureInputSize < kDICreateSize || args->structureOutput == NULL ||
	    args->structureOutputSize < 12 || controller == NULL) {
		return kIOReturnBadArgument;
	}
	memcpy(&blocks, in + kDICreateBlocks, sizeof(blocks));
	memcpy(&blockSize, in + kDICreateBlockSize, sizeof(blockSize));
	memcpy(&mode, in + kDICreateMode, sizeof(mode));
	memcpy(&uid, in + kDICreateUID, sizeof(uid));
	memcpy(&gid, in + kDICreateGID, sizeof(gid));
	memcpy(url, in + kDICreateURL, 1024);
	url[1024] = 0;
	// only RAM images, a file image needs diskimagesiod to serve each request
	if (in[kDICreateLocation] != 2) {
		return kIOReturnUnsupported;
	}

	dev = OSTypeAlloc(AppleDiskImageDevice);
	if (dev == NULL || !dev->initWithParams(blocks, blockSize, true, uid, gid, mode, in + kDICreateInstance, url)) {
		OSSafeReleaseNULL(dev);
		return kIOReturnNoMemory;
	}
	if (!dev->attach(controller)) {
		dev->release();
		return kIOReturnError;
	}
	if (!dev->start(controller)) {
		dev->detach(controller);
		dev->release();
		return kIOReturnError;
	}
	id = dev->getRegistryEntryID();
	dev->release();

	memcpy(args->structureOutput, &id, sizeof(id));
	memcpy((uint8_t *)args->structureOutput + 8, &created, sizeof(created));
	args->structureOutputSize = 12;
	return kIOReturnSuccess;
}

bool
DIDeviceIOUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type)
{
	if (!IOUserClient::initWithTask(owningTask, securityID, type)) {
		return false;
	}
	fTask = owningTask;
	return true;
}

IOReturn
DIDeviceIOUserClient::clientClose(void)
{
	AppleDiskImageDevice *dev = OSDynamicCast(AppleDiskImageDevice, getProvider());

	// the memory belongs to diskimagesiod, the device cannot outlive its client
	if (dev != NULL) {
		dev->terminate();
		dev->disconnectMemory();
	}
	terminate();
	return kIOReturnSuccess;
}

IOReturn
DIDeviceIOUserClient::registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon)
{
	// RAM images never notify the daemon, file images would need these per request
	(void)type; (void)refCon;
	if (port != MACH_PORT_NULL) {
		releaseNotificationPort(port);
	}
	return kIOReturnSuccess;
}

IOReturn
DIDeviceIOUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	AppleDiskImageDevice *dev = OSDynamicCast(AppleDiskImageDevice, getProvider());
	const uint8_t *in = (const uint8_t *)args->structureInput;
	uint64_t base, length;

	(void)dispatch; (void)target; (void)reference;
	switch (selector) {
	case 0:
		if (dev == NULL || in == NULL || args->structureInputSize < kDIConnectSize) {
			return kIOReturnBadArgument;
		}
		memcpy(&base, in + kDIConnectBase, sizeof(base));
		memcpy(&length, in + kDIConnectLength, sizeof(length));
		return dev->connectMemory(fTask, base, length, in[kDIConnectReadOnly] != 0);
	case 1:
		return kIOReturnSuccess;
	case 2:
		if (args->scalarInputCount < 1 || args->scalarOutputCount < 1) {
			return kIOReturnBadArgument;
		}
		args->scalarOutput[0] = args->scalarInput[0];
		args->scalarOutputCount = 1;
		return kIOReturnSuccess;
	default:
		return kIOReturnUnsupported;
	}
}
