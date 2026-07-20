#pragma once

#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>

// One split virtqueue: descriptor table + avail ring + used ring, each a
// separate physically-contiguous allocation.
struct VirtQueue {
    IOBufferMemoryDescriptor *descMem;
    IOBufferMemoryDescriptor *availMem;
    IOBufferMemoryDescriptor *usedMem;
    void      *desc;   // struct VRingDesc[queueSize]
    void      *avail;  // struct VRingAvailHdr + ring[queueSize]
    void      *used;   // struct VRingUsedHdr + ring[queueSize] of VRingUsedElem
    uint16_t   queueSize;
    uint16_t   queueIndex;
    uint16_t   lastUsedIdx;
    uint16_t   nextFreeDesc; // simple bump allocator; caller is responsible
                             // for not exceeding queueSize descriptors
                             // in flight at once (fine for a polling,
                             // one-request-at-a-time driver)
};

struct VRingDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};
enum { VRING_DESC_F_NEXT = 1, VRING_DESC_F_WRITE = 2 };

struct VRingAvailHdr { uint16_t flags; uint16_t idx; };
struct VRingUsedHdr  { uint16_t flags; uint16_t idx; };
struct VRingUsedElem { uint32_t id; uint32_t len; };

// One descriptor's worth of a chain being submitted; addr/len describe a
// physically-contiguous buffer, write == device-writable (response-style)
// vs device-readable (request-style).
struct VirtIOChainEntry {
    uint64_t addr;
    uint32_t len;
    bool     write;
};

enum {
    VIRTIO_STATUS_ACKNOWLEDGE        = 1,
    VIRTIO_STATUS_DRIVER             = 2,
    VIRTIO_STATUS_DRIVER_OK          = 4,
    VIRTIO_STATUS_FEATURES_OK        = 8,
    VIRTIO_STATUS_DEVICE_NEEDS_RESET = 64,
    VIRTIO_STATUS_FAILED             = 128,
};

class IOVirtIOTransport {
public:
    IOVirtIOTransport();

    // Maps the common/notify/isr/device config PCI capabilities and
    // performs ACKNOWLEDGE|DRIVER. Caller does feature negotiation next
    // (negotiateFeatures), then setDriverOk() once virtqueues are set up.
    bool attach(IOPCIDevice *pci);
    void detach();

    // Writes the low 32 driver-feature bits (features 0-31); virtio-gpu/
    // blk/net/input/snd all fit their needed feature bits in the low
    // word for our purposes, so a single 32-bit call is enough here.
    bool negotiateFeatures(uint32_t driverFeatureBitsLow);
    uint32_t deviceFeaturesLow();
    void setDriverOk();
    bool isDeviceOk(); // DRIVER_OK still set, DEVICE_NEEDS_RESET not set

    bool initQueue(VirtQueue *vq, uint16_t index, uint16_t maxSize);
    void freeQueue(VirtQueue *vq);

    // Builds a descriptor chain from `entries[0..count)` in order and
    // publishes it on the avail ring. Returns the head descriptor index.
    uint16_t addDescChain(VirtQueue *vq, const VirtIOChainEntry *entries, unsigned count);

    void notify(VirtQueue *vq);

    // Polls the used ring for the next completion. Returns false on
    // timeout. Non-blocking variant (timeoutMs == 0) checks once and
    // returns immediately - useful for drivers that want to poll several
    // queues in one loop iteration instead of blocking on each. outId is
    // the head descriptor index of the completed chain (as returned by
    // addDescChain) - needed by drivers like net's RX ring that keep
    // multiple buffers outstanding at once and must know which one the
    // device just filled.
    bool pollForCompletion(VirtQueue *vq, unsigned timeoutMs, uint32_t *outLen = 0, uint16_t *outId = 0);

    volatile uint8_t *deviceConfig() const { return fDeviceCfg; }
    uint8_t readIsr(); // clears the ISR status bits on read, per spec

private:
    IOPCIDevice       *fPCIDevice;
    IOMemoryMap       *fCommonCfgMap;
    IOMemoryMap       *fNotifyCfgMap;
    IOMemoryMap       *fIsrCfgMap;
    IOMemoryMap       *fDeviceCfgMap;
    volatile uint8_t  *fCommonCfg;
    volatile uint8_t  *fNotifyCfg;
    volatile uint8_t  *fIsrCfg;
    volatile uint8_t  *fDeviceCfg;
    uint32_t           fNotifyOffMultiplier;
};
