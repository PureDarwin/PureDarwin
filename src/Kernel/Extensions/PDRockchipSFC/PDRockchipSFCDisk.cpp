#include "PDRockchipSFCDisk.h"

#include <IOKit/IOLib.h>
#include <IOKit/storage/IOStorage.h>

// Cap on RAM held by written pages (the board has 64 MB); a full cache flushes
#define PD_SFC_OVERLAY_MAX	(4U << 20)
#define PD_SFC_FLUSH_MS		10000

#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors(PDRockchipSFCDisk, IOBlockStorageDevice);

bool
PDRockchipSFCDisk::initWithController(PDRockchipSFC *controller)
{
	if (controller == NULL) {
		return false;
	}
	if (!init(NULL)) {
		return false;
	}

	fController = controller;
	fOverlayPages = controller->pageCount();
	fOverlay = (uint8_t **)IOMalloc((vm_size_t)(fOverlayPages * sizeof(uint8_t *)));
	if (fOverlay != NULL) {
		bzero(fOverlay, (size_t)(fOverlayPages * sizeof(uint8_t *)));
	}
	fOverlayLock = IOLockAlloc();
	fBlockBuf = (uint8_t *)IOMalloc((vm_size_t)controller->pagesPerBlock() * controller->pageSize());
	if (fOverlay == NULL || fOverlayLock == NULL || fBlockBuf == NULL) {
		return false;
	}
	fWorkLoop = IOWorkLoop::workLoop();
	fTimer = IOTimerEventSource::timerEventSource(this, &PDRockchipSFCDisk::flushTimer);
	if (fWorkLoop != NULL && fTimer != NULL && fWorkLoop->addEventSource(fTimer) == kIOReturnSuccess) {
		fTimer->setTimeoutMS(PD_SFC_FLUSH_MS);
	}
	setProperty(kIOBlockStorageDeviceTypeKey, kIOBlockStorageDeviceTypeGeneric);
	setName("spinand0");
	setLocation("0");
	return true;
}

void
PDRockchipSFCDisk::free(void)
{
	if (fOverlay != NULL) {
		for (UInt64 i = 0; i < fOverlayPages; i++) {
			if (fOverlay[i] != NULL) {
				IOFree(fOverlay[i], fController->pageSize());
			}
		}
		IOFree(fOverlay, (vm_size_t)(fOverlayPages * sizeof(uint8_t *)));
		fOverlay = NULL;
	}
	if (fOverlayLock != NULL) {
		IOLockFree(fOverlayLock);
		fOverlayLock = NULL;
	}
	if (fTimer != NULL) {
		fTimer->cancelTimeout();
		if (fWorkLoop != NULL) {
			fWorkLoop->removeEventSource(fTimer);
		}
		OSSafeReleaseNULL(fTimer);
	}
	OSSafeReleaseNULL(fWorkLoop);
	if (fBlockBuf != NULL && fController != NULL) {
		IOFree(fBlockBuf, (vm_size_t)fController->pagesPerBlock() * fController->pageSize());
		fBlockBuf = NULL;
	}
	fController = NULL;
	super::free();
}

