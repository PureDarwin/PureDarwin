/*
 * ApfsFileSystemDriver.cpp - publish boot-uuid-media for an APFS boot volume.
 * Structured after Ext4FileSystemDriver: the container superblock sits in block
 * zero of the partition and carries a full 16-byte nx_uuid, so matching is a
 * direct compare with no hashing.
 */
#include "ApfsFileSystemDriver.h"

#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <libkern/OSByteOrder.h>
#include <libkern/OSAtomic.h>

#define kClassName        "ApfsFileSystemDriver"
#define kMediaMatchKey    "media-match"
#define kBootUUIDKey      "boot-uuid"
#define kBootUUIDMediaKey "boot-uuid-media"

/* APFS container superblock: block zero of the partition. obj_phys_t occupies
 * the first 32 bytes, then nx_magic 'NXSB' at 32 and nx_uuid at 72
 * (spec "Container / nx_superblock_t"). */
#define APFS_SB_OFFSET    0
#define APFS_SB_MAGIC_OFF 32
#define APFS_SB_UUID_OFF  72
#define APFS_NX_MAGIC     0x4253584EU   /* 'NXSB' little-endian */

#define AFD_LOG(fmt, args...)  kprintf(kClassName ": " fmt "\n", ## args)

#define super IOService
OSDefineMetaClassAndStructors(ApfsFileSystemDriver, IOService)

IOReturn
ApfsFileSystemDriver::readApfsUUID(IOMedia *media, uuid_t uuidOut)
{
    bool                       mediaIsOpen = false;
    UInt64                     mediaBlockSize;
    IOBufferMemoryDescriptor * buffer = NULL;
    uint8_t *                  bytes;
    UInt64                     bufferReadAt;
    UInt64                     sbInBuffer;
    vm_size_t                  bufferSize;
    IOReturn                   status = kIOReturnError;

    do {
        mediaBlockSize = media->getPreferredBlockSize();
        if (mediaBlockSize == 0) break;

        /* The superblock is block zero, so one media block covers it. */
        bufferReadAt = IOTrunc(APFS_SB_OFFSET, mediaBlockSize);
        sbInBuffer   = APFS_SB_OFFSET - bufferReadAt;
        bufferSize   = IORound(sbInBuffer + 0x100, mediaBlockSize);

        buffer = IOBufferMemoryDescriptor::withCapacity(bufferSize, kIODirectionIn);
        if (buffer == NULL) break;
        bytes = (uint8_t *)buffer->getBytesNoCopy();

        mediaIsOpen = media->open(media, 0, kIOStorageAccessReader);
        if (!mediaIsOpen) break;

        status = media->read(media, bufferReadAt, buffer);
        if (status != kIOReturnSuccess) break;

        uint8_t *sb = bytes + sbInBuffer;
        uint32_t magic;

        memcpy(&magic, sb + APFS_SB_MAGIC_OFF, sizeof(magic));
        if (OSSwapLittleToHostInt32(magic) != APFS_NX_MAGIC) {
            status = kIOReturnBadMedia;
            break;
        }
        memcpy(uuidOut, sb + APFS_SB_UUID_OFF, sizeof(uuid_t));
        status = kIOReturnSuccess;
    } while (false);

    if (mediaIsOpen) media->close(media);
    if (buffer)      buffer->release();
    return status;
}

bool
ApfsFileSystemDriver::mediaNotificationHandler(void * target, void * /*ref*/,
                                               IOService * service,
                                               IONotifier * /*notifier*/)
{
    ApfsFileSystemDriver *fs;
    IOMedia              *media;
    uuid_t                uuid;
    bool                  matched = false;

    do {
        fs = OSDynamicCast(ApfsFileSystemDriver, (IOService *)target);
        if (fs == NULL) break;
        media = OSDynamicCast(IOMedia, service);
        if (media == NULL) break;
        if (!media->isFormatted()) break;

        if (readApfsUUID(media, uuid) != kIOReturnSuccess) break;

        if (uuid_compare(uuid, fs->_uuid) == 0) {
            AFD_LOG("nx_uuid matched on container %s", media->getName());
            matched = true;
        }
    } while (false);

    if (matched) {
        if (OSCompareAndSwap(false, true, &fs->_matched) != true)
            return false;
        if (fs->_notifier != NULL) {
            fs->_notifier->remove();
            fs->_notifier = NULL;
        }
        AFD_LOG("publishing boot-uuid-media '%s'", media->getName());
        IOService::publishResource(kBootUUIDMediaKey, media);
        fs->getResourceService()->removeProperty(kBootUUIDKey);
        fs->terminate(kIOServiceRequired);
        fs->release();   /* drop the retain taken for async notification */
        return true;
    }
    return false;
}

bool
ApfsFileSystemDriver::start(IOService * provider)
{
    OSDictionary *matching;
    OSString     *uuidString;
    IOService    *resourceService;
    OSDictionary *dict;

    _matched = false;

    do {
        resourceService = getResourceService();
        if (resourceService == NULL) break;

        uuidString = OSDynamicCast(OSString, resourceService->getProperty(kBootUUIDKey));
        if (uuidString == NULL) {
            AFD_LOG("no boot-uuid property; nothing to match");
            break;
        }
        _uuidString = uuidString;
        _uuidString->retain();
        if (uuid_parse(uuidString->getCStringNoCopy(), _uuid) != 0) {
            AFD_LOG("invalid boot-uuid '%s'", uuidString->getCStringNoCopy());
            break;
        }

        dict = OSDynamicCast(OSDictionary, getProperty(kMediaMatchKey));
        if (dict == NULL) break;
        dict = OSDictionary::withDictionary(dict);
        if (dict == NULL) break;

        matching = IOService::serviceMatching("IOMedia", dict);
        if (matching == NULL) break;

        retain();   /* held until we match (or free) */
        _notifier = IOService::addMatchingNotification(gIOMatchedNotification, matching,
                                                       &mediaNotificationHandler, this, 0);
        matching->release();
        AFD_LOG("watching for APFS boot container (boot-uuid %s)",
                _uuidString->getCStringNoCopy());
        return true;
    } while (false);

    return false;
}

void
ApfsFileSystemDriver::free()
{
    if (_notifier) _notifier->remove();
    if (_uuidString) _uuidString->release();
    super::free();
}
