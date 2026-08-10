#include "IOVirtIOBlock.h"
#include "IOVirtIOBlockDisk.h"

#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(IOVirtIOBlock, IOService);

// Offsets into struct virtio_blk_config (VIRTIO 1.1 section 5.2.4).
enum {
    kBlkCfgCapacity = 0,    // u64, in 512-byte sectors
    kBlkCfgSizeMax  = 8,    // u32
    kBlkCfgSegMax   = 12,   // u32
    kBlkCfgGeometry = 16,   // u16 cylinders, u8 heads, u8 sectors
    kBlkCfgBlkSize  = 20,   // u32
};

static uint32_t
cfgRead32(volatile uint8_t *cfg, unsigned off)
{
    return OSReadLittleInt32((void *)cfg, off);
}

static uint64_t
cfgRead64(volatile uint8_t *cfg, unsigned off)
{
    return (uint64_t)cfgRead32(cfg, off) |
           ((uint64_t)cfgRead32(cfg, off + 4) << 32);
}

bool
IOVirtIOBlock::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("IOVirtIOBlock: failed to open PCI device\n");
        return false;
    }

    if (!fTransport.attach(fPCIDevice)) {
        IOLog("IOVirtIOBlock: failed to find/map virtio-pci capabilities\n");
        return false;
    }

    // Ack only the features we actually implement, and only where the device
    // offers them. Acking a feature we do not honour changes the wire format
    // or the config layout and corrupts every request.
    uint32_t offered = fTransport.deviceFeaturesLow();
    uint32_t wanted  = offered & (kVirtIOBlkFBlkSize | kVirtIOBlkFFlush |
                                  kVirtIOBlkFRO | kVirtIOBlkFSegMax |
                                  kVirtIOBlkFSizeMax | kVirtIOBlkFGeometry);
    if (!fTransport.negotiateFeatures(wanted)) {
        IOLog("IOVirtIOBlock: device rejected feature negotiation\n");
        return false;
    }
    fReadOnly = (wanted & kVirtIOBlkFRO) != 0;
    fHasFlush = (wanted & kVirtIOBlkFFlush) != 0;

    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg) {
        IOLog("IOVirtIOBlock: no device config region\n");
        return false;
    }

    // capacity is in 512-byte sectors no matter what blk_size says.
    uint64_t sectors = cfgRead64(cfg, kBlkCfgCapacity);

    fBlockSize = 512;
    if (wanted & kVirtIOBlkFBlkSize) {
        uint32_t bs = cfgRead32(cfg, kBlkCfgBlkSize);
        // Only honour a sane value; anything else and we stay at 512, which
        // is always a valid view of the device.
        if (bs >= 512 && (bs % 512) == 0 && (bs & (bs - 1)) == 0)
            fBlockSize = bs;
    }
    fBlockCount = sectors / (fBlockSize / 512);

    if (fBlockCount == 0) {
        IOLog("IOVirtIOBlock: device reports zero capacity\n");
        return false;
    }

    if (!fTransport.initQueue(&fQueue, 0, kQueueSize)) {
        IOLog("IOVirtIOBlock: failed to init request virtqueue\n");
        return false;
    }

    // Header and status in one contiguous page-safe allocation.
    fReqBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        sizeof(VirtIOBlkReqHdr) + 1, 0xFFFFFFFFULL);
    if (!fReqBuf) {
        IOLog("IOVirtIOBlock: failed to allocate request buffer\n");
        return false;
    }
    fReqBuf->prepare();
    fReqHdr    = (VirtIOBlkReqHdr *)fReqBuf->getBytesNoCopy();
    fReqStatus = (volatile uint8_t *)(fReqHdr + 1);
    IOByteCount seglen = 0;
    fReqHdrPhys    = fReqBuf->getPhysicalSegment(0, &seglen, 0);
    fReqStatusPhys = fReqHdrPhys + sizeof(VirtIOBlkReqHdr);
    if (!fReqHdrPhys) {
        IOLog("IOVirtIOBlock: request buffer has no physical address\n");
        return false;
    }

    fLock = IOLockAlloc();
    if (!fLock)
        return false;

    fTransport.setDriverOk();

    IOLog("IOVirtIOBlock: %llu blocks of %u bytes (%llu MB)%s%s\n",
          fBlockCount, (unsigned)fBlockSize,
          (fBlockCount * fBlockSize) >> 20,
          fReadOnly ? ", read-only" : "",
          fHasFlush ? ", flush" : "");

    fDisk = new IOVirtIOBlockDisk;
    if (!fDisk || !fDisk->initWithController(this)) {
        IOLog("IOVirtIOBlock: failed to create disk nub\n");
        if (fDisk) { fDisk->release(); fDisk = NULL; }
        return false;
    }
    if (!fDisk->attach(this) || !fDisk->start(this)) {
        IOLog("IOVirtIOBlock: failed to attach/start disk nub\n");
        fDisk->release();
        fDisk = NULL;
        return false;
    }
    fDisk->registerService();

    registerService();
    return true;
}

