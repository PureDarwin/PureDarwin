#include <IOKit/IOService.h>
#include <mach/kmod.h>

extern "C" {
    int cd9660_module_start(kmod_info_t *ki, void *data);
    int cd9660_module_stop(kmod_info_t *ki, void *data);
}

class com_apple_filesystems_cd9660 : public IOService
{
    OSDeclareDefaultStructors(com_apple_filesystems_cd9660)
public:
    virtual bool start(IOService *provider) APPLE_KEXT_OVERRIDE;
    virtual void stop(IOService *provider) APPLE_KEXT_OVERRIDE;
};

#define super IOService
OSDefineMetaClassAndStructors(com_apple_filesystems_cd9660, IOService)

bool
com_apple_filesystems_cd9660::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    if (cd9660_module_start(NULL, NULL) != KERN_SUCCESS)
        return false;
    registerService();
    return true;
}

void
com_apple_filesystems_cd9660::stop(IOService *provider)
{
    cd9660_module_stop(NULL, NULL);
    super::stop(provider);
}
