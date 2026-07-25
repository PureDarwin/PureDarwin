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

    // virgl/3D state. fVirglOK is set when the device offered
    // VIRTIO_GPU_F_VIRGL, we acked it, and a usable capset was read.
    bool         fVirglOK;
    uint32_t     fVirglCapsetId;
    uint32_t     fVirglCapsetVersion;
    uint32_t     fVirglCapsetSize;
    uint8_t      fVirglCaps[1536];  // cached caps blob (VIRGL2 capset ~1408B)
    uint32_t     fVirglCapsLen;

    IOLock      *fCtrlLock;
    uint32_t     fNextCtxId;        // monotonic; 3D contexts
    uint32_t     fNextResId;        // monotonic; starts above the 2D scanout id
    uint64_t     fNextFenceId;      // monotonic

    thread_call_t fFlushCall;

    bool     sendCommand(const void *cmd, size_t cmdLen, void *resp, size_t respLen);

    bool     gpuGetDisplayInfo(uint32_t *outWidth, uint32_t *outHeight);
    bool     gpuCreateResource2D(uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuSetScanout(uint32_t scanoutId, uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuTransferToHost2D(uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuResourceFlush(uint32_t resourceId, uint32_t width, uint32_t height);

    // Probe VIRGL capsets (GET_CAPSET_INFO/GET_CAPSET): exercises the 3D
    // control path end-to-end without rendering. Sets fVirgl* on success.
    bool     gpuGetCapsetInfo(uint32_t index, uint32_t *outId, uint32_t *outVer, uint32_t *outSize);
    bool     gpuGetCapset(uint32_t capsetId, uint32_t version, void *out, uint32_t size);
    void     gpuProbeVirgl();

    void     scheduleFlush();
    static void flushCallback(thread_call_param_t self, thread_call_param_t);

public:
    bool     gpuAttachBacking(uint32_t resourceId, uint64_t phys, uint32_t size);
    bool     gpuResourceUnref(uint32_t resId);
    bool     gpu3DCreateContext(uint32_t ctxId, const char *name);
    bool     gpu3DDestroyContext(uint32_t ctxId);
    bool     gpu3DCreateResource(uint32_t resId, uint32_t target, uint32_t format,
                                 uint32_t bind, uint32_t width, uint32_t height);
    bool     gpu3DCtxAttachResource(uint32_t ctxId, uint32_t resId);
    bool     gpu3DTransfer(bool toHost, uint32_t ctxId, uint32_t resId,
                           uint32_t x, uint32_t y, uint32_t z,
                           uint32_t w, uint32_t h, uint32_t d,
                           uint32_t level, uint32_t stride, uint64_t offset,
                           uint64_t fenceId);
    bool     gpu3DTransferToHost(uint32_t ctxId, uint32_t resId, uint32_t width,
                                 uint32_t height, uint32_t stride, uint64_t fenceId);
    bool     gpu3DSubmit(uint32_t ctxId, uint64_t cmdPhys, uint32_t cmdLen, uint64_t fenceId);

    bool     virglAvailable() const { return fVirglOK; }
    const uint8_t *virglCaps(uint32_t *outLen) const { if (outLen) *outLen = fVirglCapsLen; return fVirglCaps; }
    uint32_t allocContextId();
    uint32_t allocResourceId();
    uint64_t allocFenceId();

    virtual IOReturn newUserClient(task_t owningTask, void *securityID, UInt32 type,
                                   IOUserClient **handler) override;


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
