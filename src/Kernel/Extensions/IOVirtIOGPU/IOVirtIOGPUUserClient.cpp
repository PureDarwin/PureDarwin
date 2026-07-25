#include "IOVirtIOGPUUserClient.h"
#include "IOVirtIOGPU.h"
#include "IOVirtIOGPU3DShared.h"
#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(IOVirtIOGPUUserClient, IOUserClient);

IOVirtIOGPUUserClient *
IOVirtIOGPUUserClient::withOwner(IOVirtIOGPU *owner, task_t task)
{
    IOVirtIOGPUUserClient *uc = new IOVirtIOGPUUserClient;
    if (!uc)
        return NULL;
    if (!uc->init()) {
        uc->release();
        return NULL;
    }
    uc->fOwner    = owner;
    uc->fTask     = task;
    uc->fResCount = 0;
    uc->fCtxCount = 0;
    return uc;
}

bool
IOVirtIOGPUUserClient::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    fOwner = OSDynamicCast(IOVirtIOGPU, provider);
    return fOwner != NULL;
}

IOVirtIOGPUUserClient::ResEntry *
IOVirtIOGPUUserClient::findRes(uint32_t resId)
{
    for (uint32_t i = 0; i < fResCount; i++)
        if (fRes[i].resId == resId)
            return &fRes[i];
    return NULL;
}

// Destroy every context + resource this client created. Called on close so a
// crashed Mesa process cannot leak host GPU objects.
void
IOVirtIOGPUUserClient::destroyAll()
{
    if (!fOwner)
        return;
    for (uint32_t i = 0; i < fResCount; i++) {
        fOwner->gpuResourceUnref(fRes[i].resId);
        if (fRes[i].backing) {
            fRes[i].backing->release();
            fRes[i].backing = NULL;
        }
    }
    fResCount = 0;
    for (uint32_t i = 0; i < fCtxCount; i++)
        fOwner->gpu3DDestroyContext(fCtx[i]);
    fCtxCount = 0;
}

