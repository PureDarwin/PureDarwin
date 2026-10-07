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
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
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
    VirtQueue         fCursorQ;
    bool              fCursorQOK;

    // control queue slots of one page, request at 0 and response at kCtrlRespOffset. fenced submits stay in
    // flight until the host retires the fence, so completions come back out of order, matched by head
    enum { kCtrlSlots = 32, kCtrlSlotBytes = 4096, kCtrlRespOffset = 2048, kCtrlSyncReserve = 4 };
    enum { kSlotFree = 0, kSlotBusy, kSlotDone, kSlotOrphan };
    struct CtrlSlot {
        IOBufferMemoryDescriptor *mem;
        uint8_t  *virt;
        uint64_t  phys;
        IOBufferMemoryDescriptor *payload;  // submit stream, released once the device returns it
        uint64_t  fenceId;                  // async slots only
        uint16_t  head;
        uint8_t   state;
        bool      async;
    };
    CtrlSlot     fSlots[kCtrlSlots];
    unsigned     fSlotCount;
    IOLock      *fQueueLock;
    volatile uint64_t fCompletionGen;

    // the queue interrupt reaps and wakes sleepers on fCompletionGen, polling covers a device without one
    IOWorkLoop             *fIrqLoop;
    IOInterruptEventSource *fIrqSource;
    bool                    fUseIrq;
    bool                    fIrqMsix;
    int                     fIrqSourceIndex;
    uint32_t                fIrqCount;
    uint32_t                fIrqMisses;
    bool     setupInterrupt();
    void     interruptOccurred(IOInterruptEventSource *source, int count);
    void     ctrlWaitLocked(uint64_t *spins, uint64_t limitUs);
    bool     fenceBusyLocked(uint64_t fenceId);

    void     ctrlReapLocked();
    bool     ctrlSubmit(const void *cmd, size_t cmdLen, IOBufferMemoryDescriptor *payload,
                        uint32_t payloadLen, void *resp, size_t respLen, bool async,
                        unsigned timeoutMs);

    // Framebuffer backing storage (guest RAM given to the host as the
    // scanout resource's backing pages).
    IOBufferMemoryDescriptor *fFbMem;
    void        *fFbBase;
    uint64_t     fFbPhys;
    uint32_t     fWidth;
    uint32_t     fHeight;
    uint32_t     fPitch;
    uint32_t     fResourceId;
    uint32_t     fScanoutResourceId;
    uint32_t     fScanoutWidth, fScanoutHeight; // geometry SET_SCANOUT was last given
    bool         fNativePresent;
    bool         fPresentPending;
    uint32_t     fPresentX1, fPresentY1, fPresentX2, fPresentY2;

    // virgl/3D state. fVirglOK is set when the device offered
    // VIRTIO_GPU_F_VIRGL, we acked it, and a usable capset was read.
    bool         fVirglOK;
    uint32_t     fVirglCapsetId;
    uint32_t     fVirglCapsetVersion;
    uint32_t     fVirglCapsetSize;
    uint8_t      fVirglCaps[1536];  // cached caps blob (VIRGL2 capset ~1408B)
    uint32_t     fVirglCapsLen;

    // every capset the host offers, for clients that pick their own (venus)
    enum { kMaxCapsets = 8 };
    struct CapsetInfo { uint32_t id, version, size; };
    CapsetInfo   fCapsets[kMaxCapsets];
    uint32_t     fCapsetCount;

    // venus: CONTEXT_INIT + RESOURCE_BLOB acked, a venus capset, and the host visible window.
    // blobs are mapped into the window at offsets handed out by a first-fit allocator
    uint32_t     fFeatures;
    bool         fVenusOK;
    uint64_t     fHostmemPhys;
    uint64_t     fHostmemSize;
    struct HostRange { uint64_t offset, size; uint32_t resId; };
    enum { kMaxHostRanges = 4096 };
    HostRange   *fHostRanges;
    uint32_t     fHostRangeCount;

    // host3d blobs, shared between connections: refs count every connection holding one plus its
    // exports. the window mapping is made once and handed to every task, the last ref unmaps and unrefs
    struct Blob {
        uint32_t resId, refs, blobMem, blobFlags, mapInfo;
        uint64_t size;
        IOMemoryDescriptor *window;
    };
    struct BlobExport { uint64_t token; uint32_t resId; const void *owner; };
    enum { kMaxBlobs = 8192, kMaxBlobExports = 1024 };
    Blob        *fBlobs;
    uint32_t     fBlobCount;
    BlobExport  *fBlobExports;
    IOLock      *fBlobLock;
    Blob    *findBlobLocked(uint32_t resId);
    void     blobDestroy(const Blob &b);

    IOLock      *fCtrlLock;
    uint32_t     fNextCtxId;        // monotonic, 3D contexts
    uint32_t     fNextResId;        // monotonic, starts above the 2D scanout id
    uint64_t     fNextFenceId;      // monotonic

    thread_call_t fFlushCall;

    // Hardware cursor. The image is a fixed 64x64 BGRA resource whose backing
    // pages stay attached for the life of the driver, only its contents and
    // position change.
    static const uint32_t kCursorEdge = 64;
    static const size_t   kCursorCmdBytes = 256;
    IOBufferMemoryDescriptor *fCursorMem;
    void        *fCursorVirt;       // command scratch
    uint64_t     fCursorCmdPhys;
    void        *fCursorImageBase;  // 64x64 BGRA pixels
    uint64_t     fCursorImagePhys;
    uint32_t     fCursorResourceId;
    uint32_t     fCursorX, fCursorY;
    uint32_t     fCursorHotX, fCursorHotY;
    IOLock      *fCursorLock;

    bool     sendCommand(const void *cmd, size_t cmdLen, void *resp, size_t respLen);
    bool     sendCursorCommand(const void *cmd, size_t cmdLen);

    bool     gpuGetDisplayInfo(uint32_t *outWidth, uint32_t *outHeight);
    bool     gpuCreateResource2D(uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuSetScanout(uint32_t scanoutId, uint32_t resourceId, uint32_t width, uint32_t height);
    bool     gpuTransferToHost2D(uint32_t resourceId, uint32_t x, uint32_t y,
                                 uint32_t width, uint32_t height);
    bool     gpuResourceFlush(uint32_t resourceId, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height);

    // Probe VIRGL capsets (GET_CAPSET_INFO/GET_CAPSET): exercises the 3D
    // control path end-to-end without rendering. Sets fVirgl* on success.
    bool     gpuGetCapsetInfo(uint32_t index, uint32_t *outId, uint32_t *outVer, uint32_t *outSize);
    bool     gpuGetCapset(uint32_t capsetId, uint32_t version, void *out, uint32_t size);
    void     gpuProbeVirgl();
    void     probeVenus();

    bool     gpuSetupCursorResource();

    // Notice a host-side display reconfiguration (config-space events_read)
    // and re-point the scanout at it.
    void     checkDisplayEvents();

    void     scheduleFlush();
    static void flushCallback(thread_call_param_t self, thread_call_param_t);

public:
    bool     gpuPresent(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

    // Hand the pixel-push cadence back to the driver. A client that presents
    // owns it for as long as it lives, this is how that ownership ends.
    void     releasePresentOwnership();

private:
    bool         fConsolePaused;
    // Stop/resume the kernel graphics console, which draws into the very same
    // framebuffer pages a client composites into.
    void     setConsoleDrawing(bool enable);
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
    bool     gpu3DSubmit(uint32_t ctxId, IOBufferMemoryDescriptor *cmd, uint32_t cmdLen,
                         uint64_t fenceId);

    // venus/context-init path
    bool     gpu3DCreateContextInit(uint32_t ctxId, const char *name, uint32_t contextInit);
    bool     gpu3DCtxDetachResource(uint32_t ctxId, uint32_t resId);
    bool     gpuCreateBlob(uint32_t ctxId, uint32_t resId, uint32_t blobMem, uint32_t blobFlags,
                           uint64_t blobId, uint64_t size);
    bool     gpuMapBlob(uint32_t resId, uint64_t offset, uint32_t *outMapInfo);
    bool     gpuUnmapBlob(uint32_t resId);
    // cmd may be NULL for an empty stream. with a fence the call returns once queued
    bool     gpuSubmitRing(uint32_t ctxId, bool useRing, uint32_t ring, IOBufferMemoryDescriptor *cmd,
                           uint32_t cmdLen, uint64_t fenceId);
    bool     fenceRetired(uint64_t fenceId);
    bool     waitFence(uint64_t fenceId, uint64_t timeoutUs);
    uint64_t waitProgress(uint64_t seenGen, uint64_t timeoutUs);
    bool     getCapset(uint32_t capsetId, uint32_t version, void *out, uint32_t *ioSize);

    bool     blobAdd(uint32_t resId, uint32_t blobMem, uint32_t blobFlags, uint64_t size);
    void     blobRelease(uint32_t resId);
    bool     blobMap(uint32_t resId, uint32_t *outMapInfo);
    IOMemoryDescriptor *blobWindow(uint32_t resId, uint32_t *outMapInfo); // retained
    // an export holds its own ref until the exporting connection closes, importing takes another
    bool     blobExport(uint32_t resId, const void *owner, uint64_t *outToken);
    bool     blobImport(uint64_t token, uint32_t *outResId, uint32_t *outBlobMem, uint32_t *outBlobFlags,
                        uint64_t *outSize);
    void     blobDropExports(const void *owner);

    bool     hostmemAlloc(uint32_t resId, uint64_t size, uint64_t *outOffset);
    void     hostmemFree(uint32_t resId);
    uint64_t hostmemPhys() const { return fHostmemPhys; }
    uint64_t hostmemSize() const { return fHostmemSize; }
    bool     venusAvailable() const { return fVenusOK; }
    uint32_t features() const { return fFeatures; }
    uint32_t capsetMask() const;

    bool     gpuSetScanoutResource(uint32_t resourceId, uint32_t width,
                                   uint32_t height);

    // Push CPU writes in a guest-backed surface to the host and repaint it.
    bool     gpuFlushSurface(uint32_t resourceId, uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height);

    // Allocate a linear guest-backed 2D resource for the PDSurface protocol.
    // The caller owns the returned backing and maps it to userland.
    bool     gpuCreateSurfaceResource(uint32_t width, uint32_t height,
                                      uint32_t *outResourceId,
                                      uint32_t *outStride,
                                      IOBufferMemoryDescriptor **outBacking);

    bool     gpuSetCursorImage(const void *argb, uint32_t width, uint32_t height,
                              uint32_t hotX, uint32_t hotY);
    bool     gpuMoveCursor(uint32_t x, uint32_t y);
    bool     cursorAvailable() const { return fCursorQOK; }
    IOBufferMemoryDescriptor *copyFramebufferMemory();

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