void
IOVirtIOBlock::stop(IOService *provider)
{
    if (fDisk) {
        fDisk->terminate();
        fDisk->release();
        fDisk = NULL;
    }
    fTransport.freeQueue(&fQueue);
    fTransport.detach();
    if (fPCIDevice)
        fPCIDevice->close(this);
    super::stop(provider);
}

void
IOVirtIOBlock::free()
{
    if (fLock) {
        IOLockFree(fLock);
        fLock = NULL;
    }
    if (fReqBuf) {
        fReqBuf->complete();
        fReqBuf->release();
        fReqBuf = NULL;
    }
    if (fPCIDevice) {
        fPCIDevice->release();
        fPCIDevice = NULL;
    }
    super::free();
}

// Build and run one request chain covering [offset, offset+length) of `buffer`.
// Caller holds fLock.
IOReturn
IOVirtIOBlock::submit(uint32_t type, uint64_t sector,
                      IOMemoryDescriptor *buffer, UInt64 offset, UInt64 length,
                      bool deviceWrites)
{
    VirtIOChainEntry entries[kMaxDataDesc + 2];
    unsigned count = 0;

    fReqHdr->type     = OSSwapHostToLittleInt32(type);
    fReqHdr->reserved = 0;
    fReqHdr->sector   = OSSwapHostToLittleInt64(sector);
    *fReqStatus       = 0xff;   // so a device that writes nothing is not "OK"

    entries[count].addr  = fReqHdrPhys;
    entries[count].len   = sizeof(VirtIOBlkReqHdr);
    entries[count].write = false;
    count++;

    UInt64 done = 0;
    while (done < length) {
        IOByteCount seglen = 0;
        addr64_t phys = buffer->getPhysicalSegment(offset + done, &seglen, 0);
        if (!phys || seglen == 0)
            return kIOReturnVMError;
        if (seglen > length - done)
            seglen = length - done;
        if (count >= kMaxDataDesc + 1)
            return kIOReturnNoResources;   // caller splits; see readWrite

        entries[count].addr  = phys;
        entries[count].len   = (uint32_t)seglen;
        entries[count].write = deviceWrites;
        count++;
        done += seglen;
    }

    entries[count].addr  = fReqStatusPhys;
    entries[count].len   = 1;
    entries[count].write = true;
    count++;

    fTransport.addDescChain(&fQueue, entries, count);
    fTransport.notify(&fQueue);

    if (!fTransport.pollForCompletion(&fQueue, kIOTimeoutMs)) {
        IOLog("IOVirtIOBlock: request type %u sector %llu timed out\n",
              type, sector);
        return kIOReturnTimeout;
    }

    uint8_t status = *fReqStatus;
    if (status != kVirtIOBlkStatusOK) {
        IOLog("IOVirtIOBlock: request type %u sector %llu status %u\n",
              type, sector, status);
        return status == kVirtIOBlkStatusUnsupp ? kIOReturnUnsupported
                                                : kIOReturnIOError;
    }
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOBlock::readWrite(bool write, UInt64 block, UInt64 nblks,
                         IOMemoryDescriptor *buffer)
{
    if (!buffer || nblks == 0)
        return kIOReturnBadArgument;
    if (block + nblks > fBlockCount)
        return kIOReturnBadArgument;
    if (write && fReadOnly)
        return kIOReturnNotWritable;

    const UInt64 sectorsPerBlock = fBlockSize / 512;
    IOReturn ret = kIOReturnSuccess;

    IOLockLock(fLock);

    UInt64 blockDone = 0;
    UInt64 chunkBlocks = nblks;
    while (blockDone < nblks) {
        UInt64 remain = nblks - blockDone;
        if (chunkBlocks > remain)
            chunkBlocks = remain;

        UInt64 offset = blockDone * fBlockSize;
        UInt64 length = chunkBlocks * fBlockSize;

        ret = submit(write ? kVirtIOBlkTypeOut : kVirtIOBlkTypeIn,
                     (block + blockDone) * sectorsPerBlock,
                     buffer, offset, length, !write);

        if (ret == kIOReturnNoResources && chunkBlocks > 1) {
            chunkBlocks /= 2;   // too fragmented; try a smaller piece
            continue;
        }
        if (ret != kIOReturnSuccess)
            break;

        blockDone += chunkBlocks;
    }

    IOLockUnlock(fLock);
    return ret;
}

IOReturn
IOVirtIOBlock::flush()
{
    if (!fHasFlush)
        return kIOReturnSuccess;   // device has no volatile write cache

    IOLockLock(fLock);
    IOReturn ret = submit(kVirtIOBlkTypeFlush, 0, NULL, 0, 0, false);
    IOLockUnlock(fLock);
    return ret;
}
