#include "IOVirtIOTransport.h"
#include <IOKit/IOLib.h>

// struct virtio_pci_cap field offsets (generic vendor-capability header)
enum {
    PCI_CAP_ID_VNDR = 0x09,

    VIRTIO_PCI_CAP_COMMON_CFG = 1,
    VIRTIO_PCI_CAP_NOTIFY_CFG = 2,
    VIRTIO_PCI_CAP_ISR_CFG    = 3,
    VIRTIO_PCI_CAP_DEVICE_CFG = 4,

    kCapLen        = 2,
    kCapCfgType    = 3,
    kCapBar        = 4,
    kCapOffset     = 8,
    kCapLength     = 12,
    kCapNotifyMult = 16, // only present on VIRTIO_PCI_CAP_NOTIFY_CFG
};

// struct virtio_pci_common_cfg field offsets
enum {
    kCommonDeviceFeatureSelect = 0,
    kCommonDeviceFeature       = 4,
    kCommonDriverFeatureSelect = 8,
    kCommonDriverFeature       = 12,
    kCommonDeviceStatus        = 20,
    kCommonQueueSelect         = 22,
    kCommonQueueSize           = 24,
    kCommonQueueEnable         = 28,
    kCommonQueueNotifyOff      = 30,
    kCommonQueueDesc           = 32,
    kCommonQueueDriver         = 40,
    kCommonQueueDevice         = 48,
};

static inline void w8(volatile uint8_t *p, uint8_t v)   { *p = v; }
static inline void w16(volatile uint8_t *p, uint16_t v) { *(volatile uint16_t *)p = v; }
static inline void w32(volatile uint8_t *p, uint32_t v) { *(volatile uint32_t *)p = v; }
static inline void w64(volatile uint8_t *p, uint64_t v) { *(volatile uint64_t *)p = v; }
static inline uint8_t  r8(volatile uint8_t *p)  { return *p; }
static inline uint16_t r16(volatile uint8_t *p) { return *(volatile uint16_t *)p; }
static inline uint32_t r32(volatile uint8_t *p) { return *(volatile uint32_t *)p; }

// Standard PCI BAR sizing dance: disable decode, write all-ones, read
// back the size mask, restore. Needed because QEMU frequently mis-tags
// or omits IODeviceMemory ranges for these BARs.
static bool readMemoryBAR(IOPCIDevice *pci, UInt8 reg, uint64_t *outBase, uint64_t *outSize)
{
    if (!pci || !outBase || !outSize) return false;

    const uint16_t savedCmd = pci->configRead16(kIOPCIConfigCommand);
    const uint32_t savedLo  = pci->configRead32(reg);
    uint32_t savedHi = 0;

    if (savedLo & 0x1) return false; // I/O space BAR, not memory

    const bool is64 = ((savedLo & 0x6) == 0x4);
    if (is64) {
        if (reg > kIOPCIConfigBaseAddress4) return false;
        savedHi = pci->configRead32(reg + 4);
    }

    pci->configWrite16(kIOPCIConfigCommand, savedCmd & ~(uint16_t)0x3);
    pci->configWrite32(reg, 0xffffffffU);
    if (is64) pci->configWrite32(reg + 4, 0xffffffffU);

    const uint32_t maskLo = pci->configRead32(reg);
    const uint32_t maskHi = is64 ? pci->configRead32(reg + 4) : 0xffffffffU;

    pci->configWrite32(reg, savedLo);
    if (is64) pci->configWrite32(reg + 4, savedHi);
    pci->configWrite16(kIOPCIConfigCommand, savedCmd);

    uint64_t base = savedLo & ~0x0fULL;
    uint64_t sizeMask = maskLo & ~0x0fULL;
    if (is64) {
        base |= ((uint64_t)savedHi << 32);
        sizeMask |= ((uint64_t)maskHi << 32);
    }
    if (!base || !sizeMask) return false;

    uint64_t size = (~sizeMask) + 1;
    if (!is64) size &= 0xffffffffULL;
    if (size < 0x1000) size = 0x1000;
    if (size > 0x10000000ULL) return false;

    *outBase = base;
    *outSize = size;
    return true;
}

