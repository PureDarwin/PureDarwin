/*
 * IOVirtIOGPUDisplay.cpp - a virtio-gpu scanout past the first, as an
 * IOFramebuffer of its own (VIRTIO 1.1, section 5.7).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IOVirtIOGPUDisplay.h"
#include "IOVirtIOGPU.h"

#include <IOKit/IOLib.h>

#define super IOFramebuffer

#define kDepth 0
// a second display only carries the cpu path, half the scanout's cadence is enough
#define kDisplayFlushMs 33

OSDefineMetaClassAndStructors(IOVirtIOGPUDisplay, IOFramebuffer);

IOVirtIOGPUDisplay *
IOVirtIOGPUDisplay::withScanout(IOVirtIOGPU *gpu, uint32_t scanout, uint32_t width, uint32_t height)
{
    IOVirtIOGPUDisplay *display = OSTypeAlloc(IOVirtIOGPUDisplay);

    if (display == NULL || !display->init()) {
        OSSafeReleaseNULL(display);
        return NULL;
    }
    display->fGPU = gpu;
    display->fScanout = scanout;
    display->fWidth = width;
    display->fHeight = height;
    display->setProperty("IOVirtIOGPUScanout", scanout, 32);
    return display;
}

// a cleared buffer and resource at the new size take over the scanout, the old ones go after
bool
IOVirtIOGPUDisplay::makeBuffer(uint32_t width, uint32_t height)
{
    uint32_t pitch = width * 4;
    size_t bytes = (size_t)pitch * height;
    IOBufferMemoryDescriptor *mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, bytes, 0xFFFFFFFFULL);

    if (mem == NULL)
        return false;
    bzero(mem->getBytesNoCopy(), bytes);

    uint32_t resourceId = fGPU->allocResourceId();

    if (!fGPU->gpuCreateResource2D(resourceId, width, height) ||
        !fGPU->gpuAttachBacking(resourceId, mem->getPhysicalAddress(), (uint32_t)bytes) ||
        !fGPU->gpuSetScanout(fScanout, resourceId, width, height)) {
        fGPU->gpuResourceUnref(resourceId);
        mem->release();
        return false;
    }

    IOLockLock(fLock);
    IOBufferMemoryDescriptor *oldMem = fFbMem;
    uint32_t oldResource = fResourceId;

    fFbMem = mem;
    fFbPhys = mem->getPhysicalAddress();
    fWidth = width;
    fHeight = height;
    fPitch = pitch;
    fResourceId = resourceId;
    fDark = false;
    IOLockUnlock(fLock);

    fGPU->gpuTransferToHost2D(fResourceId, 0, 0, fWidth, fHeight);
    fGPU->gpuResourceFlush(fResourceId, 0, 0, fWidth, fHeight);
    // a client that mapped the old buffer keeps its pages through the mapping
    if (oldResource != 0)
        fGPU->gpuResourceUnref(oldResource);
    if (oldMem != NULL)
        oldMem->release();
    return true;
}

bool
IOVirtIOGPUDisplay::start(IOService *provider)
{
    // the same dependent id as scanout 0 puts every head on one IOFBController
    if (!getProperty(kIOFBDependentIDKey)) {
        OSNumber *number = OSNumber::withNumber(provider->getRegistryEntryID(), 64);

        if (number != NULL) {
            setProperty(kIOFBDependentIDKey, number);
            number->release();
        }
    }
    fLock = IOLockAlloc();
    if (fLock == NULL || !makeBuffer(fWidth, fHeight))
        return false;
    if (!super::start(provider))
        return false;
    fFlushCall = thread_call_allocate(flushCallback, this);
    if (fFlushCall != NULL)
        flush();
    IOLog("IOVirtIOGPU: display %u is %ux%u\n", fScanout, fWidth, fHeight);
    registerService();
    return true;
}

void
IOVirtIOGPUDisplay::free()
{
    if (fFlushCall != NULL) {
        thread_call_cancel_wait(fFlushCall);
        thread_call_free(fFlushCall);
    }
    OSSafeReleaseNULL(fFbMem);
    if (fLock != NULL)
        IOLockFree(fLock);
    super::free();
}

void
IOVirtIOGPUDisplay::gpuGone()
{
    fGone = true;
    if (fFlushCall != NULL)
        thread_call_cancel_wait(fFlushCall);
}

// the whole buffer goes to the host each tick, a cpu-composited desktop has no damage to go by here
void
IOVirtIOGPUDisplay::flush()
{
    IOLockLock(fLock);
    uint32_t resourceId = fResourceId, width = fWidth, height = fHeight;
    bool dark = fDark;
    IOLockUnlock(fLock);

    if (fGone)
        return;
    if (!dark && resourceId != 0) {
        fGPU->gpuTransferToHost2D(resourceId, 0, 0, width, height);
        fGPU->gpuResourceFlush(resourceId, 0, 0, width, height);
    }

    AbsoluteTime deadline;
    clock_interval_to_deadline(kDisplayFlushMs, kMillisecondScale, &deadline);
    thread_call_enter_delayed(fFlushCall, deadline);
}

void
IOVirtIOGPUDisplay::flushCallback(thread_call_param_t self, thread_call_param_t)
{
    ((IOVirtIOGPUDisplay *)self)->flush();
}

void
IOVirtIOGPUDisplay::hostChanged(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0) {
        if (fDark)
            return;
        IOLockLock(fLock);
        fDark = true;
        IOLockUnlock(fLock);
        fGPU->gpuSetScanout(fScanout, 0, 0, 0);
        IOLog("IOVirtIOGPU: display %u turned off\n", fScanout);
    } else if (width != fWidth || height != fHeight || fDark) {
        if (width == fWidth && height == fHeight) {
            IOLockLock(fLock);
            fDark = false;
            IOLockUnlock(fLock);
            fGPU->gpuSetScanout(fScanout, fResourceId, fWidth, fHeight);
        } else if (!makeBuffer(width, height)) {
            IOLog("IOVirtIOGPU: display %u could not take %ux%u\n", fScanout, width, height);
            return;
        }
        IOLog("IOVirtIOGPU: display %u is now %ux%u\n", fScanout, fWidth, fHeight);
    } else {
        return;
    }
    // the compositor re-reads the mode and remaps on a connect change
    if (fConnectProc != NULL)
        fConnectProc(fConnectTarget, fConnectRef);
}

IOReturn
IOVirtIOGPUDisplay::registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
                                             OSObject *target, void *ref, void **interruptRef)
{
    if (interruptType != kIOFBConnectInterruptType)
        return kIOReturnUnsupported;
    fConnectProc = proc;
    fConnectTarget = target;
    fConnectRef = ref;
    *interruptRef = (void *)&fConnectProc;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUDisplay::unregisterInterrupt(void *interruptRef)
{
    if (interruptRef != (void *)&fConnectProc)
        return kIOReturnBadArgument;
    fConnectProc = NULL;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUDisplay::enableController()
{
    return kIOReturnSuccess;
}

const char *
IOVirtIOGPUDisplay::getPixelFormats()
{
    return IO32BitDirectPixels;
}

IOReturn
IOVirtIOGPUDisplay::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
    *displayMode = 1;
    *depth = kDepth;
    return kIOReturnSuccess;
}

// the one mode is whatever size the host gave the display
IOReturn
IOVirtIOGPUDisplay::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    return displayMode == 1 && depth == kDepth ? kIOReturnSuccess : kIOReturnUnsupportedMode;
}

IODeviceMemory *
IOVirtIOGPUDisplay::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange((mach_vm_address_t)fFbPhys, (IOByteCount)fPitch * fHeight);
}

IODeviceMemory *
IOVirtIOGPUDisplay::getVRAMRange(void)
{
    return getApertureRange(kIOFBSystemAperture);
}

IOReturn
IOVirtIOGPUDisplay::getInformationForDisplayMode(IODisplayModeID displayMode,
                                                 IODisplayModeInformation *info)
{
    if (displayMode != 1)
        return kIOReturnUnsupportedMode;
    bzero(info, sizeof(*info));
    info->nominalWidth = fWidth;
    info->nominalHeight = fHeight;
    info->refreshRate = 30 << 16;
    info->maxDepthIndex = kDepth;
    return kIOReturnSuccess;
}

UInt64
IOVirtIOGPUDisplay::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
    return 0;
}

IOReturn
IOVirtIOGPUDisplay::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                        IOPixelAperture aperture, IOPixelInformation *info)
{
    if (aperture != kIOFBSystemAperture || depth != kDepth || displayMode != 1)
        return kIOReturnUnsupportedMode;
    bzero(info, sizeof(*info));
    info->activeWidth = fWidth;
    info->activeHeight = fHeight;
    info->bytesPerRow = fPitch;
    strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
    info->pixelType = kIORGBDirectPixels;
    info->componentMasks[0] = 0x00ff0000;
    info->componentMasks[1] = 0x0000ff00;
    info->componentMasks[2] = 0x000000ff;
    info->bitsPerPixel = 32;
    info->componentCount = 3;
    info->bitsPerComponent = 8;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUDisplay::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    allDisplayModes[0] = 1;
    return kIOReturnSuccess;
}

IOItemCount
IOVirtIOGPUDisplay::getDisplayModeCount(void)
{
    return 1;
}
