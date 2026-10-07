#include "IOVirtIOGPUVenusClient.h"
#include "IOVirtIOGPU.h"
#include "IOVirtIOGPU3DShared.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(IOVirtIOGPUVenusClient, IOUserClient);

// VIRTGPU_BLOB_MEM_* / VIRTGPU_BLOB_FLAG_* (same values on the wire)
enum { kBlobMemHost3D = 2 };
enum { kBlobFlagMappable = 1 };

IOVirtIOGPUVenusClient *
IOVirtIOGPUVenusClient::withOwner(IOVirtIOGPU *owner, task_t task)
{
    IOVirtIOGPUVenusClient *vc = new IOVirtIOGPUVenusClient;
    if (!vc)
        return NULL;
    if (!vc->init()) {
        vc->release();
        return NULL;
    }
    vc->fOwner = owner;
    vc->fTask = task;
    vc->fLock = IOLockAlloc();
    vc->fRes = (Res *)IOMalloc(sizeof(Res) * kMaxResources);
    if (!vc->fLock || !vc->fRes) {
        vc->release();
        return NULL;
    }
    bzero(vc->fRes, sizeof(Res) * kMaxResources);
    return vc;
}

bool
IOVirtIOGPUVenusClient::start(IOService *provider)
{
    if (!super::start(provider))
        return false;
    fOwner = OSDynamicCast(IOVirtIOGPU, provider);
    return fOwner != NULL;
}

void
IOVirtIOGPUVenusClient::free()
{
    if (fRes) {
        IOFree(fRes, sizeof(Res) * kMaxResources);
        fRes = NULL;
    }
    if (fLock) {
        IOLockFree(fLock);
        fLock = NULL;
    }
    super::free();
}

IOVirtIOGPUVenusClient::Res *
IOVirtIOGPUVenusClient::findRes(uint32_t resId)
{
    for (uint32_t i = 0; i < fResCount; i++)
        if (fRes[i].resId == resId)
            return &fRes[i];
    return NULL;
}

// drop it from this context, the device unmaps and unrefs it with the last holder
void
IOVirtIOGPUVenusClient::releaseRes(Res *r)
{
    if (fCtxId)
        fOwner->gpu3DCtxDetachResource(fCtxId, r->resId);
    fOwner->blobRelease(r->resId);
}

void
IOVirtIOGPUVenusClient::destroyAll()
{
    if (!fOwner || !fLock)
        return;

    fOwner->blobDropExports(this);
    IOLockLock(fLock);
    for (uint32_t i = 0; i < fResCount; i++)
        releaseRes(&fRes[i]);
    fResCount = 0;
    if (fCtxId) {
        fOwner->gpu3DDestroyContext(fCtxId);
        fCtxId = 0;
    }
    IOLockUnlock(fLock);
}

IOReturn
IOVirtIOGPUVenusClient::clientClose(void)
{
    destroyAll();
    if (!isInactive())
        terminate();
    return kIOReturnSuccess;
}

void
IOVirtIOGPUVenusClient::stop(IOService *provider)
{
    destroyAll();
    super::stop(provider);
}