static IOMemoryMap *mapBar(IOPCIDevice *pci, uint8_t bar)
{
    const uint8_t reg = kIOPCIConfigBaseAddress0 + (bar * 4);
    IODeviceMemory *dm = pci->getDeviceMemoryWithRegister(reg);
    IOMemoryMap *map = NULL;
    if (dm && dm->getLength())
        map = dm->map(kIOMapAnywhere);
    if (!map) {
        uint64_t base = 0, size = 0;
        if (readMemoryBAR(pci, reg, &base, &size)) {
            IOMemoryDescriptor *desc = IOMemoryDescriptor::withPhysicalAddress(
                (IOPhysicalAddress)base, (IOByteCount)size,
                kIODirectionNone | kIOMemoryMapperNone);
            if (desc) {
                map = desc->map(kIOMapAnywhere);
                desc->release();
            }
        }
    }
    return map;
}

IOVirtIOTransport::IOVirtIOTransport()
    : fPCIDevice(0), fCommonCfgMap(0), fNotifyCfgMap(0), fIsrCfgMap(0), fDeviceCfgMap(0),
      fCommonCfg(0), fNotifyCfg(0), fIsrCfg(0), fDeviceCfg(0), fNotifyOffMultiplier(0)
{
}

bool
IOVirtIOTransport::attach(IOPCIDevice *pci)
{
    fPCIDevice = pci;

    // Enable decode before touching any BAR - mapping (and even the
    // fallback BAR-sizing dance's temporary command-register state) can
    // silently fail or read garbage with Memory Space Enable still off.
    pci->setBusMasterEnable(true);
    pci->setMemoryEnable(true);

    IOByteCount capOffset = 0;
    UInt32 found;
    unsigned nCaps = 0;
    while ((found = pci->extendedFindPCICapability(PCI_CAP_ID_VNDR, &capOffset)) != 0) {
        nCaps++;
        // capOffset is the capability's own start (cap_vndr byte); field
        // offsets below are relative to it, per struct virtio_pci_cap.
        UInt8 base = (UInt8)capOffset;
        UInt8 cfgType = pci->configRead8((UInt8)(base + kCapCfgType));
        UInt8 bar     = pci->configRead8((UInt8)(base + kCapBar));
        UInt32 off    = pci->configRead32((UInt8)(base + kCapOffset));
        IOLog("IOVirtIOTransport: vendor cap #%u at off=0x%x type=%u bar=%u regoff=0x%x\n",
              nCaps, (unsigned)capOffset, cfgType, bar, off);

        IOMemoryMap *map = mapBar(pci, bar);
        if (!map) {
            IOLog("IOVirtIOTransport: failed to map BAR%u for cap type=%u\n", bar, cfgType);
            continue;
        }
        volatile uint8_t *base_va = (volatile uint8_t *)map->getVirtualAddress() + off;

        switch (cfgType) {
        case VIRTIO_PCI_CAP_COMMON_CFG:
            fCommonCfgMap = map; fCommonCfg = base_va;
            break;
        case VIRTIO_PCI_CAP_NOTIFY_CFG:
            fNotifyCfgMap = map; fNotifyCfg = base_va;
            fNotifyOffMultiplier = pci->configRead32((UInt8)(base + kCapNotifyMult));
            break;
        case VIRTIO_PCI_CAP_ISR_CFG:
            fIsrCfgMap = map; fIsrCfg = base_va;
            break;
        case VIRTIO_PCI_CAP_DEVICE_CFG:
            fDeviceCfgMap = map; fDeviceCfg = base_va;
            break;
        default:
            map->release();
            break;
        }
    }

    if (!fCommonCfg || !fNotifyCfg) {
        IOLog("IOVirtIOTransport: attach failed (nCaps=%u commonCfg=%p notifyCfg=%p)\n",
              nCaps, (void *)fCommonCfg, (void *)fNotifyCfg);
        return false;
    }

    // VIRTIO 1.1 sec 3.1.1 device initialization handshake.
    w8(fCommonCfg + kCommonDeviceStatus, 0); // reset
    w8(fCommonCfg + kCommonDeviceStatus, VIRTIO_STATUS_ACKNOWLEDGE);
    w8(fCommonCfg + kCommonDeviceStatus, VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER);
    return true;
}