IOReturn
IOVirtIOGPUUserClient::clientClose(void)
{
    destroyAll();
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

void
IOVirtIOGPUUserClient::stop(IOService *provider)
{
    destroyAll();
    super::stop(provider);
}

IOReturn
IOVirtIOGPUUserClient::mCreateContext(IOExternalMethodArguments *a)
{
    if (fCtxCount >= kMaxContexts)
        return kIOReturnNoResources;
    uint32_t ctxId = fOwner->allocContextId();
    if (!fOwner->gpu3DCreateContext(ctxId, "pd-mesa"))
        return kIOReturnIOError;
    fCtx[fCtxCount++] = ctxId;
    a->scalarOutput[0] = ctxId;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUUserClient::mDestroyContext(IOExternalMethodArguments *a)
{
    uint32_t ctxId = (uint32_t)a->scalarInput[0];
    fOwner->gpu3DDestroyContext(ctxId);
    for (uint32_t i = 0; i < fCtxCount; i++) {
        if (fCtx[i] == ctxId) {
            fCtx[i] = fCtx[--fCtxCount];
            break;
        }
    }
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUUserClient::mCreateResource(IOExternalMethodArguments *a)
{
    if (a->structureInputSize < sizeof(PDVirglResourceCreate))
        return kIOReturnBadArgument;
    if (fResCount >= kMaxResources)
        return kIOReturnNoResources;

    const PDVirglResourceCreate *rc = (const PDVirglResourceCreate *)a->structureInput;
    uint32_t bpp = rc->bytes_per_pixel ? rc->bytes_per_pixel : 4;
    uint32_t h   = rc->height ? rc->height : 1;
    uint64_t size = rc->size ? rc->size : (uint64_t)rc->width * h * bpp;
    if (size == 0 || size > (256u << 20))   // 256 MB sanity cap
        return kIOReturnBadArgument;

    IOBufferMemoryDescriptor *back = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        size, 0xFFFFFFFFULL);
    if (!back)
        return kIOReturnNoMemory;
    bzero(back->getBytesNoCopy(), size);

    uint32_t resId = fOwner->allocResourceId();
    if (!fOwner->gpu3DCreateResource(resId, rc->target, rc->format, rc->bind,
                                     rc->width, rc->height) ||
        !fOwner->gpuAttachBacking(resId, back->getPhysicalAddress(), (uint32_t)size)) {
        back->release();
        return kIOReturnIOError;
    }

    fRes[fResCount].resId   = resId;
    fRes[fResCount].backing = back;
    fResCount++;

    a->scalarOutput[0] = resId;
    a->scalarOutput[1] = size;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUUserClient::mDestroyResource(IOExternalMethodArguments *a)
{
    uint32_t resId = (uint32_t)a->scalarInput[0];
    for (uint32_t i = 0; i < fResCount; i++) {
        if (fRes[i].resId == resId) {
            fOwner->gpuResourceUnref(resId);
            if (fRes[i].backing) fRes[i].backing->release();
            fRes[i] = fRes[--fResCount];
            return kIOReturnSuccess;
        }
    }
    return kIOReturnNotFound;
}

IOReturn
IOVirtIOGPUUserClient::mAttachResource(IOExternalMethodArguments *a)
{
    uint32_t ctxId = (uint32_t)a->scalarInput[0];
    uint32_t resId = (uint32_t)a->scalarInput[1];
    if (!findRes(resId))
        return kIOReturnNotFound;
    return fOwner->gpu3DCtxAttachResource(ctxId, resId) ? kIOReturnSuccess
                                                        : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::mTransfer(IOExternalMethodArguments *a, bool toHost)
{
    if (a->structureInputSize < sizeof(PDVirglTransfer))
        return kIOReturnBadArgument;
    const PDVirglTransfer *t = (const PDVirglTransfer *)a->structureInput;
    if (!findRes(t->resource_id))
        return kIOReturnNotFound;
    return fOwner->gpu3DTransfer(toHost, t->ctx_id, t->resource_id,
                                 t->x, t->y, t->z, t->w, t->h, t->d,
                                 t->level, t->stride, t->offset, t->fence_id)
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::mSubmitCmd(IOExternalMethodArguments *a)
{
    uint32_t ctxId   = (uint32_t)a->scalarInput[0];
    uint64_t fenceId = a->scalarInput[1];

    // The command stream arrives inline (small) or via an ool descriptor
    // (large). Bounce-copy into a physically-contiguous kernel buffer before
    // handing its phys to the device (v1 - safe over fast).
    const void *src = NULL;
    IOByteCount len = 0;
    IOMemoryMap *map = NULL;
    IOMemoryDescriptor *ool = a->structureInputDescriptor;
    if (ool) {
        if (ool->prepare(kIODirectionOut) != kIOReturnSuccess)
            return kIOReturnVMError;
        map = ool->map();
        if (!map) { ool->complete(); return kIOReturnVMError; }
        src = (const void *)map->getVirtualAddress();
        len = ool->getLength();
    } else {
        src = a->structureInput;
        len = a->structureInputSize;
    }
    if (!src || len == 0) {
        if (map) map->release();
        if (ool) ool->complete();
        return kIOReturnBadArgument;
    }

    IOReturn ret = kIOReturnNoMemory;
    IOBufferMemoryDescriptor *bounce = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, len, 0xFFFFFFFFULL);
    if (bounce) {
        memcpy(bounce->getBytesNoCopy(), src, len);
        ret = fOwner->gpu3DSubmit(ctxId, bounce->getPhysicalAddress(), (uint32_t)len,
                                  fenceId) ? kIOReturnSuccess : kIOReturnIOError;
        bounce->release();
    }

    if (map) map->release();
    if (ool) ool->complete();
    return ret;
}

IOReturn
IOVirtIOGPUUserClient::mGetCaps(IOExternalMethodArguments *a)
{
    uint32_t len = 0;
    const uint8_t *caps = fOwner->virglCaps(&len);
    if (!caps || len == 0)
        return kIOReturnUnsupported;
    // Truncate to the caller's buffer rather than failing: the host caps blob
    // (virgl_caps_v2) may be larger than the client's union virgl_caps when the
    // host virglrenderer is newer than the guest Mesa. The layout is
    // append-only, so copying the leading min() bytes gives Mesa a valid caps
    // struct (it only reads its own fields); failing here instead would leave
    // Mesa with default caps whose format bitmasks are all zero -> no GLX visual.
    uint32_t copyLen = len;
    if (a->structureOutputSize < copyLen)
        copyLen = (uint32_t)a->structureOutputSize;
    memcpy(a->structureOutput, caps, copyLen);
    a->structureOutputSize = copyLen;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                      IOExternalMethodDispatch *, OSObject *, void *)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    switch (selector) {
    case kPDVirgl_GetCaps:         return mGetCaps(args);
    case kPDVirgl_CreateContext:   return mCreateContext(args);
    case kPDVirgl_DestroyContext:  return mDestroyContext(args);
    case kPDVirgl_CreateResource:  return mCreateResource(args);
    case kPDVirgl_DestroyResource: return mDestroyResource(args);
    case kPDVirgl_AttachResource:  return mAttachResource(args);
    case kPDVirgl_TransferToHost:  return mTransfer(args, true);
    case kPDVirgl_TransferFromHost:return mTransfer(args, false);
    case kPDVirgl_SubmitCmd:       return mSubmitCmd(args);
    case kPDVirgl_WaitFence:       return kIOReturnSuccess; // v1: submits are synchronous
    case kPDVirgl_AllocFenceId:    args->scalarOutput[0] = fOwner->allocFenceId();
                                   return kIOReturnSuccess;
    default:                       return kIOReturnUnsupported;
    }
}

// Hand the resource's backing buffer to userland so IOConnectMapMemory(resId)
// maps the pages Mesa stages vertex/texture data into.
IOReturn
IOVirtIOGPUUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                           IOMemoryDescriptor **memory)
{
    ResEntry *e = findRes((uint32_t)type);
    if (!e || !e->backing)
        return kIOReturnNotFound;
    e->backing->retain();
    *memory = e->backing;
    *options = 0;
    return kIOReturnSuccess;
}
