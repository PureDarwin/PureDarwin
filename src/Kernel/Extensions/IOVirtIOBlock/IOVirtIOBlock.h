#ifndef _PD_IOVIRTIO_BLOCK_H
#define _PD_IOVIRTIO_BLOCK_H

#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "IOVirtIOTransport.h"

class IOVirtIOBlockDisk;

// virtio-blk request types (VIRTIO 1.1 section 5.2.6).
enum {
    kVirtIOBlkTypeIn    = 0,    // device -> driver (read)
    kVirtIOBlkTypeOut   = 1,    // driver -> device (write)
    kVirtIOBlkTypeFlush = 4,
};

// Status byte the device writes into the tail of every request chain.
enum {
    kVirtIOBlkStatusOK     = 0,
    kVirtIOBlkStatusIOErr  = 1,
    kVirtIOBlkStatusUnsupp = 2,
};

// Feature bits we understand; anything else the device offers is not acked.
enum {
    kVirtIOBlkFSizeMax  = (1u << 1),
    kVirtIOBlkFSegMax   = (1u << 2),
    kVirtIOBlkFGeometry = (1u << 4),
    kVirtIOBlkFRO       = (1u << 5),
    kVirtIOBlkFBlkSize  = (1u << 6),
    kVirtIOBlkFFlush    = (1u << 9),
};

// Request header, device-readable, at the head of every chain.
struct VirtIOBlkReqHdr {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;    // ALWAYS in 512-byte units, independent of blk_size
} __attribute__((packed));

class IOVirtIOBlock : public IOService
{
    OSDeclareDefaultStructors(IOVirtIOBlock);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void free() override;

    IOReturn readWrite(bool write, UInt64 block, UInt64 nblks,
                       IOMemoryDescriptor *buffer);
    IOReturn flush();

    UInt64 blockCount() const { return fBlockCount; }
    UInt32 blockSize()  const { return fBlockSize; }
    bool   isReadOnly() const { return fReadOnly; }

private:
    static const unsigned kMaxDataDesc = 32;
    static const unsigned kQueueSize   = 128;
    static const unsigned kIOTimeoutMs = 30000;

    IOReturn submit(uint32_t type, uint64_t sector,
                    IOMemoryDescriptor *buffer, UInt64 offset, UInt64 length,
                    bool deviceWrites);

    IOPCIDevice        *fPCIDevice;
    IOVirtIOTransport   fTransport;
    VirtQueue           fQueue;
    IOLock             *fLock;

    // Header and status live in one small contiguous allocation so a request
    // costs no per-I/O allocation.
    IOBufferMemoryDescriptor *fReqBuf;
    VirtIOBlkReqHdr    *fReqHdr;
    volatile uint8_t   *fReqStatus;
    uint64_t            fReqHdrPhys;
    uint64_t            fReqStatusPhys;

    IOVirtIOBlockDisk  *fDisk;
    UInt64              fBlockCount;   // in fBlockSize units
    UInt32              fBlockSize;
    bool                fReadOnly;
    bool                fHasFlush;
};

#endif /* _PD_IOVIRTIO_BLOCK_H */