void
IOVirtIOTransport::detach()
{
    if (fCommonCfg)
        w8(fCommonCfg + kCommonDeviceStatus, 0); // reset the device

    if (fCommonCfgMap) { fCommonCfgMap->release(); fCommonCfgMap = 0; }
    if (fNotifyCfgMap) { fNotifyCfgMap->release(); fNotifyCfgMap = 0; }
    if (fIsrCfgMap)    { fIsrCfgMap->release();    fIsrCfgMap = 0; }
    if (fDeviceCfgMap) { fDeviceCfgMap->release();  fDeviceCfgMap = 0; }
    fCommonCfg = fNotifyCfg = fIsrCfg = fDeviceCfg = 0;
    fPCIDevice = 0;
}

uint32_t
IOVirtIOTransport::deviceFeaturesLow()
{
    w32(fCommonCfg + kCommonDeviceFeatureSelect, 0);
    return r32(fCommonCfg + kCommonDeviceFeature);
}

bool
IOVirtIOTransport::negotiateFeatures(uint32_t driverFeatureBitsLow)
{
    w32(fCommonCfg + kCommonDriverFeatureSelect, 0);
    w32(fCommonCfg + kCommonDriverFeature, driverFeatureBitsLow);
    // Word 1 (features 32-63): always ack VIRTIO_F_VERSION_1 (bit 32,
    // i.e. bit 0 of this word) - any driver using this modern-PCI-
    // capability transport is implicitly a 1.0 driver, and un-acked
    // devices may refuse FEATURES_OK or fall back to legacy framing
    // (e.g. virtio-net's 10 vs 12-byte header) that we don't implement.
    w32(fCommonCfg + kCommonDriverFeatureSelect, 1);
    w32(fCommonCfg + kCommonDriverFeature, 0x1);

    w8(fCommonCfg + kCommonDeviceStatus,
       VIRTIO_STATUS_ACKNOWLEDGE | VIRTIO_STATUS_DRIVER | VIRTIO_STATUS_FEATURES_OK);
    return (r8(fCommonCfg + kCommonDeviceStatus) & VIRTIO_STATUS_FEATURES_OK) != 0;
}

void
IOVirtIOTransport::setDriverOk()
{
    uint8_t status = r8(fCommonCfg + kCommonDeviceStatus);
    w8(fCommonCfg + kCommonDeviceStatus, status | VIRTIO_STATUS_DRIVER_OK);
}

bool
IOVirtIOTransport::isDeviceOk()
{
    uint8_t status = r8(fCommonCfg + kCommonDeviceStatus);
    return (status & VIRTIO_STATUS_DRIVER_OK) && !(status & VIRTIO_STATUS_DEVICE_NEEDS_RESET);
}

uint8_t
IOVirtIOTransport::readIsr()
{
    return fIsrCfg ? r8(fIsrCfg) : 0;
}

