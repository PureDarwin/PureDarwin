// Hyper-V synthetic SCSI (storvsc): the vstor protocol over a VMBus channel,
// with one IOBlockStorageDevice per LUN. Packet layouts follow FreeBSD's BSD-licensed hv_vstorage.h
#ifndef PD_HV_STORAGE_H
#define PD_HV_STORAGE_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include "PDHyperV.h"

#define VSTOR_OP_COMPLETEIO        1
#define VSTOR_OP_EXECUTESRB        3
#define VSTOR_OP_BEGININIT         7
#define VSTOR_OP_ENDINIT           8
#define VSTOR_OP_QUERYPROTOCOL     9
#define VSTOR_OP_QUERYPROPERTIES   10
#define VSTOR_OP_ENUMERATE_BUS     11
#define VSTOR_FLAG_COMPLETION      1

#define VSTOR_PROTO_WIN10          0x0602
#define VSTOR_PROTO_WIN8_1         0x0600
#define VSTOR_PROTO_WIN8           0x0501

#define SRB_STATUS_SUCCESS         0x01
#define SRB_FLAGS_DATA_IN          0x40
#define SRB_FLAGS_DATA_OUT         0x80
#define SRB_FLAGS_DISABLE_SYNCH    0x08
#define VSTOR_DATA_WRITE           0
#define VSTOR_DATA_READ            1
#define VSTOR_DATA_NONE            2

struct vmscsi_req {
	uint16_t length;
	uint8_t  srb_status;
	uint8_t  scsi_status;
	uint8_t  port;
	uint8_t  path_id;
	uint8_t  target_id;
	uint8_t  lun;
	uint8_t  cdb_len;
	uint8_t  sense_info_len;
	uint8_t  data_in;
	uint8_t  reserved;
	uint32_t transfer_len;
	uint8_t  cdb[20];               // CDB in, sense data out
	uint16_t win8_reserve;
	uint8_t  queue_tag;
	uint8_t  queue_action;
	uint32_t srb_flags;
	uint32_t time_out_value;
	uint32_t queue_sort_key;
} __attribute__((packed));

struct vstor_packet {
	uint32_t operation;
	uint32_t flags;
	uint32_t status;
	union {
		struct vmscsi_req srb;
		struct {
			uint16_t proto_ver;
			uint8_t  path_id;
			uint8_t  target_id;
			uint16_t max_channel_cnt;
			uint16_t port;
			uint32_t flags;
			uint32_t max_transfer_bytes;
			uint64_t unique_id;
		} __attribute__((packed)) props;
		struct {
			uint16_t major_minor;
			uint16_t revision;
		} __attribute__((packed)) version;
	} u;
} __attribute__((packed));

#define PD_HV_MAX_LUNS      8
#define PD_HV_BOUNCE_PAGES  64

class PDHVDisk;

class PDHVStorage : public IOService
{
	OSDeclareDefaultStructors(PDHVStorage);

public:
	virtual bool start(IOService *provider) override;

	// Synchronous SCSI command. Data goes through the controller's bounce buffer
	IOReturn scsi(uint8_t lun, const uint8_t *cdb, uint8_t cdbLen, int dir, void *buf,
	    uint32_t len, uint32_t *transferred);
	IOReturn readWrite(uint8_t lun, bool write, UInt64 block, UInt64 nblks, UInt32 blockSize,
	    IOMemoryDescriptor *buffer);
	IOReturn flush(uint8_t lun);

private:
	static void channelCallback(OSObject *target, PDVMBusChannel *chan);
	static void rescanThread(void *arg, wait_result_t wr);
	IOReturn transact(struct vstor_packet *pkt, bool withData, uint32_t dataLen);
	bool handshake(void);
	void probeLuns(void);

	PDVMBusChannel *fChannel;
	IOLock *fLock;             // One request on the wire at a time
	IOLock *fWaitLock;
	uint64_t fNextId;
	uint64_t fPendingId;
	bool fDone;
	struct vstor_packet fResponse;
	struct pd_hv_mem fBounce;
	uint8_t fPathId;
	uint8_t fTargetId;
	uint32_t fMaxTransfer;
	uint32_t fLunsPresent;          // Bit per LUN already published
	volatile bool fRescanWanted;    // host announced a bus change
};

class PDHVDisk : public IOBlockStorageDevice
{
	OSDeclareDefaultStructors(PDHVDisk);

public:
	bool initWithController(PDHVStorage *controller, uint8_t lun, UInt64 blocks, UInt32 blockSize,
	    const char *vendor, const char *product);

	IOReturn doAsyncReadWrite(IOMemoryDescriptor *buffer, UInt64 block, UInt64 nblks,
	    IOStorageAttributes *attributes, IOStorageCompletion *completion) override;
	IOReturn doSynchronize(UInt64 block, UInt64 nblks, IOStorageSynchronizeOptions options = 0) override;
	IOReturn doEjectMedia(void) override;
	IOReturn doFormatMedia(UInt64 byteCapacity) override;
	UInt32 doGetFormatCapacities(UInt64 *capacities, UInt32 capacitiesMaxCount) const override;

	char *getVendorString(void) override;
	char *getProductString(void) override;
	char *getRevisionString(void) override;
	char *getAdditionalDeviceInfoString(void) override;

	IOReturn reportBlockSize(UInt64 *blockSize) override;
	IOReturn reportEjectability(bool *isEjectable) override;
	IOReturn reportMaxValidBlock(UInt64 *maxBlock) override;
	IOReturn reportMediaState(bool *mediaPresent, bool *changedState = 0) override;
	IOReturn reportRemovability(bool *isRemovable) override;
	IOReturn reportWriteProtection(bool *isWriteProtected) override;

private:
	PDHVStorage *fController;
	uint8_t fLun;
	UInt64 fBlocks;
	UInt32 fBlockSize;
	char fVendor[9];
	char fProduct[17];
};

#endif
