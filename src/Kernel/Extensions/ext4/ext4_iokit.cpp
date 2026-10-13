/*
 * ext4_iokit.cpp - IOService shim that gets the ext4 filesystem registered.
 *
 * In the fileset kernel collection a filesystem kext with only a C kmod
 * MAIN_FUNCTION never has that start routine invoked (nothing matches or
 * depends on it), so vfs_fsadd is never called and root mount fails. hfs
 * works around this with a trivial IOService whose IOKitPersonality matches
 * the "IOBSD" IOResource (published once the BSD subsystem is up); matching
 * loads/starts the kext, and its start() registers the filesystem. This is
 * the ext4 equivalent.
 */
#include <IOKit/IOService.h>

extern "C" {
    int ext4_vfs_register(void);
    int ext4_vfs_unregister(void);
}

class org_puredarwin_filesystems_ext4 : public IOService
{
    OSDeclareDefaultStructors(org_puredarwin_filesystems_ext4)
public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#define super IOService
OSDefineMetaClassAndStructors(org_puredarwin_filesystems_ext4, IOService)

bool
org_puredarwin_filesystems_ext4::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    if (ext4_vfs_register() != 0)
        return false;
    registerService();
    return true;
}

void
org_puredarwin_filesystems_ext4::stop(IOService *provider)
{
    ext4_vfs_unregister();
    super::stop(provider);
}
