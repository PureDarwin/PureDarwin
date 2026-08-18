#include "IOVirtIOGPUSurfaceClient.h"
#include "IOVirtIOGPU.h"

#include <IOKit/IOLib.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(IOVirtIOGPUSurfaceClient, IOUserClient);

IOVirtIOGPUSurfaceClient *
IOVirtIOGPUSurfaceClient::withOwner(IOVirtIOGPU *owner, task_t task)
{
    IOVirtIOGPUSurfaceClient *uc = OSTypeAlloc(IOVirtIOGPUSurfaceClient);
    if (!uc)
        return NULL;
    if (!uc->init()) {
        uc->release();
        return NULL;
    }
    uc->fOwner = owner;
    uc->fTask = task;
    uc->fCount = 0;
    bzero(uc->fSurfaces, sizeof(uc->fSurfaces));
    return uc;
}

bool
IOVirtIOGPUSurfaceClient::start(IOService *provider)
{
    return super::start(provider);
}

void
IOVirtIOGPUSurfaceClient::stop(IOService *provider)
{
    destroyAll();
    super::stop(provider);
}

IOReturn
IOVirtIOGPUSurfaceClient::clientClose(void)
{
    destroyAll();
    terminate();
    return kIOReturnSuccess;
}

IOVirtIOGPUSurfaceClient::SurfaceEntry *
IOVirtIOGPUSurfaceClient::find(uint64_t surfaceId)
{
    for (uint32_t i = 0; i < fCount; i++) {
        if (fSurfaces[i].resourceId == (uint32_t)surfaceId)
            return &fSurfaces[i];
    }
    return NULL;
}

void
IOVirtIOGPUSurfaceClient::destroyAll()
{
    if (!fOwner)
        return;

    // A surface still being scanned out would leave the display pointing at
    // freed pages, so hand the framebuffer back first.
    fOwner->gpuSetScanoutResource(0, 0, 0);

    for (uint32_t i = 0; i < fCount; i++) {
        if (fSurfaces[i].backing) {
            fOwner->gpuResourceUnref(fSurfaces[i].resourceId);
            fSurfaces[i].backing->release();
            fSurfaces[i].backing = NULL;
        }
    }
    fCount = 0;

    fOwner->releasePresentOwnership();
}

