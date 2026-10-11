#include "PDRadeonRenderClient.h"
#include "PDRadeonFramebuffer.h"
#include "PDRadeonRenderShared.h"
#include <IOKit/IODeviceMemory.h>

#define super IOUserClient
OSDefineMetaClassAndStructors(PDRadeonRenderClient, IOUserClient);

IOReturn PDRadeonFramebuffer::newUserClient(task_t task, void *token, UInt32 type, IOUserClient **out)
{
    if (type != PD_RADEON_RENDER_CLIENT)
        return IOFramebuffer::newUserClient(task, token, type, out);
    if (!out || !renderCard())
        return kIOReturnUnsupported;
    *out = NULL;
    PDRadeonRenderClient *client = OSTypeAlloc(PDRadeonRenderClient);
    if (!client)
        return kIOReturnNoMemory;
    if (!client->initWithTask(task, token, type) || !client->attach(this)) {
        client->release();
        return kIOReturnNotPermitted;
    }
    if (!client->start(this)) {
        client->detach(this);
        client->release();
        return kIOReturnError;
    }
    *out = client;
    return kIOReturnSuccess;
}

bool PDRadeonRenderClient::initWithTask(task_t task, void *token, UInt32 type)
{
    // this interface submits native command packets and is restricted to administrators.
    if (clientHasPrivilege((void *)task, kIOClientPrivilegeAdministrator) != kIOReturnSuccess ||
        !super::initWithTask(task, token, type))
        return false;
    fTask = task;
    fLock = IOLockAlloc();
    return fLock != NULL;
}

bool PDRadeonRenderClient::start(IOService *provider)
{
    PDRadeonFramebuffer *fb = OSDynamicCast(PDRadeonFramebuffer, provider);
    fCard = fb ? fb->renderCard() : NULL;
    return fCard && super::start(provider);
}

void PDRadeonRenderClient::restoreScanout(void)
{
    if (!fPresentedHandle) return;
    PDRadeonFramebuffer *fb=OSDynamicCast(PDRadeonFramebuffer,getProvider());
    uint32_t width,height,stride,offset;
    if (fb && fb->scanoutInfo(&width,&height,&stride,&offset))
        fb->presentBuffer(0,width,height,stride);
    fPresentedHandle=0;
}

void PDRadeonRenderClient::releaseBuffers(void)
{
    if (!fCard || !fLock)
        return;
    IOLockLock(fLock);
    restoreScanout();
    for (unsigned i = 1; i < 512; i++) {
        if (fBuffers[i].memory)
            fBuffers[i].memory->release();
        if (fBuffers[i].size && fBuffers[i].owned)
            fCard->renderFree(fBuffers[i].offset, fBuffers[i].size);
        fBuffers[i] = {};
    }
    IOLockUnlock(fLock);
}

IOReturn PDRadeonRenderClient::clientClose(void)
{
    releaseBuffers();
    terminate();
    return kIOReturnSuccess;
}

void PDRadeonRenderClient::free(void)
{
    releaseBuffers();
    if (fLock)
        IOLockFree(fLock);
    super::free();
}

