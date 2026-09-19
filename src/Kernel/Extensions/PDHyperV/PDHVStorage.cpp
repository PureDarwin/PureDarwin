#include "PDHVStorage.h"

#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(PDHVStorage, IOService);
OSDefineMetaClassAndStructors(PDHVDisk, IOBlockStorageDevice);

#define STOR_LOG(fmt, ...) IOLog("PDHVStorage: " fmt "\n", ##__VA_ARGS__)

#define VSTOR_PKT_SIZE sizeof(struct vstor_packet)
#define PD_HV_RING_PAGES 32

void
PDHVStorage::channelCallback(OSObject *target, PDVMBusChannel *chan)
{
	PDHVStorage *self = OSDynamicCast(PDHVStorage, target);
	if (self == NULL) {
		return;
	}
	for (;;) {
		struct vstor_packet pkt;
		uint32_t len = sizeof(pkt);
		uint64_t xactid;
		uint16_t type;

		bzero(&pkt, sizeof(pkt));
		if (chan->recv(&pkt, &len, &xactid, &type) != kIOReturnSuccess) {
			break;
		}
		if (pkt.operation == VSTOR_OP_ENUMERATE_BUS) {
			self->fRescanWanted = true;       // disks hot-added or removed
			continue;
		}
		IOLockLock(self->fWaitLock);
		if (xactid == self->fPendingId && !self->fDone) {
			bcopy(&pkt, &self->fResponse, sizeof(pkt));
			self->fDone = true;
			IOLockWakeup(self->fWaitLock, &self->fDone, false);
		}
		IOLockUnlock(self->fWaitLock);
	}
}

// Send one vstor packet (optionally with the bounce buffer as data) and wait for its reply
IOReturn
PDHVStorage::transact(struct vstor_packet *pkt, bool withData, uint32_t dataLen)
{
	IOLockLock(fWaitLock);
	fPendingId = ++fNextId;
	fDone = false;
	uint64_t id = fPendingId;
	IOLockUnlock(fWaitLock);

	IOReturn ret;
	if (withData) {
		uint64_t pfns[PD_HV_BOUNCE_PAGES];
		uint32_t pages = (dataLen + PAGE_SIZE - 1) / PAGE_SIZE;
		for (uint32_t i = 0; i < pages; i++) {
			pfns[i] = (fBounce.pa >> 12) + i;
		}
		ret = fChannel->sendPages(pfns, pages, 0, dataLen, pkt, VSTOR_PKT_SIZE, id);
	} else {
		ret = fChannel->send(VMBUS_CHANPKT_TYPE_INBAND, VMBUS_CHANPKT_FLAG_RC, pkt, VSTOR_PKT_SIZE, id);
	}
	if (ret != kIOReturnSuccess) {
		return ret;
	}

	IOLockLock(fWaitLock);
	if (!fDone) {
		AbsoluteTime deadline;
		clock_interval_to_deadline(30, kSecondScale, &deadline);
		IOLockSleepDeadline(fWaitLock, &fDone, deadline, THREAD_UNINT);
	}
	bool done = fDone;
	if (done) {
		bcopy(&fResponse, pkt, sizeof(*pkt));
	}
	fPendingId = 0;
	IOLockUnlock(fWaitLock);
	return done ? kIOReturnSuccess : kIOReturnTimeout;
}

bool
PDHVStorage::handshake(void)
{
	struct vstor_packet pkt;
	static const uint16_t protos[] = { VSTOR_PROTO_WIN10, VSTOR_PROTO_WIN8_1, VSTOR_PROTO_WIN8 };

	bzero(&pkt, sizeof(pkt));
	pkt.operation = VSTOR_OP_BEGININIT;
	pkt.flags = VSTOR_FLAG_COMPLETION;
	if (transact(&pkt, false, 0) != kIOReturnSuccess || pkt.status != 0) {
		STOR_LOG("begin initialization failed (status 0x%x)", pkt.status);
		return false;
	}

	bool agreed = false;
	for (size_t i = 0; i < sizeof(protos) / sizeof(protos[0]) && !agreed; i++) {
		bzero(&pkt, sizeof(pkt));
		pkt.operation = VSTOR_OP_QUERYPROTOCOL;
		pkt.flags = VSTOR_FLAG_COMPLETION;
		pkt.u.version.major_minor = protos[i];
		if (transact(&pkt, false, 0) != kIOReturnSuccess) {
			return false;
		}
		if (pkt.status == 0) {
			STOR_LOG("vstor protocol %u.%u", protos[i] >> 8, protos[i] & 0xff);
			agreed = true;
		}
	}
	if (!agreed) {
		STOR_LOG("no common vstor protocol");
		return false;
	}

	bzero(&pkt, sizeof(pkt));
	pkt.operation = VSTOR_OP_QUERYPROPERTIES;
	pkt.flags = VSTOR_FLAG_COMPLETION;
	if (transact(&pkt, false, 0) != kIOReturnSuccess || pkt.status != 0) {
		STOR_LOG("query properties failed (status 0x%x)", pkt.status);
		return false;
	}
	fPathId = pkt.u.props.path_id;
	fTargetId = pkt.u.props.target_id;
	fMaxTransfer = pkt.u.props.max_transfer_bytes;
	STOR_LOG("path %u target %u, max transfer %u bytes, %u channel(s)", fPathId, fTargetId,
	    fMaxTransfer, pkt.u.props.max_channel_cnt + 1);

	bzero(&pkt, sizeof(pkt));
	pkt.operation = VSTOR_OP_ENDINIT;
	pkt.flags = VSTOR_FLAG_COMPLETION;
	if (transact(&pkt, false, 0) != kIOReturnSuccess || pkt.status != 0) {
		STOR_LOG("end initialization failed (status 0x%x)", pkt.status);
		return false;
	}
	return true;
}

