#pragma once

#include <IOKit/IOUserClient.h>
#include <IOKit/IOMemoryDescriptor.h>

class IOVirtIOGPU;

// the 'vnus' connection: one 3D context, blobs mapped from the host visible window and per-ring fenced submits.
// blobs live in the device so other connections can import them, libdrm-virtgpu turns drm ioctls into these
class IOVirtIOGPUVenusClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOVirtIOGPUVenusClient);

    enum { kMaxResources = 4096, kMaxFenceQuery = 512, kMaxSubmitBytes = 8u << 20 };

    struct Res {
        uint32_t resId;
        uint32_t blobMem;
        uint32_t blobFlags;
        uint32_t mapInfo;
        uint64_t size;
    };

private:
    IOVirtIOGPU *fOwner;
    task_t       fTask;
    IOLock      *fLock;
    uint32_t     fCtxId;
    Res         *fRes;
    uint32_t     fResCount;

    Res     *findRes(uint32_t resId);
    void     releaseRes(Res *r);
    void     destroyAll();

    IOReturn mGetInfo(IOExternalMethodArguments *a);
    IOReturn mGetCaps(IOExternalMethodArguments *a);
    IOReturn mContextInit(IOExternalMethodArguments *a);
    IOReturn mCreateBlob(IOExternalMethodArguments *a);
    IOReturn mResourceInfo(IOExternalMethodArguments *a);
    IOReturn mUnref(IOExternalMethodArguments *a);
    IOReturn mMap(IOExternalMethodArguments *a);
    IOReturn mSubmit(IOExternalMethodArguments *a);
    IOReturn mFenceStatus(IOExternalMethodArguments *a);
    IOReturn mWaitFence(IOExternalMethodArguments *a);
    IOReturn mWaitProgress(IOExternalMethodArguments *a);
    IOReturn mExport(IOExternalMethodArguments *a);
    IOReturn mImport(IOExternalMethodArguments *a);

public:
    static IOVirtIOGPUVenusClient *withOwner(IOVirtIOGPU *owner, task_t task);

    virtual bool     start(IOService *provider) override;
    virtual void     stop(IOService *provider) override;
    virtual void     free() override;
    virtual IOReturn clientClose(void) override;

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) override;

    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                         IOMemoryDescriptor **memory) override;
};
