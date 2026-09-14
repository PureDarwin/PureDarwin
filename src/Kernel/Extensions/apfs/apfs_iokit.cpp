/* Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT */

#include <IOKit/IOService.h>

extern "C" {
    int apfs_vfs_register(void);
    int apfs_vfs_unregister(void);
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