IOReturn
PDHVStorage::scsi(uint8_t lun, const uint8_t *cdb, uint8_t cdbLen, int dir, void *buf, uint32_t len,
    uint32_t *transferred)
{
	struct vstor_packet pkt;

	if (len > PD_HV_BOUNCE_PAGES * PAGE_SIZE || cdbLen > 16) {
		return kIOReturnBadArgument;
	}
	bzero(&pkt, sizeof(pkt));
	pkt.operation = VSTOR_OP_EXECUTESRB;
	pkt.flags = VSTOR_FLAG_COMPLETION;
	pkt.u.srb.length = sizeof(struct vmscsi_req);
	pkt.u.srb.path_id = 0;
	pkt.u.srb.target_id = 0;
	pkt.u.srb.lun = lun;
	pkt.u.srb.cdb_len = cdbLen;
	pkt.u.srb.sense_info_len = 20;
	pkt.u.srb.transfer_len = len;
	pkt.u.srb.time_out_value = 60;
	pkt.u.srb.srb_flags = SRB_FLAGS_DISABLE_SYNCH;
	bcopy(cdb, pkt.u.srb.cdb, cdbLen);
	if (len == 0) {
		pkt.u.srb.data_in = VSTOR_DATA_NONE;
	} else if (dir > 0) {
		pkt.u.srb.data_in = VSTOR_DATA_WRITE;
		pkt.u.srb.srb_flags |= SRB_FLAGS_DATA_OUT;
	} else {
		pkt.u.srb.data_in = VSTOR_DATA_READ;
		pkt.u.srb.srb_flags |= SRB_FLAGS_DATA_IN;
	}

	IOLockLock(fLock);
	if (len > 0 && dir > 0) {
		bcopy(buf, fBounce.va, len);
	}
	IOReturn ret = transact(&pkt, len > 0, len);
	if (ret == kIOReturnSuccess && len > 0 && dir <= 0) {
		bcopy(fBounce.va, buf, len);
	}
	IOLockUnlock(fLock);

	if (ret != kIOReturnSuccess) {
		return ret;
	}
	if (transferred != NULL) {
		*transferred = pkt.u.srb.transfer_len;
	}
	if ((pkt.u.srb.srb_status & 0x3f) != SRB_STATUS_SUCCESS || pkt.u.srb.scsi_status != 0) {
		return kIOReturnIOError;
	}
	return kIOReturnSuccess;
}

