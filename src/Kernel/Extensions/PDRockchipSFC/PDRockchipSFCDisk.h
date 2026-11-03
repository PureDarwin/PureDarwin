#ifndef _PUREDARWIN_PDROCKCHIPSFCDISK_H
#define _PUREDARWIN_PDROCKCHIPSFCDISK_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include "PDRockchipSFC.h"

class PDRockchipSFCDisk : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(PDRockchipSFCDisk);

public:
	bool initWithController(PDRockchipSFC *controller);
	void free(void) APPLE_KEXT_OVERRIDE;

	IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
	    UInt64 nblks, IOStorageAttributes *attributes,
	    IOStorageCompletion *completion) APPLE_KEXT_OVERRIDE;
	IOReturn doSynchronize(UInt64 block, UInt64 nblks,
	    IOStorageSynchronizeOptions options = 0) APPLE_KEXT_OVERRIDE;
	IOReturn doEjectMedia(void) APPLE_KEXT_OVERRIDE;
	IOReturn doFormatMedia(UInt64 byteCapacity) APPLE_KEXT_OVERRIDE;
	UInt32   doGetFormatCapacities(UInt64 *capacities,
	    UInt32 capacitiesMaxCount) const APPLE_KEXT_OVERRIDE;

	char *getVendorString(void) APPLE_KEXT_OVERRIDE;
	char *getProductString(void) APPLE_KEXT_OVERRIDE;
	char *getRevisionString(void) APPLE_KEXT_OVERRIDE;
	char *getAdditionalDeviceInfoString(void) APPLE_KEXT_OVERRIDE;

	IOReturn reportBlockSize(UInt64 *blockSize) APPLE_KEXT_OVERRIDE;
	IOReturn reportEjectability(bool *isEjectable) APPLE_KEXT_OVERRIDE;
	IOReturn reportMaxValidBlock(UInt64 *maxBlock) APPLE_KEXT_OVERRIDE;
	IOReturn reportMediaState(bool *mediaPresent,
	    bool *changedState = 0) APPLE_KEXT_OVERRIDE;
	IOReturn reportRemovability(bool *isRemovable) APPLE_KEXT_OVERRIDE;
	IOReturn reportWriteProtection(bool *isWriteProtected) APPLE_KEXT_OVERRIDE;

private:
	IOReturn writeOverlay(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks);
	void applyOverlay(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks);
	IOReturn flushLocked(void);
	static void flushTimer(OSObject *owner, IOTimerEventSource *sender);

	PDRockchipSFC *fController;
	// Write-back cache: written pages wait in RAM until a sync, the timer or a full cache
	// rewrites their erase blocks on the flash
	uint8_t **fOverlay;
	UInt64 fOverlayPages, fOverlayUsed;
	IOLock *fOverlayLock;
	uint8_t *fBlockBuf;
	IOWorkLoop *fWorkLoop;
	IOTimerEventSource *fTimer;
};

#endif /* _PUREDARWIN_PDROCKCHIPSFCDISK_H */
