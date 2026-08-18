#include "IOVirtIOGPUUserClient.h"
#include "IOVirtIOGPU.h"
#include "IOVirtIOGPU3DShared.h"
#include <IOKit/IOLib.h>

#define super IOUserClient
#define kIOFBGetPixelInformationSelector 1
#define kIOFBGetCurrentDisplayModeSelector 2
#define kIOFBVRAMMemory 110
#define kPDGPU_Present 11
OSDefineMetaClassAndStructors(IOVirtIOGPUUserClient, IOUserClient);

IOVirtIOGPUUserClient *
IOVirtIOGPUUserClient::withOwner(IOVirtIOGPU *owner, task_t task,
                                 bool framebufferClient)
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
    uc->fFramebufferClient = framebufferClient;
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

    // Nothing is driving the framebuffer any more, so the driver has to go
    // back to pushing it itself or the display freezes on this client's last
    // frame.
    fOwner->releasePresentOwnership();
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
IOVirtIOGPUUserClient::mPresent(IOExternalMethodArguments *a)
{
    if (a->structureInputSize < sizeof(PDGpuPresent))
        return kIOReturnBadArgument;
    const PDGpuPresent *present = (const PDGpuPresent *)a->structureInput;
    return fOwner->gpuPresent(present->x, present->y, present->width,
                               present->height)
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::mSetCursor(IOExternalMethodArguments *a)
{
    // A 64x64 BGRA cursor is 16KB, past the 4K inband limit, so it arrives as
    // an ool descriptor with structureInput left NULL - same split as
    // mSubmitCmd above.
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

    IOReturn ret = applyCursorImage(src, (size_t)len);

    if (map) map->release();
    if (ool) ool->complete();
    return ret;
}

IOReturn
IOVirtIOGPUUserClient::applyCursorImage(const void *input, size_t inputSize)
{
    if (input == NULL || inputSize < sizeof(PDGpuCursorImage))
        return kIOReturnBadArgument;
    const PDGpuCursorImage *image = (const PDGpuCursorImage *)input;
    if (image->width > 64 || image->height > 64)
        return kIOReturnBadArgument;

    size_t pixelBytes = (size_t)image->width * image->height * 4;
    if (inputSize - sizeof(PDGpuCursorImage) < pixelBytes)
        return kIOReturnBadArgument;

    const void *pixels = pixelBytes ?
        (const uint8_t *)input + sizeof(PDGpuCursorImage) : NULL;
    return fOwner->gpuSetCursorImage(pixels, image->width, image->height,
                                     image->hot_x, image->hot_y)
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::mMoveCursor(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 2)
        return kIOReturnBadArgument;
    return fOwner->gpuMoveCursor((uint32_t)a->scalarInput[0],
                                 (uint32_t)a->scalarInput[1])
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::mSetScanoutResource(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 3)
        return kIOReturnBadArgument;
    return fOwner->gpuSetScanoutResource((uint32_t)a->scalarInput[0],
                                         (uint32_t)a->scalarInput[1],
                                         (uint32_t)a->scalarInput[2])
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                      IOExternalMethodDispatch *, OSObject *, void *)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    if (fFramebufferClient) {
        /* Minimal type-0 framebuffer ABI used by PDGOP. */
        switch (selector) {
        case kIOFBGetPixelInformationSelector:
            if (args->scalarInputCount < 3 || !args->structureOutput ||
                args->structureOutputSize < sizeof(IOPixelInformation))
                return kIOReturnBadArgument;
            return fOwner->getPixelInformation(
                (IODisplayModeID)args->scalarInput[0],
                (IOIndex)args->scalarInput[1],
                (IOPixelAperture)args->scalarInput[2],
                (IOPixelInformation *)args->structureOutput);
        case kIOFBGetCurrentDisplayModeSelector:
            if (args->scalarOutputCount < 2)
                return kIOReturnBadArgument;
            return fOwner->getCurrentDisplayMode(
                (IODisplayModeID *)&args->scalarOutput[0],
                (IOIndex *)&args->scalarOutput[1]);
        case kPDGPU_Present:
            if (!args->structureInput ||
                args->structureInputSize < sizeof(uint32_t) * 4)
                return kIOReturnBadArgument;
            {
                const uint32_t *rect = (const uint32_t *)args->structureInput;
                return fOwner->gpuPresent(rect[0], rect[1], rect[2], rect[3])
                    ? kIOReturnSuccess : kIOReturnIOError;
            }
        default:
            return kIOReturnUnsupported;
        }
    }

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
    case kPDGPU_Present:           return mPresent(args);
    case kPDGPU_SetCursor:         return mSetCursor(args);
    case kPDGPU_MoveCursor:        return mMoveCursor(args);
    case kPDGPU_SetScanoutResource: return mSetScanoutResource(args);
    default:                       return kIOReturnUnsupported;
    }
}

// Hand the resource's backing buffer to userland so IOConnectMapMemory(resId)
// maps the pages Mesa stages vertex/texture data into.
IOReturn
IOVirtIOGPUUserClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                           IOMemoryDescriptor **memory)
{
    if (fFramebufferClient && type == kIOFBVRAMMemory) {
        IOBufferMemoryDescriptor *fb = fOwner ? fOwner->copyFramebufferMemory() : NULL;
        if (!fb)
            return kIOReturnNotFound;
        *memory = fb;
        *options = 0;
        return kIOReturnSuccess;
    }

    ResEntry *e = findRes((uint32_t)type);
    if (!e || !e->backing)
        return kIOReturnNotFound;
    e->backing->retain();
    *memory = e->backing;
    *options = 0;
    return kIOReturnSuccess;
}
