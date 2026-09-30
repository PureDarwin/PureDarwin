/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include <IOKit/IOService.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOKitKeys.h>
#include <sys/types.h>

extern "C" {
    int apfs_vfs_register(void);
    int apfs_vfs_unregister(void);
    int apfs_volume_slot_for_dev(dev_t dev, uint32_t *slot);
    int apfs_bsd_name_for_dev(dev_t dev, char *buf, size_t len);
    uint16_t apfs_role_for_dev(dev_t dev);
    int apfs_role_dev(dev_t dev, uint16_t role, dev_t *out, char *bsd, size_t len);
}

// BSD name ("disk2s1") of the IOMedia with this major/minor. The root mount's vnode has none
int
apfs_bsd_name_for_dev(dev_t dev, char *buf, size_t len)
{
    OSDictionary *match = IOService::serviceMatching("IOMedia");
    OSDictionary *props = OSDictionary::withCapacity(2);
    OSNumber *maj = OSNumber::withNumber((unsigned long long)major(dev), 32);
    OSNumber *min = OSNumber::withNumber((unsigned long long)minor(dev), 32);
    IOService *svc = NULL;
    int found = -1;

    if (len > 0)
        buf[0] = '\0';
    if (match != NULL && props != NULL && maj != NULL && min != NULL) {
        props->setObject(kIOBSDMajorKey, maj);
        props->setObject(kIOBSDMinorKey, min);
        match->setObject(gIOPropertyMatchKey, props);
        svc = IOService::copyMatchingService(match);
    }
    if (svc != NULL) {
        OSString *name = OSDynamicCast(OSString, svc->getProperty(kIOBSDNameKey));

        if (name != NULL && len > 0) {
            strlcpy(buf, name->getCStringNoCopy(), len);
            found = 0;
        }
        svc->release();
    }
    OSSafeReleaseNULL(match);
    OSSafeReleaseNULL(props);
    OSSafeReleaseNULL(maj);
    OSSafeReleaseNULL(min);
    return found;
}

class com_apple_filesystems_apfs : public IOService
{
    OSDeclareDefaultStructors(com_apple_filesystems_apfs)
public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#define super IOService
OSDefineMetaClassAndStructors(com_apple_filesystems_apfs, IOService)

bool
com_apple_filesystems_apfs::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    if (apfs_vfs_register() != 0)
        return false;
    registerService();
    return true;
}

void
com_apple_filesystems_apfs::stop(IOService *provider)
{
    apfs_vfs_unregister();
    super::stop(provider);
}

int
apfs_volume_slot_for_dev(dev_t dev, uint32_t *slot)
{
    OSDictionary *match = IOService::serviceMatching("IOMedia");
    OSDictionary *props = OSDictionary::withCapacity(2);
    OSNumber *maj = OSNumber::withNumber((unsigned long long)major(dev), 32);
    OSNumber *min = OSNumber::withNumber((unsigned long long)minor(dev), 32);
    IOService *svc = NULL;
    int found = -1;

    *slot = 0;
    if (match != NULL && props != NULL && maj != NULL && min != NULL) {
        props->setObject(kIOBSDMajorKey, maj);
        props->setObject(kIOBSDMinorKey, min);
        match->setObject(gIOPropertyMatchKey, props);
        svc = IOService::copyMatchingService(match);
        match = NULL;                // Consumed
    }
    if (svc != NULL) {
        found = 0;
        if (svc->metaCast("AppleAPFSVolume") != NULL) {
            OSNumber *id = OSDynamicCast(OSNumber,
                svc->getProperty("Partition ID"));

            if (id != NULL && id->unsigned32BitValue() >= 1)
                *slot = id->unsigned32BitValue() - 1;
        }
        svc->release();
    }
    if (match != NULL)
        match->release();
    if (props != NULL)
        props->release();
    if (maj != NULL)
        maj->release();
    if (min != NULL)
        min->release();
    return found;
}

// the IOMedia with this major/minor, retained, or NULL
static IOService *
apfs_copy_media_for_dev(dev_t dev)
{
    OSDictionary *match = IOService::serviceMatching("IOMedia");
    OSDictionary *props = OSDictionary::withCapacity(2);
    OSNumber *maj = OSNumber::withNumber((unsigned long long)major(dev), 32);
    OSNumber *min = OSNumber::withNumber((unsigned long long)minor(dev), 32);
    IOService *svc = NULL;

    if (match != NULL && props != NULL && maj != NULL && min != NULL) {
        props->setObject(kIOBSDMajorKey, maj);
        props->setObject(kIOBSDMinorKey, min);
        match->setObject(gIOPropertyMatchKey, props);
        svc = IOService::copyMatchingService(match);
    }
    OSSafeReleaseNULL(match);
    OSSafeReleaseNULL(props);
    OSSafeReleaseNULL(maj);
    OSSafeReleaseNULL(min);
    return svc;
}

uint16_t
apfs_role_for_dev(dev_t dev)
{
    IOService *svc = apfs_copy_media_for_dev(dev);
    uint16_t role = 0;

    if (svc != NULL) {
        OSNumber *rv = OSDynamicCast(OSNumber, svc->getProperty("RoleValue"));

        if (svc->metaCast("AppleAPFSVolume") != NULL && rv != NULL)
            role = rv->unsigned16BitValue();
        svc->release();
    }
    return role;
}

// the volumes hang off the AppleAPFSContainer that is dev's provider
int
apfs_role_dev(dev_t dev, uint16_t role, dev_t *out, char *bsd, size_t len)
{
    IOService *svc = apfs_copy_media_for_dev(dev);
    IOService *cont = svc != NULL ? svc->getProvider() : NULL;
    OSIterator *it = cont != NULL ? cont->getChildIterator(gIOServicePlane) : NULL;
    OSObject *o;
    int found = -1;

    if (bsd != NULL && len > 0)
        bsd[0] = '\0';
    while (it != NULL && found != 0 && (o = it->getNextObject()) != NULL) {
        IOService *vol = OSDynamicCast(IOService, o);
        OSNumber *rv, *maj, *min;
        OSString *name;

        if (vol == NULL || vol->metaCast("AppleAPFSVolume") == NULL)
            continue;
        rv = OSDynamicCast(OSNumber, vol->getProperty("RoleValue"));
        maj = OSDynamicCast(OSNumber, vol->getProperty(kIOBSDMajorKey));
        min = OSDynamicCast(OSNumber, vol->getProperty(kIOBSDMinorKey));
        name = OSDynamicCast(OSString, vol->getProperty(kIOBSDNameKey));
        if (rv == NULL || rv->unsigned16BitValue() != role || maj == NULL || min == NULL)
            continue;
        if (out != NULL)
            *out = makedev(maj->unsigned32BitValue(), min->unsigned32BitValue());
        if (bsd != NULL && len > 0 && name != NULL)
            strlcpy(bsd, name->getCStringNoCopy(), len);
        found = 0;
    }
    OSSafeReleaseNULL(it);
    OSSafeReleaseNULL(svc);
    return found;
}
