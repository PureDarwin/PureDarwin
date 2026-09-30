/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */
#ifdef KERNEL
#ifdef __cplusplus

#include <IOKit/IOService.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/storage/IOMedia.h>
#include <IOKit/storage/IOPartitionScheme.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <uuid/uuid.h>

// Whole-container media (Apple: "diskN")
class AppleAPFSMedia : public IOMedia
{
    OSDeclareDefaultStructors(AppleAPFSMedia)
};

// One APFS volume (Apple: "diskNsM")
class AppleAPFSVolume : public IOMedia
{
    OSDeclareDefaultStructors(AppleAPFSVolume)
};

// Sits on the APFS partition and publishes the AppleAPFSMedia
class AppleAPFSContainerScheme : public IOPartitionScheme
{
    OSDeclareDefaultStructors(AppleAPFSContainerScheme)

protected:
    AppleAPFSMedia *_media;

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;
};

// Sits on the AppleAPFSMedia and publishes one AppleAPFSVolume per volume
class AppleAPFSContainer : public IOPartitionScheme
{
    OSDeclareDefaultStructors(AppleAPFSContainer)

protected:
    OSSet *_volumes;
    UInt32 _blockSize;
    UInt64 _blockCount;
    UInt32 _maxFs;

public:
    UInt32 spaceBlockSize() const { return _blockSize; }
    UInt64 spaceBlockCount() const { return _blockCount; }
    UInt32 spaceMaxVolumes() const { return _maxFs; }
    OSIterator *copyVolumeIterator() const
    {
        return _volumes != NULL ? OSCollectionIterator::withCollection(_volumes) : NULL;
    }

protected:

    IOReturn readBlock(IOMedia *media, UInt64 block, UInt32 blockSize,
                       IOBufferMemoryDescriptor **out);
    IOReturn readLatestSuperblock(IOMedia *media, UInt32 blockSize,
                                  IOBufferMemoryDescriptor **out);
    IOReturn omapLookup(IOMedia *media, UInt32 blockSize, UInt64 treePaddr,
                        UInt64 oid, UInt64 *paddrOut);
    AppleAPFSVolume *publishVolume(IOMedia *media, UInt32 blockSize,
                                   const uint8_t *nx, const uint8_t *vsb,
                                   UInt32 index);
    UInt32 scanVolumes(IOMedia *media, const uint8_t *nx, UInt32 blockSize,
                       UInt64 treePaddr, UInt32 maxFs);
    void rescanVolumes();
    bool anyVolumeOpen();
    UInt64 _slotMask[2];
    uint8_t _nxUuid[16];

public:
    IOReturn createVolume(const char *name, uint16_t role, const uint8_t uuid[16],
                          uint32_t *slot);

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

    // Role of the volume published as diskNs(index)
    IOReturn volumeRole(UInt32 index, UInt16 *role);
};

// the space report selector 8 returns, keyed by BSD name as macOS does
OSDictionary *AppleAPFSContainerCopySpaceInfo(AppleAPFSContainer *container);

class AppleAPFSUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(AppleAPFSUserClient)

protected:
    AppleAPFSContainer *_container;

public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual IOReturn clientClose() APPLE_KEXT_OVERRIDE;
    virtual IOReturn externalMethod(uint32_t selector,
                                    IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch,
                                    OSObject *target,
                                    void *reference) APPLE_KEXT_OVERRIDE;
};

#endif /* __cplusplus */
#endif /* KERNEL */