IOReturn
PDHVStorage::readWrite(uint8_t lun, bool write, UInt64 block, UInt64 nblks, UInt32 blockSize,
    IOMemoryDescriptor *buffer)
{
	uint32_t maxBlocks = (PD_HV_BOUNCE_PAGES * PAGE_SIZE) / blockSize;
	UInt64 done = 0;

	while (done < nblks) {
		uint32_t n = (uint32_t)((nblks - done) < maxBlocks ? (nblks - done) : maxBlocks);
		uint32_t bytes = n * blockSize;
		UInt64 lba = block + done;
		uint8_t cdb[16];

		bzero(cdb, sizeof(cdb));
		cdb[0] = write ? 0x8a : 0x88;           // WRITE(16) / READ(16)
		for (int i = 0; i < 8; i++) {
			cdb[2 + i] = (uint8_t)(lba >> (56 - 8 * i));
		}
		cdb[10] = (uint8_t)(n >> 24);
		cdb[11] = (uint8_t)(n >> 16);
		cdb[12] = (uint8_t)(n >> 8);
		cdb[13] = (uint8_t)n;

		struct vstor_packet pkt;
		bzero(&pkt, sizeof(pkt));
		pkt.operation = VSTOR_OP_EXECUTESRB;
		pkt.flags = VSTOR_FLAG_COMPLETION;
		pkt.u.srb.length = sizeof(struct vmscsi_req);
		pkt.u.srb.lun = lun;
		pkt.u.srb.cdb_len = 16;
		pkt.u.srb.sense_info_len = 20;
		pkt.u.srb.transfer_len = bytes;
		pkt.u.srb.time_out_value = 60;
		pkt.u.srb.srb_flags = SRB_FLAGS_DISABLE_SYNCH | (write ? SRB_FLAGS_DATA_OUT : SRB_FLAGS_DATA_IN);
		pkt.u.srb.data_in = write ? VSTOR_DATA_WRITE : VSTOR_DATA_READ;
		bcopy(cdb, pkt.u.srb.cdb, 16);

		IOLockLock(fLock);
		if (write && buffer->readBytes(done * blockSize, fBounce.va, bytes) != bytes) {
			IOLockUnlock(fLock);
			return kIOReturnIOError;
		}
		IOReturn ret = transact(&pkt, true, bytes);
		if (ret == kIOReturnSuccess && !write &&
		    ((pkt.u.srb.srb_status & 0x3f) == SRB_STATUS_SUCCESS && pkt.u.srb.scsi_status == 0)) {
			buffer->writeBytes(done * blockSize, fBounce.va, bytes);
		}
		IOLockUnlock(fLock);
		if (ret != kIOReturnSuccess) {
			return ret;
		}
		if ((pkt.u.srb.srb_status & 0x3f) != SRB_STATUS_SUCCESS || pkt.u.srb.scsi_status != 0) {
			STOR_LOG("lun %u %s lba %llu x%u failed: srb 0x%x scsi 0x%x", lun, write ? "write" : "read",
			    lba, n, pkt.u.srb.srb_status, pkt.u.srb.scsi_status);
			return kIOReturnIOError;
		}
		done += n;
	}
	return kIOReturnSuccess;
}

IOReturn
PDHVStorage::flush(uint8_t lun)
{
	const uint8_t cdb[10] = { 0x35 };               // SYNCHRONIZE CACHE(10)
	return scsi(lun, cdb, sizeof(cdb), 0, NULL, 0, NULL);
}

void
PDHVStorage::probeLuns(void)
{
	for (uint8_t lun = 0; lun < PD_HV_MAX_LUNS; lun++) {
		uint8_t inq[36], cap[32];

		if (fLunsPresent & (1U << lun)) {
			continue;
		}
		const uint8_t inqCdb[6] = { 0x12, 0, 0, 0, sizeof(inq), 0 };
		uint32_t got = 0;

		bzero(inq, sizeof(inq));
		if (scsi(lun, inqCdb, sizeof(inqCdb), -1, inq, sizeof(inq), &got) != kIOReturnSuccess) {
			continue;
		}
		if ((inq[0] & 0xe0) != 0 || (inq[0] & 0x1f) != 0) {       // Connected direct-access only
			continue;
		}

		uint8_t capCdb[16];
		bzero(capCdb, sizeof(capCdb));
		capCdb[0] = 0x9e;                               // READ CAPACITY(16)
		capCdb[1] = 0x10;
		capCdb[13] = sizeof(cap);
		bzero(cap, sizeof(cap));
		if (scsi(lun, capCdb, sizeof(capCdb), -1, cap, sizeof(cap), &got) != kIOReturnSuccess) {
			STOR_LOG("lun %u: READ CAPACITY failed", lun);
			continue;
		}
		UInt64 last = 0;
		for (int i = 0; i < 8; i++) {
			last = (last << 8) | cap[i];
		}
		UInt32 bsize = ((UInt32)cap[8] << 24) | ((UInt32)cap[9] << 16) | ((UInt32)cap[10] << 8) | cap[11];
		char vendor[9], product[17];
		bcopy(inq + 8, vendor, 8);
		vendor[8] = 0;
		bcopy(inq + 16, product, 16);
		product[16] = 0;
		STOR_LOG("lun %u: %s %s, %llu blocks of %u bytes (%llu MB)", lun, vendor, product, last + 1,
		    bsize, ((last + 1) * bsize) >> 20);
		if (bsize == 0 || bsize > PAGE_SIZE * PD_HV_BOUNCE_PAGES) {
			continue;
		}

		PDHVDisk *disk = OSTypeAlloc(PDHVDisk);
		if (disk == NULL || !disk->initWithController(this, lun, last + 1, bsize, vendor, product) ||
		    !disk->attach(this)) {
			OSSafeReleaseNULL(disk);
			continue;
		}
		fLunsPresent |= 1U << lun;
		disk->registerService();
		disk->release();
	}
}

