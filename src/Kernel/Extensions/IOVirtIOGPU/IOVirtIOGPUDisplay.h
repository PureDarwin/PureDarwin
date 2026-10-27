/*
 * IOVirtIOGPUDisplay.h - a virtio-gpu scanout past the first, as an
 * IOFramebuffer of its own (VIRTIO 1.1, section 5.7).
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#pragma once

#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLocks.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <kern/thread_call.h>

class IOVirtIOGPU;

class IOVirtIOGPUDisplay : public IOFramebuffer
{
    OSDeclareDefaultStructors(IOVirtIOGPUDisplay);

    IOVirtIOGPU *fGPU;
    uint32_t     fScanout;
    // guest memory the host reads the pixels from, behind a 2D resource of the same size
    IOBufferMemoryDescriptor *fFbMem;
    uint64_t     fFbPhys;
    uint32_t     fWidth, fHeight, fPitch;
    uint32_t     fResourceId;
    // the host turned this display off, nothing is pushed until it comes back
    bool         fDark;
    IOLock      *fLock;
    thread_call_t fFlushCall;
    bool         fGone;

    IOFBInterruptProc fConnectProc;
    OSObject    *fConnectTarget;
    void        *fConnectRef;

    bool     makeBuffer(uint32_t width, uint32_t height);
    void     flush();
    static void flushCallback(thread_call_param_t self, thread_call_param_t);

public:
    static IOVirtIOGPUDisplay *withScanout(IOVirtIOGPU *gpu, uint32_t scanout, uint32_t width,
                                           uint32_t height);
    uint32_t scanout() const { return fScanout; }
    // the host resized this display, or turned it off with 0x0
    void     hostChanged(uint32_t width, uint32_t height);
    // the gpu driver is stopping, the timer stops sending it commands
    void     gpuGone();

    bool     start(IOService *provider) override;
    void     free() override;

    IOReturn registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
                                      OSObject *target, void *ref, void **interruptRef) override;
    IOReturn unregisterInterrupt(void *interruptRef) override;
    IOReturn enableController() override;
    const char *getPixelFormats() override;
    IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
    IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    IODeviceMemory *getVRAMRange(void) override;
    IOReturn getInformationForDisplayMode(IODisplayModeID displayMode,
                                          IODisplayModeInformation *info) override;
    UInt64   getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                 IOPixelAperture aperture, IOPixelInformation *info) override;
    IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
    IOItemCount getDisplayModeCount(void) override;
};
