/*
 * ApfsFileSystemDriver - boot-uuid-media publisher for an APFS boot volume.
 *
 * PureDarwin's apfs.kext registers the "apfs" filesystem, but root mount blocks
 * on the "boot-uuid-media" IOResource, which is published by a driver that
 * recognizes the boot volume and matches its UUID against the boot-uuid the
 * loader put in /chosen.
 *
 * AppleFileSystemDriver already has an APFS path, but it only triggers on
 * media that is an AppleAPFSVolume - it expects Apple's apfs.kext to have
 * published an IOMedia per volume first. Ours is a VFS-only scaffold that
 * publishes no volume media, so that path never fires. This is the direct
 * analogue of Ext4FileSystemDriver: match the raw container media, read the
 * container superblock, compare nx_uuid against boot-uuid, publish.
 *
 * The container UUID is used rather than a volume UUID because it sits at a
 * fixed offset in block zero, so both this driver and the loader can read it
 * without walking object maps.
 */
#ifdef KERNEL
#ifdef __cplusplus

#include <IOKit/IOService.h>
#include <IOKit/storage/IOMedia.h>
#include <uuid/uuid.h>

class ApfsFileSystemDriver : public IOService
{
    OSDeclareDefaultStructors(ApfsFileSystemDriver)

protected:
    IONotifier *_notifier;
    uuid_t      _uuid;
    OSString   *_uuidString;
    UInt32      _matched;

public:
    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual void free() APPLE_KEXT_OVERRIDE;

private:
    static bool mediaNotificationHandler(void * target, void * ref,
                                         IOService * newService,
                                         IONotifier * notifier);
    /* Read block zero and copy nx_uuid out. Returns kIOReturnSuccess only if
     * the container magic 'NXSB' is present. */
    static IOReturn readApfsUUID(IOMedia *media, uuid_t uuidOut);
};

#endif /* __cplusplus */
#endif /* KERNEL */
