/*
 * IOVirtIOGPU: minimal virtio-gpu 2D scanout driver. See IOVirtIOGPU.cpp
 * for scope notes. Transport (capability walking, virtqueues, handshake)
 * lives in IOVirtIOFamily's IOVirtIOTransport, shared with the other
 * virtio drivers. Register/protocol layout from the public VIRTIO 1.1
 * spec (PCI transport, section 4.1) and the virtio-gpu device spec
 * (section 5.7) - no Apple source exists for this device class.
 */
#pragma once

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>
#include "IOVirtIOTransport.h"

class IOVirtIOGPU : public IOFramebuffer
{
    OSDeclareDefaultStructors(IOVirtIOGPU);

private:
    IOPCIDevice      *fPCIDevice;
    IOVirtIOTransport fTransport;
    VirtQueue         fControlQ;

    // Command/response scratch buffer (physically contiguous, one at a
    // time - polling model, no concurrent commands).
    IOBufferMemoryDescriptor *fCmdMem;
    void        *fCmdVirt;
    uint64_t     fCmdPhys;

    // Framebuffer backing storage (guest RAM given to the host as the
    // scanout resource's backing pages).
    IOBufferMemoryDescriptor *fFbMem;
    void        *fFbBase;
    uint64_t     fFbPhys;
    uint32_t     fWidth;
    uint32_t     fHeight;
    uint32_t     fPitch;
    uint32_t     fResourceId;

    thread_call_t fFlushCall;

    bool     sendCommand(const void *cmd, size_t cmdLen, void *resp, size_t respLen);

    bool     gpuGetDisplayInfo(uint32_t *outWidth, uint32_t *outHeight);
    bool     gpuCreateResource2D(uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuAttachBacking(uint32_t resourceId, uint64_t phys, uint32_t size);
    bool     gpuSetScanout(uint32_t scanoutId, uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuTransferToHost2D(uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuResourceFlush(uint32_t resourceId, uint32_t width, uint32_t height);

    void     scheduleFlush();
    static void flushCallback(thread_call_param_t self, thread_call_param_t);

public:
    IOService * probe(IOService * provider, SInt32 * score) override;
    virtual bool start(IOService * provider) override;
    virtual void stop(IOService * provider) override;

    virtual IOReturn enableController() override;

    virtual const char * getPixelFormats() override;
    virtual IOReturn getCurrentDisplayMode(IODisplayModeID * displayMode,
                                           IOIndex * depth) override;

    virtual IOReturn setDisplayMode(IODisplayModeID displayMode,
                                    IOIndex depth) override;

    virtual IODeviceMemory * getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory * getVRAMRange(void) override;

    virtual IOReturn getInformationForDisplayMode(
        IODisplayModeID displayMode,
        IODisplayModeInformation * info) override;

    virtual UInt64 getPixelFormatsForDisplayMode(
        IODisplayModeID displayMode,
        IOIndex depth) override;

    virtual IOReturn getPixelInformation(
        IODisplayModeID displayMode, IOIndex depth,
        IOPixelAperture aperture, IOPixelInformation * info ) override;

    virtual IOReturn getDisplayModes(IODisplayModeID * allDisplayModes) override;

    virtual IOItemCount getDisplayModeCount( void ) override;
};