IOReturn
IOVirtIOGPUSurfaceClient::mGetVersion(IOExternalMethodArguments *a)
{
    if (a->scalarOutputCount < 1)
        return kIOReturnBadArgument;
    a->scalarOutput[0] = kPDSurfaceProtocolVersion;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUSurfaceClient::mCreate(IOExternalMethodArguments *a)
{
    if (a->structureInputSize < sizeof(PDSurfaceCreateRequest) ||
        a->scalarOutputCount < 3)
        return kIOReturnBadArgument;
    if (fCount >= kMaxSurfaces)
        return kIOReturnNoResources;

    const PDSurfaceCreateRequest *request =
        (const PDSurfaceCreateRequest *)a->structureInput;
    if (request->format != kPDSurfaceFormatARGB8888 &&
        request->format != kPDSurfaceFormatXRGB8888)
        return kIOReturnUnsupported;

    uint32_t resourceId = 0, stride = 0;
    IOBufferMemoryDescriptor *backing = NULL;
    if (!fOwner->gpuCreateSurfaceResource(request->width, request->height,
                                          &resourceId, &stride, &backing))
        return kIOReturnNoMemory;

    SurfaceEntry *entry = &fSurfaces[fCount++];
    entry->resourceId = resourceId;
    entry->width = request->width;
    entry->height = request->height;
    entry->format = request->format;
    entry->usage = request->usage;
    entry->stride = stride;
    entry->length = (uint64_t)stride * request->height;
    entry->backing = backing;

    a->scalarOutput[0] = resourceId;
    a->scalarOutput[1] = stride;
    a->scalarOutput[2] = entry->length;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUSurfaceClient::mDestroy(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1)
        return kIOReturnBadArgument;
    SurfaceEntry *entry = find(a->scalarInput[0]);
    if (!entry)
        return kIOReturnNotFound;

    fOwner->gpuResourceUnref(entry->resourceId);
    if (entry->backing)
        entry->backing->release();

    *entry = fSurfaces[--fCount];
    bzero(&fSurfaces[fCount], sizeof(fSurfaces[fCount]));
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUSurfaceClient::mLookup(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1 || a->structureOutput == NULL ||
        a->structureOutputSize < sizeof(PDSurfaceInfo))
        return kIOReturnBadArgument;
    SurfaceEntry *entry = find(a->scalarInput[0]);
    if (!entry)
        return kIOReturnNotFound;

    PDSurfaceInfo info;
    bzero(&info, sizeof(info));
    info.surface_id = entry->resourceId;
    info.width = entry->width;
    info.height = entry->height;
    info.format = entry->format;
    info.usage = entry->usage;
    info.stride = entry->stride;
    info.length = entry->length;

    bcopy(&info, a->structureOutput, sizeof(info));
    a->structureOutputSize = sizeof(info);
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUSurfaceClient::mSetScanout(IOExternalMethodArguments *a)
{
    if (a->scalarInputCount < 1)
        return kIOReturnBadArgument;

    if (a->scalarInput[0] == 0)
        return fOwner->gpuSetScanoutResource(0, 0, 0) ? kIOReturnSuccess
                                                      : kIOReturnIOError;

    SurfaceEntry *entry = find(a->scalarInput[0]);
    if (!entry)
        return kIOReturnNotFound;
    if (!(entry->usage & kPDSurfaceUsageScanout))
        return kIOReturnBadArgument;

    return fOwner->gpuSetScanoutResource(entry->resourceId, entry->width,
                                         entry->height)
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUSurfaceClient::mFlush(IOExternalMethodArguments *a)
{
    if (a->structureInputSize < sizeof(PDSurfaceFlushRequest))
        return kIOReturnBadArgument;
    const PDSurfaceFlushRequest *request =
        (const PDSurfaceFlushRequest *)a->structureInput;

    SurfaceEntry *entry = find(request->surface_id);
    if (!entry)
        return kIOReturnNotFound;

    uint32_t x = request->x, y = request->y;
    uint32_t width = request->width, height = request->height;
    if (x >= entry->width || y >= entry->height)
        return kIOReturnBadArgument;
    if (!width || width > entry->width - x)
        width = entry->width - x;
    if (!height || height > entry->height - y)
        height = entry->height - y;

    return fOwner->gpuFlushSurface(entry->resourceId, x, y, width, height)
               ? kIOReturnSuccess : kIOReturnIOError;
}

IOReturn
IOVirtIOGPUSurfaceClient::externalMethod(uint32_t selector,
                                         IOExternalMethodArguments *args,
                                         IOExternalMethodDispatch *, OSObject *,
                                         void *)
{
    if (!fOwner)
        return kIOReturnNotAttached;

    switch (selector) {
    case kPDSurface_GetVersion:  return mGetVersion(args);
    case kPDSurface_Create:      return mCreate(args);
    case kPDSurface_Destroy:     return mDestroy(args);
    case kPDSurface_Lookup:      return mLookup(args);
    case kPDSurface_SetScanout:  return mSetScanout(args);
    case kPDSurface_Flush:       return mFlush(args);
    default:                     return kIOReturnUnsupported;
    }
}

// libPDSurface maps a surface's storage using its id as the memory type.
IOReturn
IOVirtIOGPUSurfaceClient::clientMemoryForType(UInt32 type, IOOptionBits *options,
                                              IOMemoryDescriptor **memory)
{
    SurfaceEntry *entry = find(type);
    if (!entry || !entry->backing)
        return kIOReturnNotFound;
    entry->backing->retain();
    *memory = entry->backing;
    *options = 0;
    return kIOReturnSuccess;
}
