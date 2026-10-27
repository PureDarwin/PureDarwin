/*
 * IOVirtIOGPUSurfaceClient.h - IOVirtIOGPU's implementation of the PDSurface
 * protocol.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#pragma once

#include <IOKit/IOUserClient.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include "PDSurfaceProtocol.h"

class IOVirtIOGPU;

class IOVirtIOGPUSurfaceClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOVirtIOGPUSurfaceClient);

    enum { kMaxSurfaces = 256 };

    struct SurfaceEntry {
        uint32_t                  resourceId;
        uint32_t                  width, height, format, usage, stride;
        uint64_t                  length;
        IOBufferMemoryDescriptor *backing;
    };

private:
    IOVirtIOGPU *fOwner;
    task_t       fTask;

    SurfaceEntry fSurfaces[kMaxSurfaces];
    uint32_t     fCount;

    SurfaceEntry *find(uint64_t surfaceId);
    void          destroyAll();

    IOReturn mGetVersion(IOExternalMethodArguments *a);
    IOReturn mCreate(IOExternalMethodArguments *a);
    IOReturn mDestroy(IOExternalMethodArguments *a);
    IOReturn mLookup(IOExternalMethodArguments *a);
    IOReturn mSetScanout(IOExternalMethodArguments *a);
    IOReturn mFlush(IOExternalMethodArguments *a);

public:
    static IOVirtIOGPUSurfaceClient *withOwner(IOVirtIOGPU *owner, task_t task);

    virtual bool     start(IOService *provider) override;
    virtual void     stop(IOService *provider) override;
    virtual IOReturn clientClose(void) override;

    virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
                                    IOExternalMethodDispatch *dispatch, OSObject *target,
                                    void *reference) override;

    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options,
                                         IOMemoryDescriptor **memory) override;
};
