/*
 * IOVirtIOGPUPresenter.h - a compositor's flip pair on one virtio-gpu scanout.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#pragma once

#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

class IOVirtIOGPU;

// 'pdpf', the presenter protocol PDDRMFramebuffer offers
#define kIOVirtIOGPUPresenterType 0x70647066

class IOVirtIOGPUPresenter : public IOUserClient
{
    OSDeclareDefaultStructors(IOVirtIOGPUPresenter);

    enum {
        kPresenterGetInfo = 0,  // out: width, height, pitch, buffer count
        kPresenterFlip = 1,     // in: buffer, out: flip sequence, time in ns
        kPresenterExport = 2,   // in: buffer, out: a dma-buf fd, not offered here
        kPresenterMethodCount
    };

    IOVirtIOGPU *fGPU;
    uint32_t     fScanout;
    uint32_t     fWidth, fHeight, fPitch;
    // guest pages behind each 2D resource, mapped into the compositor
    IOBufferMemoryDescriptor *fMem[2];
    uint32_t     fRes[2];
    uint64_t     fFlips, fLastNs;
    IOLock      *fLock;
    bool         fClosed;

    void     teardown();

    static IOReturn getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
    static IOReturn flip(OSObject *target, void *reference, IOExternalMethodArguments *args);
    static IOReturn exportBuffer(OSObject *target, void *reference, IOExternalMethodArguments *args);

public:
    static IOVirtIOGPUPresenter *withScanout(IOVirtIOGPU *gpu, uint32_t scanout, task_t task,
                                             void *securityID, UInt32 type);

    bool     start(IOService *provider) override;
    void     free() override;
    IOReturn clientClose(void) override;
    IOReturn clientDied(void) override;
    IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                 IOMemoryDescriptor **memory) override;
    IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                            IOExternalMethodDispatch *dispatch, OSObject *target,
                            void *reference) override;
};