IOReturn
PDRockchipSFCDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block,
    UInt64 nblks, IOStorageAttributes *attributes,
    IOStorageCompletion *completion)
{
	(void)attributes;

	if (fController == NULL || buffer == NULL) {
		IOStorage::complete(completion, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
	}

	if (block >= fOverlayPages || nblks > fOverlayPages - block) {
		IOStorage::complete(completion, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
	}
	if ((buffer->getDirection() & kIODirectionOut) != 0) {
		IOReturn w = writeOverlay(buffer, block, nblks);
		IOStorage::complete(completion, w, w == kIOReturnSuccess ? nblks * fController->pageSize() : 0);
		return w;
	}

	IOReturn prep = buffer->prepare();
	if (prep != kIOReturnSuccess) {
		IOStorage::complete(completion, prep, 0);
		return prep;
	}

	IOReturn ret = fController->readPages(block, nblks, buffer);
	if (ret == kIOReturnSuccess) {
		applyOverlay(buffer, block, nblks);
	}
	buffer->complete();

	IOStorage::complete(completion, ret,
	    ret == kIOReturnSuccess ? nblks * fController->pageSize() : 0);
	return ret;
}

IOReturn
PDRockchipSFCDisk::writeOverlay(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks)
{
	UInt32 ps = fController->pageSize();
	IOReturn ret = kIOReturnSuccess;

	IOLockLock(fOverlayLock);
	for (UInt64 i = 0; i < nblks; i++) {
		uint8_t *p = fOverlay[block + i];
		if (p == NULL) {
			if (fOverlayUsed + ps > PD_SFC_OVERLAY_MAX && (ret = flushLocked()) != kIOReturnSuccess) {
				break;
			}
			p = (uint8_t *)IOMalloc(ps);
			if (p == NULL) {
				ret = kIOReturnNoMemory;
				break;
			}
			fOverlay[block + i] = p;
			fOverlayUsed += ps;
		}
		if (buffer->readBytes(i * ps, p, ps) != ps) {
			ret = kIOReturnUnderrun;
			break;
		}
	}
	IOLockUnlock(fOverlayLock);
	return ret;
}

void
PDRockchipSFCDisk::applyOverlay(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks)
{
	UInt32 ps = fController->pageSize();

	IOLockLock(fOverlayLock);
	for (UInt64 i = 0; i < nblks; i++) {
		if (fOverlay[block + i] != NULL) {
			buffer->writeBytes(i * ps, fOverlay[block + i], ps);
		}
	}
	IOLockUnlock(fOverlayLock);
}

// Every erase block holding a written page: read the pages it keeps, erase, program all
IOReturn
PDRockchipSFCDisk::flushLocked(void)
{
	UInt32 ps = fController->pageSize(), ppb = fController->pagesPerBlock();
	UInt64 blocks = 0;
	IOReturn ret = kIOReturnSuccess;

	for (UInt64 b = 0; b * ppb < fOverlayPages && ret == kIOReturnSuccess; b++) {
		UInt64 first = b * ppb;
		bool dirty = false;

		for (UInt32 i = 0; i < ppb && first + i < fOverlayPages; i++) {
			dirty |= fOverlay[first + i] != NULL;
		}
		if (!dirty) {
			continue;
		}
		for (UInt32 i = 0; i < ppb && ret == kIOReturnSuccess; i++) {
			uint8_t *dst = fBlockBuf + (size_t)i * ps;

			if (fOverlay[first + i] != NULL) {
				memcpy(dst, fOverlay[first + i], ps);
			} else {
				ret = fController->readPage(first + i, dst);
			}
		}
		if (ret == kIOReturnSuccess) {
			ret = fController->writeBlock(b, fBlockBuf);
		}
		if (ret != kIOReturnSuccess) {
			IOLog("PDRockchipSFCDisk: flushing block %llu failed: 0x%x\n", b, ret);
			break;
		}
		for (UInt32 i = 0; i < ppb; i++) {
			if (fOverlay[first + i] != NULL) {
				IOFree(fOverlay[first + i], ps);
				fOverlay[first + i] = NULL;
				fOverlayUsed -= ps;
			}
		}
		blocks++;
	}
	if (blocks != 0) {
		static uint32_t sFlushLogs;

		if (sFlushLogs++ < 20) {
			IOLog("PDRockchipSFCDisk: flushed %llu erase blocks to flash\n", blocks);
		}
	}
	return ret;
}

void
PDRockchipSFCDisk::flushTimer(OSObject *owner, IOTimerEventSource *sender)
{
	PDRockchipSFCDisk *d = OSDynamicCast(PDRockchipSFCDisk, owner);

	if (d == NULL || d->fController == NULL) {
		return;
	}
	IOLockLock(d->fOverlayLock);
	if (d->fOverlayUsed != 0) {
		d->flushLocked();
	}
	IOLockUnlock(d->fOverlayLock);
	sender->setTimeoutMS(PD_SFC_FLUSH_MS);
}

IOReturn
PDRockchipSFCDisk::doSynchronize(UInt64 block, UInt64 nblks,
    IOStorageSynchronizeOptions options)
{
	IOReturn ret;

	(void)block;
	(void)nblks;
	(void)options;
	IOLockLock(fOverlayLock);
	ret = flushLocked();
	IOLockUnlock(fOverlayLock);
	return ret;
}

IOReturn PDRockchipSFCDisk::doEjectMedia(void) { return kIOReturnUnsupported; }

IOReturn
PDRockchipSFCDisk::doFormatMedia(UInt64 byteCapacity)
{
	(void)byteCapacity;
	return kIOReturnUnsupported;
}

UInt32
PDRockchipSFCDisk::doGetFormatCapacities(UInt64 *capacities,
    UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount != 0 && fController != NULL) {
		capacities[0] = fController->pageCount() * fController->pageSize();
	}
	return 1;
}

char *PDRockchipSFCDisk::getVendorString(void)   { return (char *)"Winbond"; }
char *PDRockchipSFCDisk::getProductString(void)  { return (char *)"SPI NAND"; }
char *PDRockchipSFCDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDRockchipSFCDisk::getAdditionalDeviceInfoString(void) { return (char *)""; }

IOReturn
PDRockchipSFCDisk::reportBlockSize(UInt64 *blockSize)
{
	if (blockSize == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	*blockSize = fController->pageSize();
	return kIOReturnSuccess;
}

IOReturn
PDRockchipSFCDisk::reportEjectability(bool *isEjectable)
{
	if (isEjectable != NULL) {
		*isEjectable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDRockchipSFCDisk::reportMaxValidBlock(UInt64 *maxBlock)
{
	if (maxBlock == NULL || fController == NULL) {
		return kIOReturnBadArgument;
	}
	UInt64 blocks = fController->pageCount();
	*maxBlock = blocks ? blocks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
PDRockchipSFCDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
	if (mediaPresent != NULL) {
		*mediaPresent = fController != NULL;
	}
	if (changedState != NULL) {
		*changedState = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDRockchipSFCDisk::reportRemovability(bool *isRemovable)
{
	if (isRemovable != NULL) {
		*isRemovable = false;
	}
	return kIOReturnSuccess;
}

IOReturn
PDRockchipSFCDisk::reportWriteProtection(bool *isWriteProtected)
{
	if (isWriteProtected != NULL) {
		*isWriteProtected = false;
	}
	return kIOReturnSuccess;
}