IOReturn
IOVirtIOGPUVenusClient::mGetInfo(IOExternalMethodArguments *a)
{
    if (!a->structureOutput || a->structureOutputSize < sizeof(PDVenusInfo))
        return kIOReturnBadArgument;

    PDVenusInfo *info = (PDVenusInfo *)a->structureOutput;
    bzero(info, sizeof(*info));
    info->features = fOwner->features();
    info->capset_mask = fOwner->capsetMask();
    info->hostmem_size = fOwner->hostmemSize();
    a->structureOutputSize = sizeof(*info);
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mGetCaps(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 2 || !a->structureOutput || !a->structureOutputSize)
        return kIOReturnBadArgument;

    uint32_t size = (uint32_t)a->structureOutputSize;
    if (!fOwner->getCapset((uint32_t)a->scalarInput[0], (uint32_t)a->scalarInput[1],
                           a->structureOutput, &size))
        return kIOReturnNotFound;
    a->structureOutputSize = size;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mContextInit(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->scalarOutputCount < 1)
        return kIOReturnBadArgument;

    IOLockLock(fLock);
    if (fCtxId) {
        IOLockUnlock(fLock);
        return kIOReturnBusy;
    }
    uint32_t ctxId = fOwner->allocContextId();
    bool ok = fOwner->gpu3DCreateContextInit(ctxId, "pd-venus", (uint32_t)a->scalarInput[0] & 0xff);
    if (ok) {
        fCtxId = ctxId;
        // blobs imported before there was a context
        for (uint32_t i = 0; i < fResCount; i++)
            fOwner->gpu3DCtxAttachResource(ctxId, fRes[i].resId);
    }
    IOLockUnlock(fLock);

    if (!ok)
        return kIOReturnIOError;
    a->scalarOutput[0] = ctxId;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mCreateBlob(IOExternalMethodArguments *a)
{
    if (a->structureInputSize < sizeof(PDVenusBlobCreate) || a->scalarOutputCount < 2)
        return kIOReturnBadArgument;

    const PDVenusBlobCreate *bc = (const PDVenusBlobCreate *)a->structureInput;
    // guest backed blobs need backing pages, venus only asks for host3d
    if (bc->blob_mem != kBlobMemHost3D || !bc->size)
        return kIOReturnUnsupported;

    IOLockLock(fLock);
    IOReturn ret = kIOReturnSuccess;
    uint32_t resId = 0;
    if (!fCtxId)
        ret = kIOReturnNotReady;
    else if (fResCount >= kMaxResources)
        ret = kIOReturnNoResources;
    else {
        resId = fOwner->allocResourceId();
        if (!fOwner->gpuCreateBlob(fCtxId, resId, bc->blob_mem, bc->blob_flags, bc->blob_id, bc->size))
            ret = kIOReturnIOError;
        else if (!fOwner->gpu3DCtxAttachResource(fCtxId, resId)) {
            fOwner->gpuResourceUnref(resId);
            ret = kIOReturnIOError;
        } else if (!fOwner->blobAdd(resId, bc->blob_mem, bc->blob_flags, bc->size)) {
            fOwner->gpu3DCtxDetachResource(fCtxId, resId);
            fOwner->gpuResourceUnref(resId);
            ret = kIOReturnNoResources;
        } else {
            Res *r = &fRes[fResCount++];
            bzero(r, sizeof(*r));
            r->resId = resId;
            r->blobMem = bc->blob_mem;
            r->blobFlags = bc->blob_flags;
            r->size = bc->size;
        }
    }
    IOLockUnlock(fLock);

    if (ret != kIOReturnSuccess)
        return ret;
    a->scalarOutput[0] = resId;
    a->scalarOutput[1] = bc->size;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mResourceInfo(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->scalarOutputCount < 3)
        return kIOReturnBadArgument;

    IOLockLock(fLock);
    Res *r = findRes((uint32_t)a->scalarInput[0]);
    if (r) {
        a->scalarOutput[0] = r->size;
        a->scalarOutput[1] = r->blobMem;
        a->scalarOutput[2] = r->blobFlags;
    }
    IOLockUnlock(fLock);
    return r ? kIOReturnSuccess : kIOReturnNotFound;
}

IOReturn
IOVirtIOGPUVenusClient::mUnref(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1)
        return kIOReturnBadArgument;

    IOLockLock(fLock);
    Res *r = findRes((uint32_t)a->scalarInput[0]);
    if (r) {
        releaseRes(r);
        *r = fRes[--fResCount];
    }
    IOLockUnlock(fLock);
    return r ? kIOReturnSuccess : kIOReturnNotFound;
}

// the device maps the blob into the host visible window once for every holder, then
// IOConnectMapMemory64(res id) maps it into the task
IOReturn
IOVirtIOGPUVenusClient::mMap(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->scalarOutputCount < 2)
        return kIOReturnBadArgument;

    IOLockLock(fLock);
    IOReturn ret = kIOReturnSuccess;
    uint32_t info = 0;
    Res *r = findRes((uint32_t)a->scalarInput[0]);
    if (!r)
        ret = kIOReturnNotFound;
    else if (!(r->blobFlags & kBlobFlagMappable))
        ret = kIOReturnNotWritable;
    else if (!fOwner->blobMap(r->resId, &info))
        ret = kIOReturnIOError;
    if (ret == kIOReturnSuccess) {
        r->mapInfo = info;
        a->scalarOutput[0] = info;
        a->scalarOutput[1] = r->size;
    }
    IOLockUnlock(fLock);
    return ret;
}

// a token any connection can import the blob with, valid while this connection is open
IOReturn
IOVirtIOGPUVenusClient::mExport(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->scalarOutputCount < 1)
        return kIOReturnBadArgument;

    uint64_t token = 0;
    IOLockLock(fLock);
    Res *r = findRes((uint32_t)a->scalarInput[0]);
    bool ok = r && fOwner->blobExport(r->resId, this, &token);
    IOLockUnlock(fLock);
    if (!r)
        return kIOReturnNotFound;
    if (!ok)
        return kIOReturnNoResources;
    a->scalarOutput[0] = token;
    return kIOReturnSuccess;
}

