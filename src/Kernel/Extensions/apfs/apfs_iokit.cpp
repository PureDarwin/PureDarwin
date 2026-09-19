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
