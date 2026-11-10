/*
 * IOVirtIOGPUPresenter.cpp - a compositor's flip pair on one virtio-gpu scanout.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include "IOVirtIOGPUPresenter.h"
#include "IOVirtIOGPU.h"

#include <IOKit/IOLib.h>
#include <kern/clock.h>

#define super IOUserClient

OSDefineMetaClassAndStructors(IOVirtIOGPUPresenter, IOUserClient);

IOVirtIOGPUPresenter *
IOVirtIOGPUPresenter::withScanout(IOVirtIOGPU *gpu, uint32_t scanout, task_t task, void *securityID,
                                  UInt32 type)
{
    IOVirtIOGPUPresenter *client = OSTypeAlloc(IOVirtIOGPUPresenter);

    if (client == NULL || !client->initWithTask(task, securityID, type)) {
        OSSafeReleaseNULL(client);
        return NULL;
    }
    client->fGPU = gpu;
    client->fScanout = scanout;
    return client;
}

// two 2D resources the size of the scanout, each over guest pages the compositor maps
bool
IOVirtIOGPUPresenter::start(IOService *provider)
{
    if (fGPU == NULL || !super::start(provider))
        return false;
    fLock = IOLockAlloc();
    if (fLock == NULL || !fGPU->scanoutSize(fScanout, &fWidth, &fHeight))
        return false;
    fPitch = fWidth * 4;

    size_t bytes = (size_t)fPitch * fHeight;

    for (int i = 0; i < 2; i++) {
        fMem[i] = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous, bytes, 0xFFFFFFFFULL);
        if (fMem[i] == NULL)
            goto fail;
        bzero(fMem[i]->getBytesNoCopy(), bytes);
        fRes[i] = fGPU->allocResourceId();
        if (!fGPU->gpuCreateResource2D(fRes[i], fWidth, fHeight)) {
            fRes[i] = 0;
            goto fail;
        }
        if (!fGPU->gpuAttachBacking(fRes[i], fMem[i]->getPhysicalAddress(), (uint32_t)bytes))
            goto fail;
    }
    if (!fGPU->presenterBegin(fScanout))
        goto fail;
    IOLog("IOVirtIOGPU: presenter opened on display %u, %ux%u\n", fScanout, fWidth, fHeight);
    return true;

fail:
    IOLog("IOVirtIOGPU: no presenter on display %u\n", fScanout);
    for (int i = 0; i < 2; i++) {
        if (fRes[i] != 0)
            fGPU->gpuResourceUnref(fRes[i]);
        fRes[i] = 0;
        OSSafeReleaseNULL(fMem[i]);
    }
    return false;
}

// the display gets its own buffer back, then the pair goes
void
IOVirtIOGPUPresenter::teardown()
{
    IOLockLock(fLock);
    bool closed = fClosed;
    fClosed = true;
    IOLockUnlock(fLock);
    if (closed)
        return;

    fGPU->presenterEnd(fScanout);
    for (int i = 0; i < 2; i++) {
        if (fRes[i] != 0)
            fGPU->gpuResourceUnref(fRes[i]);
        fRes[i] = 0;
    }
}

IOReturn
IOVirtIOGPUPresenter::clientClose(void)
{
    teardown();
    terminate();
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUPresenter::clientDied(void)
{
    return clientClose();
}

void
IOVirtIOGPUPresenter::free()
{
    // a mapping still in the task keeps its own reference to the pages
    for (int i = 0; i < 2; i++)
        OSSafeReleaseNULL(fMem[i]);
    if (fLock != NULL)
        IOLockFree(fLock);
    super::free();
}

IOReturn
IOVirtIOGPUPresenter::clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory)
{
    if (type > 1 || fMem[type] == NULL)
        return kIOReturnBadArgument;
    fMem[type]->retain();
    *memory = fMem[type];
    *options = 0;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPUPresenter::getInfo(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOVirtIOGPUPresenter *self = (IOVirtIOGPUPresenter *)target;

    args->scalarOutput[0] = self->fWidth;
    args->scalarOutput[1] = self->fHeight;
    args->scalarOutput[2] = self->fPitch;
    args->scalarOutput[3] = 2;
    return kIOReturnSuccess;
}

// the host takes the whole buffer, scans it out and repaints, all before the call returns
IOReturn
IOVirtIOGPUPresenter::flip(OSObject *target, void *, IOExternalMethodArguments *args)
{
    IOVirtIOGPUPresenter *self = (IOVirtIOGPUPresenter *)target;
    uint64_t index = args->scalarInput[0];
    uint64_t now, ns;

    if (index > 1)
        return kIOReturnBadArgument;
    IOLockLock(self->fLock);
    if (self->fClosed) {
        IOLockUnlock(self->fLock);
        return kIOReturnNotOpen;
    }
    // a refused flip sends the compositor to remap, the display changed size under it
    bool ok = self->fGPU->presenterShow(self->fScanout, self->fRes[index], self->fWidth, self->fHeight);
    if (ok) {
        now = mach_absolute_time();
        absolutetime_to_nanoseconds(now, &ns);
        self->fFlips++;
        if (self->fFlips <= 3 || self->fFlips % 1000 == 0)
            IOLog("IOVirtIOGPU: flip %llu to buffer %llu on display %u, %llu us after the last\n",
                  self->fFlips, index, self->fScanout, self->fLastNs ? (ns - self->fLastNs) / 1000 : 0);
        self->fLastNs = ns;
        args->scalarOutput[0] = self->fFlips;
        args->scalarOutput[1] = ns;
    }
    IOLockUnlock(self->fLock);
    return ok ? kIOReturnSuccess : kIOReturnNotReady;
}

// guest pages behind a 2D resource have no dma-buf, a renderer uses the 3D client instead
IOReturn
IOVirtIOGPUPresenter::exportBuffer(OSObject *, void *, IOExternalMethodArguments *)
{
    return kIOReturnUnsupported;
}

IOReturn
IOVirtIOGPUPresenter::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                     IOExternalMethodDispatch *, OSObject *, void *reference)
{
    static const IOExternalMethodDispatch methods[kPresenterMethodCount] = {
        { &IOVirtIOGPUPresenter::getInfo, 0, 0, 4, 0 },
        { &IOVirtIOGPUPresenter::flip, 1, 0, 2, 0 },
        { &IOVirtIOGPUPresenter::exportBuffer, 1, 0, 1, 0 },
    };

    if (selector >= kPresenterMethodCount)
        return kIOReturnBadArgument;
    return super::externalMethod(selector, args, (IOExternalMethodDispatch *)&methods[selector], this,
                                 reference);
}