// importing a blob the connection already holds gives back the same handle, as prime does
IOReturn
IOVirtIOGPUVenusClient::mImport(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->scalarOutputCount < 4)
        return kIOReturnBadArgument;

    uint32_t resId = 0, blobMem = 0, blobFlags = 0;
    uint64_t size = 0;
    if (!fOwner->blobImport(a->scalarInput[0], &resId, &blobMem, &blobFlags, &size))
        return kIOReturnNotFound;

    IOLockLock(fLock);
    IOReturn ret = kIOReturnSuccess;
    bool drop = false;
    if (findRes(resId))
        drop = true;
    else if (fResCount >= kMaxResources) {
        drop = true;
        ret = kIOReturnNoResources;
    } else if (fCtxId && !fOwner->gpu3DCtxAttachResource(fCtxId, resId)) {
        drop = true;
        ret = kIOReturnIOError;
    } else {
        Res *r = &fRes[fResCount++];
        bzero(r, sizeof(*r));
        r->resId = resId;
        r->blobMem = blobMem;
        r->blobFlags = blobFlags;
        r->size = size;
    }
    IOLockUnlock(fLock);
    if (drop)
        fOwner->blobRelease(resId);
    if (ret != kIOReturnSuccess)
        return ret;

    a->scalarOutput[0] = resId;
    a->scalarOutput[1] = size;
    a->scalarOutput[2] = blobMem;
    a->scalarOutput[3] = blobFlags;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mSubmit(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 2 || a->scalarOutputCount < 1)
        return kIOReturnBadArgument;

    uint32_t ring = (uint32_t)a->scalarInput[0];
    uint32_t flags = (uint32_t)a->scalarInput[1];
    uint32_t ctxId = fCtxId;
    if (!ctxId)
        return kIOReturnNotReady;

    // inline streams arrive in structureInput, larger ones as an ool descriptor
    IOMemoryDescriptor *ool = a->structureInputDescriptor;
    IOByteCount len = ool ? ool->getLength() : a->structureInputSize;
    if (len > kMaxSubmitBytes)
        return kIOReturnBadArgument;

    IOBufferMemoryDescriptor *bounce = NULL;
    if (len) {
        bounce = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, len, 0xFFFFFFFFULL);
        if (!bounce)
            return kIOReturnNoMemory;
        if (ool) {
            if (ool->prepare(kIODirectionOut) != kIOReturnSuccess) {
                bounce->release();
                return kIOReturnVMError;
            }
            ool->readBytes(0, bounce->getBytesNoCopy(), len);
            ool->complete();
        } else
            memcpy(bounce->getBytesNoCopy(), a->structureInput, len);
    }

    uint64_t fenceId = (flags & PD_VENUS_SUBMIT_FENCE) ? fOwner->allocFenceId() : 0;
    bool ok = fOwner->gpuSubmitRing(ctxId, (flags & PD_VENUS_SUBMIT_RING) != 0, ring, bounce,
                                    (uint32_t)len, fenceId);
    if (bounce)
        bounce->release();
    if (!ok)
        return kIOReturnIOError;
    a->scalarOutput[0] = fenceId;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mFenceStatus(IOExternalMethodArguments *a)
{
    uint32_t n = (uint32_t)(a->structureInputSize / sizeof(uint64_t));
    if (!n || n > kMaxFenceQuery || !a->structureOutput || a->structureOutputSize < n)
        return kIOReturnBadArgument;

    const uint64_t *ids = (const uint64_t *)a->structureInput;
    uint8_t *out = (uint8_t *)a->structureOutput;
    for (uint32_t i = 0; i < n; i++)
        out[i] = fOwner->fenceRetired(ids[i]) ? 1 : 0;
    a->structureOutputSize = n;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::mWaitFence(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 2)
        return kIOReturnBadArgument;
    return fOwner->waitFence(a->scalarInput[0], a->scalarInput[1]) ? kIOReturnSuccess
                                                                     : kIOReturnTimeout;
}

IOReturn
IOVirtIOGPUVenusClient::mWaitProgress(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 2 || a->scalarOutputCount < 1)
        return kIOReturnBadArgument;
    a->scalarOutput[0] = fOwner->waitProgress(a->scalarInput[0], a->scalarInput[1]);
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUVenusClient::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                       IOExternalMethodDispatch *, OSObject *, void *)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    switch (selector) {
    case kPDVenus_GetInfo:      return mGetInfo(args);
    case kPDVenus_GetCaps:      return mGetCaps(args);
    case kPDVenus_ContextInit:  return mContextInit(args);
    case kPDVenus_CreateBlob:   return mCreateBlob(args);
    case kPDVenus_ResourceInfo: return mResourceInfo(args);
    case kPDVenus_Unref:        return mUnref(args);
    case kPDVenus_Map:          return mMap(args);
    case kPDVenus_Submit:       return mSubmit(args);
    case kPDVenus_FenceStatus:  return mFenceStatus(args);
    case kPDVenus_WaitFence:    return mWaitFence(args);
    case kPDVenus_WaitProgress: return mWaitProgress(args);
    case kPDVenus_Export:       return mExport(args);
    case kPDVenus_Import:       return mImport(args);
    default:                    return kIOReturnUnsupported;
    }
}

IOReturn
IOVirtIOGPUVenusClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                            IOMemoryDescriptor **memory)
{
    IOLockLock(fLock);
    bool held = findRes((uint32_t)type) != NULL;
    IOLockUnlock(fLock);

    uint32_t cache = 0;
    IOMemoryDescriptor *window = held ? fOwner->blobWindow((uint32_t)type, &cache) : NULL;
    if (!window)
        return kIOReturnNotFound;

    *memory = window;
    switch (cache & PD_VENUS_MAP_CACHE_MASK) {
    case PD_VENUS_MAP_CACHE_CACHED: *options = kIOMapCopybackCache; break;
    case PD_VENUS_MAP_CACHE_WC:     *options = kIOMapWriteCombineCache; break;
    default:                        *options = kIOMapInhibitCache; break;
    }
    return kIOReturnSuccess;
}