bool
IOVirtIOTransport::initQueue(VirtQueue *vq, uint16_t index, uint16_t size)
{
    bzero(vq, sizeof(*vq));
    vq->queueIndex = index;

    w16(fCommonCfg + kCommonQueueSelect, index);
    uint16_t maxSize = r16(fCommonCfg + kCommonQueueSize);
    if (maxSize == 0)
        return false;
    if (size > maxSize) size = maxSize;
    vq->queueSize = size;

    size_t descBytes  = sizeof(VRingDesc) * size;
    size_t availBytes = sizeof(VRingAvailHdr) + sizeof(uint16_t) * size;
    size_t usedBytes  = sizeof(VRingUsedHdr) + sizeof(VRingUsedElem) * size;

    vq->descMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, descBytes, 0xFFFFFFFFULL);
    vq->availMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, availBytes, 0xFFFFFFFFULL);
    vq->usedMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, usedBytes, 0xFFFFFFFFULL);
    if (!vq->descMem || !vq->availMem || !vq->usedMem)
        return false;

    vq->desc  = vq->descMem->getBytesNoCopy();
    vq->avail = vq->availMem->getBytesNoCopy();
    vq->used  = vq->usedMem->getBytesNoCopy();
    bzero(vq->desc, descBytes);
    bzero(vq->avail, availBytes);
    bzero(vq->used, usedBytes);

    w64(fCommonCfg + kCommonQueueDesc, vq->descMem->getPhysicalAddress());
    w64(fCommonCfg + kCommonQueueDriver, vq->availMem->getPhysicalAddress());
    w64(fCommonCfg + kCommonQueueDevice, vq->usedMem->getPhysicalAddress());
    w16(fCommonCfg + kCommonQueueSize, vq->queueSize);
    w16(fCommonCfg + kCommonQueueEnable, 1);
    return true;
}

void
IOVirtIOTransport::freeQueue(VirtQueue *vq)
{
    if (vq->descMem)  { vq->descMem->release();  vq->descMem = 0; }
    if (vq->availMem) { vq->availMem->release(); vq->availMem = 0; }
    if (vq->usedMem)  { vq->usedMem->release();  vq->usedMem = 0; }
}

uint16_t
IOVirtIOTransport::addDescChain(VirtQueue *vq, const VirtIOChainEntry *entries, unsigned count)
{
    VRingDesc *desc = (VRingDesc *)vq->desc;
    uint16_t head = 0, prev = 0;

    for (unsigned i = 0; i < count; i++) {
        uint16_t d = (uint16_t)((vq->nextFreeDesc + i) % vq->queueSize);
        if (i == 0) head = d;

        desc[d].addr  = entries[i].addr;
        desc[d].len   = entries[i].len;
        desc[d].flags = entries[i].write ? VRING_DESC_F_WRITE : 0;
        if (i + 1 < count) {
            desc[d].flags |= VRING_DESC_F_NEXT;
            desc[d].next = (uint16_t)((vq->nextFreeDesc + i + 1) % vq->queueSize);
        } else {
            desc[d].next = 0;
        }
        prev = d;
    }
    (void)prev;
    vq->nextFreeDesc = (uint16_t)(vq->nextFreeDesc + count);

    VRingAvailHdr *ah = (VRingAvailHdr *)vq->avail;
    uint16_t *ring = (uint16_t *)((uint8_t *)vq->avail + sizeof(VRingAvailHdr));
    ring[ah->idx % vq->queueSize] = head;
    OSSynchronizeIO();
    ah->idx = (uint16_t)(ah->idx + 1);
    OSSynchronizeIO();

    return head;
}

void
IOVirtIOTransport::notify(VirtQueue *vq)
{
    w16(fCommonCfg + kCommonQueueSelect, vq->queueIndex);
    uint16_t notifyOff = r16(fCommonCfg + kCommonQueueNotifyOff);
    w16(fNotifyCfg + (size_t)notifyOff * fNotifyOffMultiplier, vq->queueIndex);
}

bool
IOVirtIOTransport::pollForCompletion(VirtQueue *vq, unsigned timeoutMs, uint32_t *outLen, uint16_t *outId)
{
    VRingUsedHdr *uh = (VRingUsedHdr *)vq->used;
    unsigned waited = 0;
    while (uh->idx == vq->lastUsedIdx) {
        if (timeoutMs == 0)
            return false; // non-blocking: nothing ready yet
        IOSleep(1);
        if (++waited >= timeoutMs)
            return false;
    }

    if (outLen || outId) {
        VRingUsedElem *ring = (VRingUsedElem *)((uint8_t *)vq->used + sizeof(VRingUsedHdr));
        VRingUsedElem *elem = &ring[vq->lastUsedIdx % vq->queueSize];
        if (outLen) *outLen = elem->len;
        if (outId)  *outId  = (uint16_t)elem->id;
    }
    vq->lastUsedIdx = (uint16_t)(vq->lastUsedIdx + 1);
    return true;
}
