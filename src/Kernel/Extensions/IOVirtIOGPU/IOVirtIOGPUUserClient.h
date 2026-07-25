#pragma once

#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

class IOVirtIOGPU;

class IOVirtIOGPUUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOVirtIOGPUUserClient);

    enum { kMaxResources = 1024, kMaxContexts = 64 };

    struct ResEntry {
        uint32_t                  resId;
        IOBufferMemoryDescriptor *backing; // mapped to userland via clientMemoryForType
    };

private:
    IOVirtIOGPU *fOwner;
    task_t       fTask;

    ResEntry     fRes[kMaxResources];
    uint32_t     fResCount;
    uint32_t     fCtx[kMaxContexts];
    uint32_t     fCtxCount;

    ResEntry *findRes(uint32_t resId);
    void      destroyAll();

    // Selector handlers.
    IOReturn mCreateContext(IOExternalMethodArguments *a);
    IOReturn mDestroyContext(IOExternalMethodArguments *a);
    IOReturn mCreateResource(IOExternalMethodArguments *a);
    IOReturn mDestroyResource(IOExternalMethodArguments *a);
    IOReturn mAttachResource(IOExternalMethodArguments *a);
    IOReturn mTransfer(IOExternalMethodArguments *a, bool toHost);
    IOReturn mSubmitCmd(IOExternalMethodArguments *a);
    IOReturn mGetCaps(IOExternalMethodArguments *a);

public:
    static IOVirtIOGPUUserClient *withOwner(IOVirtIOGPU *owner, task_t task);

    virtual bool     start(IOService *provider) override;
    virtual void     stop(IOService *provider) override;
    virtual IOReturn clientClose(void) override;

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) override;

    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                         IOMemoryDescriptor **memory) override;
};