IOReturn PDRadeonRenderClient::externalMethod(uint32_t selector, IOExternalMethodArguments *a,
    IOExternalMethodDispatch *, OSObject *, void *)
{
    if (!fCard || !fCard->accelReady)
        return kIOReturnNotReady;
    IOReturn result = kIOReturnBadArgument;
    IOLockLock(fLock);
    switch (selector) {
    case PD_RADEON_INFO:
        if (a->structureOutput && a->structureOutputSize >= sizeof(pd_radeon_info)) {
            pd_radeon_info info = {};
            info.version = 1;
            info.pci_id = fCard->pci->configRead16(2);
            info.heap_size = (uint64_t)fCard->renderPageCount * 4096;
            info.tiling_config = fCard->gpuTiling;
            info.backend_map = fCard->gpuBackendMap;
            info.active_simds = fCard->gpuActiveSimds;
            info.shader_config = fCard->rreg(0x8950);
            info.ram_config = fCard->rreg(0x2760);
            info.max_clock_mhz = fCard->engineClockMhz;
            info.clock_crystal_khz = fCard->pll.refFreq * 10;
            memcpy(a->structureOutput, &info, sizeof(info));
            a->structureOutputSize = sizeof(info);
            result = kIOReturnSuccess;
        }
        break;
    case PD_RADEON_FLIP:
        if (a->scalarInputCount==4 && a->scalarInput[0]<512) {
            unsigned handle=(unsigned)a->scalarInput[0];
            uint64_t width=a->scalarInput[1],height=a->scalarInput[2],stride=a->scalarInput[3];
            PDRadeonFramebuffer *fb=OSDynamicCast(PDRadeonFramebuffer,getProvider());
            uint32_t scanWidth,scanHeight,scanStride,scanOffset,bitsPerPixel;
            if (!fb || !fb->scanoutInfo(&scanWidth,&scanHeight,&scanStride,&scanOffset,&bitsPerPixel) ||
                !width || !height || width>4096 || height>4096 || stride>16384 ||
                stride<width*(bitsPerPixel/8))
                break;
            if (handle && (!fBuffers[handle].owned || stride*height>fBuffers[handle].size)) break;
            uint32_t offset=handle ? fBuffers[handle].offset : 0;
            if (fb && fb->presentBuffer(offset,(uint32_t)width,(uint32_t)height,(uint32_t)stride)) {
                fPresentedHandle=handle; result=kIOReturnSuccess;
            } else result=kIOReturnTimeout;
        }
        break;
    case PD_RADEON_SCANOUT:
        if (a->structureOutput && a->structureOutputSize >= sizeof(pd_radeon_scanout)) {
            pd_radeon_scanout info = {};
            uint32_t offset;
            PDRadeonFramebuffer *fb = OSDynamicCast(PDRadeonFramebuffer, getProvider());
            IOLockLock(fCard->lock);
            bool valid = fb && fb->scanoutInfo(&info.width, &info.height, &info.stride,
                                              &offset, &info.reserved);
            IOLockUnlock(fCard->lock);
            if (!valid) { result=kIOReturnNotReady; break; }
            unsigned handle;
            for (handle=1; handle<512 && fBuffers[handle].size; handle++) {}
            if (handle==512) { result=kIOReturnNoResources; break; }
            info.handle=handle; info.offset=offset; info.size=(uint64_t)info.stride*info.height;
            fBuffers[handle] = { offset, (uint32_t)info.size, NULL, false };
            memcpy(a->structureOutput,&info,sizeof(info));
            a->structureOutputSize=sizeof(info); result=kIOReturnSuccess;
        }
        break;
    case PD_RADEON_ALLOC:
        if (a->scalarInputCount == 2 && a->scalarOutputCount >= 3) {
            uint64_t size = a->scalarInput[0], alignment = a->scalarInput[1];
            if (!size || size > (uint64_t)fCard->renderPageCount * 4096 ||
                alignment > 0x800000 || (alignment && (alignment & (alignment - 1))))
                break;
            uint32_t rounded = ((uint32_t)size + 4095) & ~4095u;
            unsigned handle;
            for (handle = 1; handle < 512 && fBuffers[handle].size; handle++) {}
            if (handle == 512) { result = kIOReturnNoResources; break; }
            uint32_t offset = fCard->renderAlloc(rounded, (uint32_t)alignment);
            if (!offset) { result = kIOReturnNoMemory; break; }
            fBuffers[handle] = { offset, rounded, NULL, true };
            a->scalarOutput[0] = handle;
            a->scalarOutput[1] = offset;
            a->scalarOutput[2] = rounded;
            a->scalarOutputCount = 3;
            result = kIOReturnSuccess;
        }
        break;
    case PD_RADEON_FREE:
        if (a->scalarInputCount == 1 && a->scalarInput[0] > 0 && a->scalarInput[0] < 512) {
            unsigned handle = (unsigned)a->scalarInput[0];
            if (!fBuffers[handle].size)
                break;
            if (fPresentedHandle==handle) restoreScanout();
            if (fBuffers[handle].memory)
                fBuffers[handle].memory->release();
            if (fBuffers[handle].owned)
                fCard->renderFree(fBuffers[handle].offset, fBuffers[handle].size);
            fBuffers[handle] = {};
            result = kIOReturnSuccess;
        }
        break;
    case PD_RADEON_COPY_OUT:
        // gpu copy of a linear vram region into the caller's memory, no cpu reads over the bar
        if (a->scalarInputCount == 7 && a->scalarInput[0] > 0 && a->scalarInput[0] < 512) {
            Buffer &b = fBuffers[a->scalarInput[0]];
            uint64_t off = a->scalarInput[1], spitch = a->scalarInput[2], row = a->scalarInput[3];
            uint64_t rows = a->scalarInput[4], dpitch = a->scalarInput[5], addr = a->scalarInput[6];
            if (!b.size || !row || !rows || row > spitch || row > dpitch || rows > 8192 ||
                off + (rows - 1) * spitch + row > b.size)
                break;
            uint64_t length = (rows - 1) * dpitch + row;
            IOMemoryDescriptor *md = IOMemoryDescriptor::withAddressRange(addr, length, kIODirectionIn, fTask);
            if (!md) { result = kIOReturnNoMemory; break; }
            result = fCard->copyOut(md, b.offset + (uint32_t)off, (uint32_t)spitch, (uint32_t)row,
                                    (uint32_t)rows, (uint32_t)dpitch) ? kIOReturnSuccess : kIOReturnError;
            md->release();
        }
        break;
    case PD_RADEON_SUBMIT: {
        size_t bytes = a->structureInputDescriptor ? a->structureInputDescriptor->getLength() : a->structureInputSize;
        if (!bytes || (bytes & 3) || bytes > PD_RADEON_MAX_SUBMIT_DW * 4)
            break;
        uint32_t *commands = (uint32_t *)IOMalloc(bytes);
        if (!commands) { result = kIOReturnNoMemory; break; }
        bool copied = false;
        if (a->structureInputDescriptor) {
            // IOKit supplies large command streams as pageable user memory.
            // readBytes walks physical segments, so wire the descriptor first.
            result = a->structureInputDescriptor->prepare();
            if (result == kIOReturnSuccess) {
                copied = a->structureInputDescriptor->readBytes(0, commands, bytes) == bytes;
                a->structureInputDescriptor->complete();
                if (!copied) result = kIOReturnUnderrun;
            }
        } else if (a->structureInput) {
            memcpy(commands, a->structureInput, bytes);
            copied = true;
        }
        if (copied)
            result = fCard->accelSubmit(commands, (uint32_t)(bytes / 4)) ? kIOReturnSuccess : kIOReturnTimeout;
        IOFree(commands, bytes);
        break;
    }
    }
    IOLockUnlock(fLock);
    return result;
}

IOReturn PDRadeonRenderClient::clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory)
{
    if (!fCard || !type || type >= 512)
        return kIOReturnBadArgument;
    IOLockLock(fLock);
    Buffer &b = fBuffers[type];
    if (b.size && !b.memory)
        b.memory = IODeviceMemory::withRange(fCard->vramBase + b.offset, b.size);
    *memory = b.memory;
    if (*memory)
        (*memory)->retain();
    *options = (*options & ~kIOMapCacheMask) | kIOMapWriteCombineCache;
    IOLockUnlock(fLock);
    return *memory ? kIOReturnSuccess : kIOReturnBadArgument;
}