// Hot-added disks (e.g. wsl --mount): rescan on the host's bus notice,
// and poll for a while in case it is missed. Runs on its own thread. Probing waits on the work loop
void
PDHVStorage::rescanThread(void *arg, wait_result_t wr)
{
	PDHVStorage *self = (PDHVStorage *)arg;
	(void)wr;

	// WSL attaches distro disks long after boot. Keep a slow poll in case a bus notice is missed
	for (uint64_t tick = 0;; tick++) {
		IOSleep(1000);
		if (self->fRescanWanted || (tick % (tick < 600 ? 5 : 30)) == 4) {
			self->fRescanWanted = false;
			self->probeLuns();
		}
	}
}

bool
PDHVStorage::start(IOService *provider)
{
	if (!super::start(provider)) {
		return false;
	}
	fChannel = OSDynamicCast(PDVMBusChannel, provider);
	fLock = IOLockAlloc();
	fWaitLock = IOLockAlloc();
	if (fChannel == NULL || fLock == NULL || fWaitLock == NULL ||
	    !PDHyperV::allocPages(&fBounce, PD_HV_BOUNCE_PAGES)) {
		return false;
	}
	IOReturn ret = fChannel->open(PD_HV_RING_PAGES, PD_HV_RING_PAGES, this, channelCallback);
	if (ret != kIOReturnSuccess) {
		STOR_LOG("channel %u open failed: 0x%x", fChannel->channelID(), ret);
		return false;
	}
	STOR_LOG("channel %u open", fChannel->channelID());
	if (!handshake()) {
		return false;
	}
	probeLuns();
	registerService();

	thread_t thread;
	retain();
	if (kernel_thread_start(rescanThread, this, &thread) == KERN_SUCCESS) {
		thread_deallocate(thread);
	} else {
		release();
	}
	return true;
}

bool
PDHVDisk::initWithController(PDHVStorage *controller, uint8_t lun, UInt64 blocks, UInt32 blockSize,
    const char *vendor, const char *product)
{
	if (!IOBlockStorageDevice::init(NULL)) {
		return false;
	}
	fController = controller;
	fLun = lun;
	fBlocks = blocks;
	fBlockSize = blockSize;
	strlcpy(fVendor, vendor, sizeof(fVendor));
	strlcpy(fProduct, product, sizeof(fProduct));
	return true;
}

IOReturn
PDHVDisk::doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks,
    IOStorageAttributes *attributes, IOStorageCompletion *completion)
{
	(void)attributes;
	if (buffer == NULL || block + nblks > fBlocks) {
		IOStorage::complete(completion, kIOReturnBadArgument, 0);
		return kIOReturnBadArgument;
	}
	bool write = (buffer->getDirection() & kIODirectionOut) != 0;
	IOReturn ret = fController->readWrite(fLun, write, block, nblks, fBlockSize, buffer);
	IOStorage::complete(completion, ret, ret == kIOReturnSuccess ? nblks * fBlockSize : 0);
	return kIOReturnSuccess;
}

IOReturn
PDHVDisk::doSynchronize(UInt64 block, UInt64 nblks, IOStorageSynchronizeOptions options)
{
	(void)block;
	(void)nblks;
	(void)options;
	return fController->flush(fLun);
}

IOReturn PDHVDisk::doEjectMedia(void) { return kIOReturnUnsupported; }
IOReturn PDHVDisk::doFormatMedia(UInt64 byteCapacity) { (void)byteCapacity; return kIOReturnUnsupported; }

UInt32
PDHVDisk::doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const
{
	if (capacities != NULL && capacitiesMaxCount > 0) {
		capacities[0] = fBlocks * fBlockSize;
	}
	return 1;
}

char *PDHVDisk::getVendorString(void) { return fVendor; }
char *PDHVDisk::getProductString(void) { return fProduct; }
char *PDHVDisk::getRevisionString(void) { return (char *)"1.0"; }
char *PDHVDisk::getAdditionalDeviceInfoString(void) { return (char *)"Hyper-V synthetic SCSI"; }

IOReturn PDHVDisk::reportBlockSize(UInt64 *blockSize) { *blockSize = fBlockSize; return kIOReturnSuccess; }
IOReturn PDHVDisk::reportEjectability(bool *isEjectable) { *isEjectable = false; return kIOReturnSuccess; }
IOReturn PDHVDisk::reportMaxValidBlock(UInt64 *maxBlock) { *maxBlock = fBlocks - 1; return kIOReturnSuccess; }

IOReturn
PDHVDisk::reportMediaState(bool *mediaPresent, bool *changedState)
{
	*mediaPresent = true;
	if (changedState != NULL) {
		*changedState = false;
	}
	return kIOReturnSuccess;
}

IOReturn PDHVDisk::reportRemovability(bool *isRemovable) { *isRemovable = false; return kIOReturnSuccess; }
IOReturn PDHVDisk::reportWriteProtection(bool *isWriteProtected) { *isWriteProtected = false; return kIOReturnSuccess; }
