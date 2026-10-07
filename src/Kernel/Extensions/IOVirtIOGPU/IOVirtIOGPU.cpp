/*
 * IOVirtIOGPU: minimal virtio-gpu 2D scanout driver for QEMU's
 * -device virtio-gpu-pci. Uses IOVirtIOFamily's IOVirtIOTransport for
 * the virtio-1.0 PCI transport (capability walking, handshake, split
 * virtqueues; completions reaped on the legacy intx line or polled), and layers
 * the virtio-gpu 2D control protocol on top:
 *   GET_DISPLAY_INFO -> RESOURCE_CREATE_2D -> RESOURCE_ATTACH_BACKING ->
 *   SET_SCANOUT, then TRANSFER_TO_HOST_2D + RESOURCE_FLUSH on a timer.
 * Register/protocol layout from the public VIRTIO 1.1 spec (PCI
 * transport, section 4.1; virtio-gpu device, section 5.7)
 */

#include "IOVirtIOGPU.h"
#include "IOVirtIOGPUSurfaceClient.h"
#include "IOVirtIOGPU3DShared.h"
#include "IOVirtIOGPUUserClient.h"
#include "IOVirtIOGPUVenusClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread_call.h>
#include <sys/errno.h>
#include <sys/random.h>

extern "C" int switch_to_video_console(void);
extern "C" boolean_t PE_parse_boot_argn(const char *arg_string, void *arg_ptr, int max_arg);
extern "C" void vc_progress_set(boolean_t enable, uint32_t vc_delay);

#define super IOFramebuffer
OSDefineMetaClassAndStructors(IOVirtIOGPU, IOFramebuffer);

#define kDisplayModeID 1
#define kDepth         0
#define kDefaultWidth  1024
#define kDefaultHeight 768
#define kFlushIntervalMs 16   // fallback cadence, native presents wake it immediately

static bool gVGPUDebug;
static bool gVGPUDebugChecked;

static bool
vgpu_debug_enabled()
{
    if (!gVGPUDebugChecked) {
        PE_parse_boot_argn("vgpu_debug", &gVGPUDebug, sizeof(gVGPUDebug));
        gVGPUDebugChecked = true;
    }
    return gVGPUDebug;
}

#define DEBUG(x, ...) do {                                       \
    if (vgpu_debug_enabled())                                    \
        kprintf("IOVirtIOGPU: " x, ##__VA_ARGS__);                \
} while (0)

// virtio-gpu control protocol (virtio-gpu device spec sec 5.7)
enum {
    VIRTIO_GPU_CMD_GET_DISPLAY_INFO       = 0x0100,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_2D     = 0x0101,
    VIRTIO_GPU_CMD_RESOURCE_UNREF         = 0x0102,
    VIRTIO_GPU_CMD_SET_SCANOUT            = 0x0103,
    VIRTIO_GPU_CMD_RESOURCE_FLUSH         = 0x0104,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D    = 0x0105,
    VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING = 0x0106,

    // 3D/virgl control commands (spec sec 5.7, GL bind). Only reachable
    // when the device offered VIRTIO_GPU_F_VIRGL and we acked it.
    VIRTIO_GPU_CMD_CTX_CREATE            = 0x0200,
    VIRTIO_GPU_CMD_CTX_DESTROY           = 0x0201,
    VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE   = 0x0202,
    VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE   = 0x0203,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_3D    = 0x0204,
    VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D   = 0x0205,
    VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D = 0x0206,
    VIRTIO_GPU_CMD_SUBMIT_3D             = 0x0207,
    VIRTIO_GPU_CMD_GET_CAPSET_INFO       = 0x0108,
    VIRTIO_GPU_CMD_GET_CAPSET            = 0x0109,
    VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB  = 0x010c,
    VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB     = 0x0208,
    VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB   = 0x0209,

    // Cursor queue (queue 1). The device consumes these without writing a
    // response, unlike everything on the control queue.
    VIRTIO_GPU_CMD_UPDATE_CURSOR         = 0x0300,
    VIRTIO_GPU_CMD_MOVE_CURSOR           = 0x0301,

    VIRTIO_GPU_RESP_OK_NODATA        = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO  = 0x1101,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO   = 0x1102,
    VIRTIO_GPU_RESP_OK_CAPSET        = 0x1103,
    VIRTIO_GPU_RESP_OK_MAP_INFO      = 0x1106,
};

// virtio-gpu device feature bits (low word). VIRGL = 3D/OpenGL support.
enum {
    VIRTIO_GPU_F_VIRGL         = (1u << 0),
    VIRTIO_GPU_F_RESOURCE_BLOB = (1u << 3),
    VIRTIO_GPU_F_CONTEXT_INIT  = (1u << 4),
};

// shmid of the host visible memory window
enum { VIRTIO_GPU_SHM_ID_HOST_VISIBLE = 1 };

// Control-header flag: request a fence for this command. In the split
// virtqueue the used-ring writeback for a fenced command lands only after
// the host retires the GPU work, so polling the completion (sendCommand
// already does) is the fence wait - no separate fence queue needed in v1.
enum {
    VIRTIO_GPU_FLAG_FENCE         = (1u << 0),
    VIRTIO_GPU_FLAG_INFO_RING_IDX = (1u << 1),
};

// Capset ids the host advertises. VIRGL2 is virglrenderer's modern GL
// capability set, VIRGL(1) is the legacy one. We probe VIRGL2 first.
enum {
    VIRTIO_GPU_CAPSET_VIRGL  = 1,
    VIRTIO_GPU_CAPSET_VIRGL2 = 2,
    VIRTIO_GPU_CAPSET_VENUS  = 4,
};

enum { VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1 };

enum {
    kGpuCfgEventsRead   = 0,
    kGpuCfgEventsClear  = 4,
    kGpuCfgNumScanouts  = 8,
    kGpuCfgNumCapsets   = 12,
};
enum { VIRTIO_GPU_EVENT_DISPLAY = (1u << 0) };

struct VGpuCtrlHdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t  ring_idx;
    uint8_t  padding[3];
};

struct VGpuRect { uint32_t x, y, width, height; };

#define kMaxScanouts 16
struct VGpuDisplayOne { VGpuRect r; uint32_t enabled; uint32_t flags; };
struct VGpuRespDisplayInfo {
    VGpuCtrlHdr hdr;
    VGpuDisplayOne pmodes[kMaxScanouts];
};

struct VGpuResourceCreate2D {
    VGpuCtrlHdr hdr;
    uint32_t resource_id, format, width, height;
};

struct VGpuCursorPos { uint32_t scanout_id, x, y, padding; };
struct VGpuUpdateCursor {
    VGpuCtrlHdr hdr;
    VGpuCursorPos pos;
    uint32_t resource_id;
    uint32_t hot_x, hot_y;
    uint32_t padding;
};

struct VGpuMemEntry { uint64_t addr; uint32_t length; uint32_t padding; };
struct VGpuResourceAttachBacking {
    VGpuCtrlHdr hdr;
    uint32_t resource_id, nr_entries;
};

struct VGpuSetScanout {
    VGpuCtrlHdr hdr;
    VGpuRect r;
    uint32_t scanout_id, resource_id;
};

struct VGpuTransferToHost2D {
    VGpuCtrlHdr hdr;
    VGpuRect r;
    uint64_t offset;
    uint32_t resource_id, padding;
};

struct VGpuResourceFlush {
    VGpuCtrlHdr hdr;
    VGpuRect r;
    uint32_t resource_id, padding;
};

// 3D/virgl capset query (spec sec 5.7).
struct VGpuGetCapsetInfo {
    VGpuCtrlHdr hdr;
    uint32_t capset_index;
    uint32_t padding;
};
struct VGpuRespCapsetInfo {
    VGpuCtrlHdr hdr;
    uint32_t capset_id;
    uint32_t capset_max_version;
    uint32_t capset_max_size;
    uint32_t padding;
};
struct VGpuGetCapset {
    VGpuCtrlHdr hdr;
    uint32_t capset_id;
    uint32_t capset_version;
};
// Response is VGpuCtrlHdr followed by capset_max_size bytes of caps blob.
// The virgl2 capset is ~700 bytes, the resp half of the 4KB scratch is
// 2048 bytes, so it fits in a request slot.

// 3D/virgl command payloads (spec sec 5.7). Context-scoped commands carry
// the context id in hdr.ctx_id, a fenced command sets VIRTIO_GPU_FLAG_FENCE
// and hdr.fence_id.
struct VGpuCtxCreate {
    VGpuCtrlHdr hdr;
    uint32_t nlen;          // length of debug_name
    uint32_t context_init;  // 0 with legacy VIRGL (no CONTEXT_INIT) = virgl ctx
    char     debug_name[64];
};
struct VGpuCtxResource {    // CTX_ATTACH_RESOURCE / CTX_DETACH_RESOURCE
    VGpuCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};
struct VGpuResourceCreate3D {
    VGpuCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t target;        // pipe_texture_target (2 = TEXTURE_2D)
    uint32_t format;        // virgl_formats
    uint32_t bind;          // VIRGL_BIND_*
    uint32_t width, height, depth;
    uint32_t array_size;
    uint32_t last_level;
    uint32_t nr_samples;
    uint32_t flags;
    uint32_t padding;
};
struct VGpuBox { uint32_t x, y, z, w, h, d; };
struct VGpuTransferHost3D {
    VGpuCtrlHdr hdr;
    VGpuBox  box;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t level;
    uint32_t stride;
    uint32_t layer_stride;
};
struct VGpuCmdSubmit3D {    // followed by `size` bytes of virgl command stream
    VGpuCtrlHdr hdr;
    uint32_t size;
    uint32_t padding;
};

// blob resources (spec sec 5.7.6.1/5.7.6.2), host3d blobs carry no backing entries
struct VGpuResourceCreateBlob {
    VGpuCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t blob_mem;
    uint32_t blob_flags;
    uint32_t nr_entries;
    uint64_t blob_id;
    uint64_t size;
};
struct VGpuResourceMapBlob {
    VGpuCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t padding;
    uint64_t offset;
};
struct VGpuRespMapInfo {
    VGpuCtrlHdr hdr;
    uint32_t map_info;
    uint32_t padding;
};
struct VGpuResourceUnmapBlob {
    VGpuCtrlHdr hdr;
    uint32_t resource_id;
    uint32_t padding;
};

// virgl resource bind bits + pipe target/format subset used here.
enum {
    VIRGL_BIND_RENDER_TARGET = (1u << 1),
    VIRGL_BIND_SAMPLER_VIEW  = (1u << 3),
    PIPE_TEXTURE_2D          = 2,
    VIRGL_FORMAT_B8G8R8A8_UNORM = 1,
};

IOService *
IOVirtIOGPU::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return NULL;

    UInt16 vendor = pci->configRead16(kIOPCIConfigVendorID);
    UInt16 device = pci->configRead16(kIOPCIConfigDeviceID);
    DEBUG("probe vendor=0x%04x device=0x%04x\n", vendor, device);

    // Red Hat/Virtio vendor. Modern virtio-gpu (post virtio-1.0
    // transitional) is device id 0x1050, legacy/transitional is 0x1010 -
    // match both, QEMU's default is the modern id.
    if (vendor != 0x1af4)
        return NULL;
    if (device != 0x1050 && device != 0x1010)
        return NULL;

    if (score)
        *score = 6000; // outrank IOGOPFramebuffer's EFI GOP path when both match
    return this;
}

// reap every chain the device has returned. sync slots become done for their waiter, async and
// abandoned ones are recycled here. caller holds fQueueLock
void
IOVirtIOGPU::ctrlReapLocked()
{
    uint16_t head;

    while (fTransport.pollForCompletion(&fControlQ, 0, NULL, &head)) {
        fTransport.freeDescChain(&fControlQ, head);
        fCompletionGen++;
        IOLockWakeup(fQueueLock, (event_t)&fCompletionGen, false);

        for (unsigned i = 0; i < fSlotCount; i++) {
            CtrlSlot *s = &fSlots[i];
            if (s->head != head || (s->state != kSlotBusy && s->state != kSlotOrphan))
                continue;

            if (s->payload) {
                s->payload->release();
                s->payload = NULL;
            }
            if (s->async && ((VGpuCtrlHdr *)(s->virt + kCtrlRespOffset))->type != VIRTIO_GPU_RESP_OK_NODATA)
                DEBUG("fenced command 0x%x (fence %llu) failed: resp=0x%x\n",
                      ((VGpuCtrlHdr *)s->virt)->type, s->fenceId,
                      ((VGpuCtrlHdr *)(s->virt + kCtrlRespOffset))->type);
            s->state = (s->state == kSlotBusy && !s->async) ? kSlotDone : kSlotFree;
            break;
        }
    }
}

static void
ctrlPause(uint64_t *spins)
{
    // spin briefly so fast completions cost microseconds, then sleep in 100 us slices
    if (*spins < 200) {
        IODelay(1);
        (*spins)++;
    } else {
        delay_for_interval(100, kMicrosecondScale);
        *spins += 100;
    }
}

// caller holds fQueueLock and has just reaped. spins briefly, then sleeps until the interrupt reaps a
// completion, or polls in 100 us slices without one, *spins accumulates the microseconds spent
void
IOVirtIOGPU::ctrlWaitLocked(uint64_t *spins, uint64_t limitUs)
{
    // a completion the interrupt never reported within this long means the line is dead. a working
    // interrupt wakes the sleeper at once, so the slice only bounds the one-off stall on a dead line
    static const uint64_t kLostInterruptUs = 20000;
    uint64_t gen = fCompletionGen, start, now, deadline, ns, sliceUs;

    if (!fUseIrq || *spins < 200) {
        IOLockUnlock(fQueueLock);
        ctrlPause(spins);
        IOLockLock(fQueueLock);
        return;
    }

    sliceUs = limitUs > *spins ? limitUs - *spins : 1;
    if (sliceUs > kLostInterruptUs)
        sliceUs = kLostInterruptUs;
    clock_get_uptime(&start);
    clock_interval_to_deadline((uint32_t)sliceUs, kMicrosecondScale, &deadline);
    wait_result_t r = IOLockSleepDeadline(fQueueLock, (event_t)&fCompletionGen, deadline, THREAD_UNINT);
    clock_get_uptime(&now);
    absolutetime_to_nanoseconds(now - start, &ns);
    *spins += ns / 1000 + 1;

    if (r != THREAD_TIMED_OUT)
        return;
    ctrlReapLocked();
    // a line that ever delivered is alive, and one late work loop is not a dead line: two misses
    if (fCompletionGen == gen || fIrqCount != 0 || ++fIrqMisses < 2)
        return;
    IOLog("IOVirtIOGPU: request completed without an interrupt (source %d, %u taken), polling from now on\n",
          fIrqSourceIndex, fIrqCount);
    fUseIrq = false;
}

// queue one request in a slot, header (+ response) with an optional payload between. sync callers wait for
// their completion, async ones (fenced submits) return once it is queued and leave the payload to the slot
bool
IOVirtIOGPU::ctrlSubmit(const void *cmd, size_t cmdLen, IOBufferMemoryDescriptor *payload,
                        uint32_t payloadLen, void *resp, size_t respLen, bool async,
                        unsigned timeoutMs)
{
    if (!fQueueLock || !fSlotCount || cmdLen > kCtrlRespOffset ||
        respLen > kCtrlSlotBytes - kCtrlRespOffset) {
        DEBUG("ctrlSubmit: no queue or request too large\n");
        return false;
    }

    uint64_t limitUs = (uint64_t)timeoutMs * 1000;
    uint64_t spins = 0;
    CtrlSlot *s = NULL;

    IOLockLock(fQueueLock);
    for (;;) {
        ctrlReapLocked();

        unsigned nfree = 0;
        CtrlSlot *cand = NULL;
        for (unsigned i = 0; i < fSlotCount; i++) {
            if (fSlots[i].state != kSlotFree)
                continue;
            nfree++;
            if (!cand)
                cand = &fSlots[i];
        }
        // fenced work may not take the last slots, the flush timer and waiters still need them
        if (cand && (!async || nfree > kCtrlSyncReserve)) {
            s = cand;
            break;
        }

        if (spins >= limitUs) {
            IOLockUnlock(fQueueLock);
            DEBUG("ctrlSubmit: no free request slot\n");
            return false;
        }
        ctrlWaitLocked(&spins, limitUs);
    }

    memcpy(s->virt, cmd, cmdLen);
    bzero(s->virt + kCtrlRespOffset, respLen ? respLen : sizeof(VGpuCtrlHdr));
    if (!respLen)
        respLen = sizeof(VGpuCtrlHdr);

    VirtIOChainEntry chain[3];
    unsigned n = 0;
    chain[n++] = { s->phys, (uint32_t)cmdLen, false };
    if (payload && payloadLen)
        chain[n++] = { payload->getPhysicalAddress(), payloadLen, false };
    chain[n++] = { s->phys + kCtrlRespOffset, (uint32_t)respLen, true };

    int head = fTransport.addDescChainFree(&fControlQ, chain, n);
    if (head < 0) {
        IOLockUnlock(fQueueLock);
        DEBUG("ctrlSubmit: virtqueue full\n");
        return false;
    }

    s->head = (uint16_t)head;
    s->async = async;
    s->state = kSlotBusy;
    s->fenceId = async ? ((const VGpuCtrlHdr *)cmd)->fence_id : 0;
    if (payload && payloadLen) {
        payload->retain();
        s->payload = payload;
    }
    fTransport.notify(&fControlQ);

    if (async) {
        IOLockUnlock(fQueueLock);
        return true;
    }

    spins = 0;
    for (;;) {
        ctrlReapLocked();
        if (s->state == kSlotDone)
            break;

        if (spins >= limitUs) {
            // the device still owns the chain, the reaper recycles it whenever it comes back
            s->state = kSlotOrphan;
            IOLockUnlock(fQueueLock);
            DEBUG("ctrlSubmit: command 0x%x timed out\n", ((const VGpuCtrlHdr *)cmd)->type);
            return false;
        }
        ctrlWaitLocked(&spins, limitUs);
    }

    if (resp)
        memcpy(resp, s->virt + kCtrlRespOffset, respLen);
    s->state = kSlotFree;
    IOLockUnlock(fQueueLock);
    return true;
}

bool
IOVirtIOGPU::sendCommand(const void *cmd, size_t cmdLen, void *resp, size_t respLen)
{
    return ctrlSubmit(cmd, cmdLen, NULL, 0, resp, respLen, false, 1000);
}

bool
IOVirtIOGPU::gpuGetDisplayInfo(uint32_t *outWidth, uint32_t *outHeight)
{
    VGpuCtrlHdr req; bzero(&req, sizeof(req));
    req.type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;

    VGpuRespDisplayInfo resp; bzero(&resp, sizeof(resp));
    if (!sendCommand(&req, sizeof(req), &resp, sizeof(resp)))
        return false;

    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_DISPLAY_INFO)
        return false;

    for (unsigned i = 0; i < kMaxScanouts; i++) {
        if (resp.pmodes[i].enabled && resp.pmodes[i].r.width && resp.pmodes[i].r.height) {
            *outWidth  = resp.pmodes[i].r.width;
            *outHeight = resp.pmodes[i].r.height;
            return true;
        }
    }
    return false; // no scanout enabled - caller falls back to a default mode
}

bool
IOVirtIOGPU::gpuCreateResource2D(uint32_t resourceId, uint32_t width, uint32_t height)
{
    VGpuResourceCreate2D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    req.resource_id = resourceId;
    req.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    req.width = width;
    req.height = height;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuAttachBacking(uint32_t resourceId, uint64_t phys, uint32_t size)
{
    struct { VGpuResourceAttachBacking hdr; VGpuMemEntry entry; } req;
    bzero(&req, sizeof(req));
    req.hdr.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    req.hdr.resource_id = resourceId;
    req.hdr.nr_entries = 1;
    req.entry.addr = phys;
    req.entry.length = size;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuSetScanout(uint32_t scanoutId, uint32_t resourceId, uint32_t width, uint32_t height)
{
    VGpuSetScanout req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    req.r.width = width;
    req.r.height = height;
    req.scanout_id = scanoutId;
    req.resource_id = resourceId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuFlushSurface(uint32_t resourceId, uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height)
{
    return gpuTransferToHost2D(resourceId, x, y, width, height) &&
           gpuResourceFlush(resourceId, x, y, width, height);
}

bool
IOVirtIOGPU::gpuCreateSurfaceResource(uint32_t width, uint32_t height,
                                      uint32_t *outResourceId,
                                      uint32_t *outStride,
                                      IOBufferMemoryDescriptor **outBacking)
{
    if (!width || !height || !outResourceId || !outStride || !outBacking)
        return false;

    uint32_t stride = width * 4;
    size_t bytes = (size_t)stride * height;
    IOBufferMemoryDescriptor *backing =
        IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            bytes, 0xFFFFFFFFULL);
    if (!backing)
        return false;
    bzero(backing->getBytesNoCopy(), bytes);

    uint32_t resourceId = allocResourceId();
    if (!gpuCreateResource2D(resourceId, width, height) ||
        !gpuAttachBacking(resourceId, backing->getPhysicalAddress(),
                          (uint32_t)bytes)) {
        backing->release();
        return false;
    }

    *outResourceId = resourceId;
    *outStride = stride;
    *outBacking = backing;
    return true;
}

// Point the scanout at a resource other than the driver's own 2D framebuffer,
// so a client that renders on the host GPU can be displayed without its pixels
// ever travelling back through guest memory. Passing resource 0 restores the
// framebuffer, which is also what happens if the client goes away.
bool
IOVirtIOGPU::gpuSetScanoutResource(uint32_t resourceId, uint32_t width,
                                   uint32_t height)
{
    if (resourceId == 0) {
        resourceId = fResourceId;
        width = fWidth;
        height = fHeight;
    }
    if (width == 0 || height == 0 || width > fWidth || height > fHeight)
        return false;

    if (!gpuSetScanout(0, resourceId, width, height))
        return false;

    fScanoutResourceId = resourceId;
    fScanoutWidth = width;
    fScanoutHeight = height;
    // Whatever the resource already holds on the host is what appears, a
    // guest-backed surface has to be pushed with gpuFlushSurface() first.
    return gpuResourceFlush(resourceId, 0, 0, width, height);
}

bool
IOVirtIOGPU::gpuTransferToHost2D(uint32_t resourceId, uint32_t x, uint32_t y,
                                 uint32_t width, uint32_t height)
{
    VGpuTransferToHost2D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.x = x;
    req.r.y = y;
    req.r.width = width;
    req.r.height = height;
    req.resource_id = resourceId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

// One physically contiguous allocation holds both the cursor command scratch
// and the 64x64 image, so the image pages can stay attached to the resource for
// the life of the driver.
bool
IOVirtIOGPU::gpuSetupCursorResource()
{
    if (!fCursorQOK)
        return false;

    size_t imageBytes = (size_t)kCursorEdge * kCursorEdge * 4;
    fCursorMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        kCursorCmdBytes + imageBytes, 0xFFFFFFFFULL);
    if (!fCursorMem) {
        DEBUG("failed to allocate cursor buffer\n");
        return false;
    }

    fCursorVirt = fCursorMem->getBytesNoCopy();
    fCursorCmdPhys = fCursorMem->getPhysicalAddress();
    bzero(fCursorVirt, kCursorCmdBytes + imageBytes);
    fCursorImageBase = (uint8_t *)fCursorVirt + kCursorCmdBytes;
    fCursorImagePhys = fCursorCmdPhys + kCursorCmdBytes;

    fCursorLock = IOLockAlloc();
    if (!fCursorLock) {
        DEBUG("failed to allocate cursor lock\n");
        return false;
    }

    fCursorResourceId = allocResourceId();
    if (!gpuCreateResource2D(fCursorResourceId, kCursorEdge, kCursorEdge) ||
        !gpuAttachBacking(fCursorResourceId, fCursorImagePhys,
                          (uint32_t)imageBytes)) {
        DEBUG("failed to create cursor resource\n");
        return false;
    }

    DEBUG("hardware cursor ready (resource %u)\n", fCursorResourceId);
    return true;
}

// Cursor-queue commands carry no response: the device consumes the buffer and
// returns it with zero written length, so there is nothing to read back.
bool
IOVirtIOGPU::sendCursorCommand(const void *cmd, size_t cmdLen)
{
    if (!fCursorQOK || !fCursorVirt || cmdLen > kCursorCmdBytes)
        return false;

    IOLockLock(fCursorLock);
    memcpy(fCursorVirt, cmd, cmdLen);
    VirtIOChainEntry chain[1] = { { fCursorCmdPhys, (uint32_t)cmdLen, false } };
    fTransport.addDescChain(&fCursorQ, chain, 1);
    fTransport.notify(&fCursorQ);
    bool ok = fTransport.pollForCompletion(&fCursorQ, 100);
    IOLockUnlock(fCursorLock);
    return ok;
}

bool
IOVirtIOGPU::gpuSetCursorImage(const void *argb, uint32_t width, uint32_t height,
                               uint32_t hotX, uint32_t hotY)
{
    if (!fCursorQOK || !fCursorImageBase)
        return false;

    // virtio-gpu cursors are a fixed 64x64, anything smaller is placed in the
    // top-left corner and the remainder left transparent.
    if (width > kCursorEdge || height > kCursorEdge)
        return false;

    bzero(fCursorImageBase, kCursorEdge * kCursorEdge * 4);
    if (argb != NULL) {
        for (uint32_t row = 0; row < height; row++) {
            memcpy((uint8_t *)fCursorImageBase + row * kCursorEdge * 4,
                   (const uint8_t *)argb + (size_t)row * width * 4,
                   (size_t)width * 4);
        }
    }

    if (!gpuTransferToHost2D(fCursorResourceId, 0, 0, kCursorEdge, kCursorEdge))
        return false;

    VGpuUpdateCursor req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_UPDATE_CURSOR;
    req.pos.scanout_id = 0;
    req.pos.x = fCursorX;
    req.pos.y = fCursorY;
    // A null image hides the cursor, which virtio-gpu spells as resource 0.
    req.resource_id = (argb != NULL) ? fCursorResourceId : 0;
    req.hot_x = hotX;
    req.hot_y = hotY;
    fCursorHotX = hotX;
    fCursorHotY = hotY;
    return sendCursorCommand(&req, sizeof(req));
}

bool
IOVirtIOGPU::gpuMoveCursor(uint32_t x, uint32_t y)
{
    if (!fCursorQOK)
        return false;

    fCursorX = x;
    fCursorY = y;

    VGpuUpdateCursor req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_MOVE_CURSOR;
    req.pos.scanout_id = 0;
    req.pos.x = x;
    req.pos.y = y;
    // MOVE_CURSOR ignores the resource and hotspot fields, but the host keeps
    // whatever UPDATE_CURSOR last set.
    req.resource_id = fCursorResourceId;
    req.hot_x = fCursorHotX;
    req.hot_y = fCursorHotY;
    return sendCursorCommand(&req, sizeof(req));
}

bool
IOVirtIOGPU::gpuResourceFlush(uint32_t resourceId, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height)
{
    VGpuResourceFlush req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.r.x = x;
    req.r.y = y;
    req.r.width = width;
    req.r.height = height;
    req.resource_id = resourceId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

void
IOVirtIOGPU::setConsoleDrawing(bool enable)
{
    IOPlatformExpert *pe = getPlatform();
    if (!pe || enable == !fConsolePaused)
        return;

    pe->setConsoleInfo(NULL, enable ? kPEEnableScreen : kPEDisableScreen);
    fConsolePaused = !enable;
    DEBUG("kernel console %s\n", enable ? "resumed" : "paused for client");
}

void
IOVirtIOGPU::releasePresentOwnership()
{
    IOLockLock(fCtrlLock);
    fNativePresent = false;
    fPresentPending = false;
    IOLockUnlock(fCtrlLock);

    setConsoleDrawing(true);
}

bool
IOVirtIOGPU::gpuPresent(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!fFbBase || x >= fWidth || y >= fHeight)
        return false;
    if (width > fWidth - x)
        width = fWidth - x;
    if (height > fHeight - y)
        height = fHeight - y;
    if (!width || !height)
        return true;

    /* Queue the region for the periodic control-queue worker.  The old path
     * synchronously waited for two VirtIO responses on every mouse motion,
     * which made XWayland clients block input while native Wayland appeared
     * responsive. */
    IOLockLock(fCtrlLock);
    bool tookOver = !fNativePresent;
    fNativePresent = true;
    if (!fPresentPending) {
        fPresentX1 = x;
        fPresentY1 = y;
        fPresentX2 = x + width;
        fPresentY2 = y + height;
        fPresentPending = true;
    } else {
        if (x < fPresentX1) fPresentX1 = x;
        if (y < fPresentY1) fPresentY1 = y;
        if (x + width > fPresentX2) fPresentX2 = x + width;
        if (y + height > fPresentY2) fPresentY2 = y + height;
    }
    IOLockUnlock(fCtrlLock);

    // Outside the lock: setConsoleInfo reaches into the console machinery and
    // has no business running with a driver lock held.
    if (tookOver)
        setConsoleDrawing(false);
    return true;
}

bool
IOVirtIOGPU::gpuGetCapsetInfo(uint32_t index, uint32_t *outId, uint32_t *outVer, uint32_t *outSize)
{
    VGpuGetCapsetInfo req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_GET_CAPSET_INFO;
    req.capset_index = index;

    VGpuRespCapsetInfo resp; bzero(&resp, sizeof(resp));
    if (!sendCommand(&req, sizeof(req), &resp, sizeof(resp)))
        return false;
    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_CAPSET_INFO)
        return false;

    *outId   = resp.capset_id;
    *outVer  = resp.capset_max_version;
    *outSize = resp.capset_max_size;
    return true;
}

bool
IOVirtIOGPU::gpuGetCapset(uint32_t capsetId, uint32_t version, void *out, uint32_t size)
{
    VGpuGetCapset req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_GET_CAPSET;
    req.capset_id = capsetId;
    req.capset_version = version;

    // Response = VGpuCtrlHdr + `size` bytes of caps. Cap the copy to the
    // resp half of the request slot (2048 - hdr) so we never overrun it.
    if (size > 2048 - sizeof(VGpuCtrlHdr))
        size = 2048 - sizeof(VGpuCtrlHdr);

    struct { VGpuCtrlHdr hdr; uint8_t caps[2048 - sizeof(VGpuCtrlHdr)]; } resp;
    bzero(&resp, sizeof(resp));
    if (!sendCommand(&req, sizeof(req), &resp, sizeof(VGpuCtrlHdr) + size))
        return false;
    if (resp.hdr.type != VIRTIO_GPU_RESP_OK_CAPSET)
        return false;

    memcpy(out, resp.caps, size);
    return true;
}

void
IOVirtIOGPU::gpuProbeVirgl()
{
    // Only meaningful if the device offered VIRGL and we acked it (see
    // start()). Walk capset indices 0.. and prefer VIRGL2 over VIRGL1.
    uint32_t id = 0, ver = 0, size = 0;
    bool found = false;
    volatile uint8_t *cfg = fTransport.deviceConfig();
    uint32_t ncaps = cfg ? *(volatile uint32_t *)(cfg + kGpuCfgNumCapsets) : 8;
    if (ncaps > kMaxCapsets)
        ncaps = kMaxCapsets;
    fCapsetCount = 0;
    for (uint32_t idx = 0; idx < ncaps; idx++) {
        uint32_t cid, cver, csize;
        if (!gpuGetCapsetInfo(idx, &cid, &cver, &csize))
            break; // no more capsets
        DEBUG("capset[%u]: id=%u max_version=%u max_size=%u\n",
              idx, cid, cver, csize);
        fCapsets[fCapsetCount].id = cid;
        fCapsets[fCapsetCount].version = cver;
        fCapsets[fCapsetCount].size = csize;
        fCapsetCount++;
        if (cid == VIRTIO_GPU_CAPSET_VIRGL2 ||
            (cid == VIRTIO_GPU_CAPSET_VIRGL && (!found || id != VIRTIO_GPU_CAPSET_VIRGL2))) {
            id = cid; ver = cver; size = csize;
            found = true;
        }
    }

    if (!found) {
        DEBUG("virgl: device offered no VIRGL capset\n");
        return;
    }

    // Pull the caps blob to prove the full request/response 3D path works
    // (and cache it for the user client's GetCaps).
    uint8_t caps[1536];
    uint32_t want = size < sizeof(caps) ? size : sizeof(caps);
    if (!gpuGetCapset(id, ver, caps, want)) {
        DEBUG("virgl: GET_CAPSET id=%u ver=%u failed\n", id, ver);
        return;
    }

    fVirglOK            = true;
    fVirglCapsetId      = id;
    fVirglCapsetVersion = ver;
    fVirglCapsetSize    = size;
    fVirglCapsLen       = want;
    memcpy(fVirglCaps, caps, want);
    DEBUG("virgl: 3D control path OK - capset id=%u version=%u size=%u "
          "(first caps word=0x%08x)\n", id, ver, size,
          *(const uint32_t *)caps);
}

void
IOVirtIOGPU::probeVenus()
{
    const uint32_t need = VIRTIO_GPU_F_RESOURCE_BLOB | VIRTIO_GPU_F_CONTEXT_INIT;
    bool capset = false;

    for (uint32_t i = 0; i < fCapsetCount; i++)
        if (fCapsets[i].id == VIRTIO_GPU_CAPSET_VENUS)
            capset = true;
    if ((fFeatures & need) != need || !capset) {
        DEBUG("venus: unavailable (features=0x%x venus capset=%d)\n", fFeatures, capset);
        return;
    }
    if (!fTransport.sharedMemoryRegion(VIRTIO_GPU_SHM_ID_HOST_VISIBLE, &fHostmemPhys, &fHostmemSize) ||
        !fHostmemSize) {
        DEBUG("venus: no host visible memory window (hostmem=0?)\n");
        return;
    }

    fHostRanges = (HostRange *)IOMalloc(sizeof(HostRange) * kMaxHostRanges);
    fBlobs = (Blob *)IOMalloc(sizeof(Blob) * kMaxBlobs);
    fBlobExports = (BlobExport *)IOMalloc(sizeof(BlobExport) * kMaxBlobExports);
    fBlobLock = IOLockAlloc();
    if (!fHostRanges || !fBlobs || !fBlobExports || !fBlobLock)
        return;
    fBlobCount = 0;
    bzero(fBlobExports, sizeof(BlobExport) * kMaxBlobExports);
    fHostRangeCount = 0;
    fVenusOK = true;
    IOLog("IOVirtIOGPU: venus ready, host visible window 0x%llx + 0x%llx\n", fHostmemPhys, fHostmemSize);
}

bool
IOVirtIOGPU::gpu3DCreateContext(uint32_t ctxId, const char *name)
{
    VGpuCtxCreate req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    req.hdr.ctx_id = ctxId;
    size_t nlen = 0;
    if (name) {
        while (name[nlen] && nlen < sizeof(req.debug_name) - 1) {
            req.debug_name[nlen] = name[nlen];
            nlen++;
        }
    }
    req.nlen = (uint32_t)nlen;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DDestroyContext(uint32_t ctxId)
{
    VGpuCtrlHdr req; bzero(&req, sizeof(req));
    req.type = VIRTIO_GPU_CMD_CTX_DESTROY;
    req.ctx_id = ctxId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DCreateResource(uint32_t resId, uint32_t target, uint32_t format,
                                 uint32_t bind, uint32_t width, uint32_t height)
{
    VGpuResourceCreate3D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_3D;
    req.resource_id = resId;
    req.target = target;
    req.format = format;
    req.bind = bind;
    req.width = width;
    req.height = height;
    req.depth = 1;
    req.array_size = 1;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    bool sent = sendCommand(&req, sizeof(req), &resp, sizeof(resp));
    DEBUG("create3d: res=%u target=%u fmt=%u bind=%u %ux%u -> sent=%d resp=0x%x\n",
          resId, target, format, bind, width, height, sent, resp.type);
    return sent && resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DCtxAttachResource(uint32_t ctxId, uint32_t resId)
{
    VGpuCtxResource req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_CTX_ATTACH_RESOURCE;
    req.hdr.ctx_id = ctxId;
    req.resource_id = resId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DTransfer(bool toHost, uint32_t ctxId, uint32_t resId,
                           uint32_t x, uint32_t y, uint32_t z,
                           uint32_t w, uint32_t h, uint32_t d,
                           uint32_t level, uint32_t stride, uint64_t offset,
                           uint64_t fenceId)
{
    VGpuTransferHost3D req; bzero(&req, sizeof(req));
    req.hdr.type = toHost ? VIRTIO_GPU_CMD_TRANSFER_TO_HOST_3D
                          : VIRTIO_GPU_CMD_TRANSFER_FROM_HOST_3D;
    req.hdr.ctx_id = ctxId;
    if (fenceId) {
        req.hdr.flags |= VIRTIO_GPU_FLAG_FENCE;
        req.hdr.fence_id = fenceId;
    }
    req.box.x = x; req.box.y = y; req.box.z = z;
    req.box.w = w; req.box.h = h; req.box.d = d;
    req.offset = offset;
    req.resource_id = resId;
    req.level = level;
    req.stride = stride;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DTransferToHost(uint32_t ctxId, uint32_t resId, uint32_t width,
                                 uint32_t height, uint32_t stride, uint64_t fenceId)
{
    return gpu3DTransfer(true, ctxId, resId, 0, 0, 0, width, height, 1,
                         0, stride, 0, fenceId);
}

bool
IOVirtIOGPU::gpuResourceUnref(uint32_t resId)
{
    VGpuCtxResource req; bzero(&req, sizeof(req)); // {hdr, resource_id, padding}
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    req.resource_id = resId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

// virgl submits are synchronous: the response of a fenced command is written back only when the
// host retires the fence, so the wait is the fence wait
bool
IOVirtIOGPU::gpu3DSubmit(uint32_t ctxId, IOBufferMemoryDescriptor *cmd, uint32_t cmdLen,
                         uint64_t fenceId)
{
    if (!cmd || cmdLen == 0)
        return false;

    VGpuCmdSubmit3D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    req.hdr.ctx_id = ctxId;
    if (fenceId) {
        req.hdr.flags |= VIRTIO_GPU_FLAG_FENCE;
        req.hdr.fence_id = fenceId;
    }
    req.size = cmdLen;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    bool ok = ctrlSubmit(&req, sizeof(req), cmd, cmdLen, &resp, sizeof(resp), false, 1000);
    if (!ok)
        DEBUG("gpu3DSubmit: timed out\n");
    return ok && resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DCreateContextInit(uint32_t ctxId, const char *name, uint32_t contextInit)
{
    VGpuCtxCreate req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_CTX_CREATE;
    req.hdr.ctx_id = ctxId;
    size_t nlen = 0;
    while (name && name[nlen] && nlen < sizeof(req.debug_name) - 1) {
        req.debug_name[nlen] = name[nlen];
        nlen++;
    }
    req.nlen = (uint32_t)nlen;
    req.context_init = contextInit;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    bool ok = sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
              resp.type == VIRTIO_GPU_RESP_OK_NODATA;
    DEBUG("ctx create %u init=0x%x -> resp=0x%x\n", ctxId, contextInit, resp.type);
    return ok;
}

bool
IOVirtIOGPU::gpu3DCtxDetachResource(uint32_t ctxId, uint32_t resId)
{
    VGpuCtxResource req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_CTX_DETACH_RESOURCE;
    req.hdr.ctx_id = ctxId;
    req.resource_id = resId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuCreateBlob(uint32_t ctxId, uint32_t resId, uint32_t blobMem, uint32_t blobFlags,
                           uint64_t blobId, uint64_t size)
{
    VGpuResourceCreateBlob req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB;
    req.hdr.ctx_id = ctxId;
    req.resource_id = resId;
    req.blob_mem = blobMem;
    req.blob_flags = blobFlags;
    req.blob_id = blobId;
    req.size = size;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    bool ok = sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
              resp.type == VIRTIO_GPU_RESP_OK_NODATA;
    DEBUG("blob create res=%u ctx=%u mem=%u flags=0x%x id=%llu size=%llu -> resp=0x%x\n",
          resId, ctxId, blobMem, blobFlags, blobId, size, resp.type);
    return ok;
}

bool
IOVirtIOGPU::gpuMapBlob(uint32_t resId, uint64_t offset, uint32_t *outMapInfo)
{
    VGpuResourceMapBlob req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_MAP_BLOB;
    req.resource_id = resId;
    req.offset = offset;

    VGpuRespMapInfo resp; bzero(&resp, sizeof(resp));
    bool ok = sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
              resp.hdr.type == VIRTIO_GPU_RESP_OK_MAP_INFO;
    DEBUG("blob map res=%u offset=0x%llx -> resp=0x%x info=0x%x\n",
          resId, offset, resp.hdr.type, resp.map_info);
    if (ok && outMapInfo)
        *outMapInfo = resp.map_info;
    return ok;
}

bool
IOVirtIOGPU::gpuUnmapBlob(uint32_t resId)
{
    VGpuResourceUnmapBlob req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNMAP_BLOB;
    req.resource_id = resId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuSubmitRing(uint32_t ctxId, bool useRing, uint32_t ring,
                           IOBufferMemoryDescriptor *cmd, uint32_t cmdLen, uint64_t fenceId)
{
    VGpuCmdSubmit3D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    req.hdr.ctx_id = ctxId;
    if (useRing) {
        req.hdr.flags |= VIRTIO_GPU_FLAG_INFO_RING_IDX;
        req.hdr.ring_idx = (uint8_t)ring;
    }
    if (fenceId) {
        req.hdr.flags |= VIRTIO_GPU_FLAG_FENCE;
        req.hdr.fence_id = fenceId;
    }
    req.size = cmd ? cmdLen : 0;

    if (fenceId)
        return ctrlSubmit(&req, sizeof(req), cmd, req.size, NULL, 0, true, 5000);

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return ctrlSubmit(&req, sizeof(req), cmd, req.size, &resp, sizeof(resp), false, 5000) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

// caller holds fQueueLock
bool
IOVirtIOGPU::fenceBusyLocked(uint64_t fenceId)
{
    for (unsigned i = 0; i < fSlotCount; i++) {
        if (fSlots[i].state == kSlotBusy && fSlots[i].async && fSlots[i].fenceId == fenceId)
            return true;
    }
    return false;
}

bool
IOVirtIOGPU::fenceRetired(uint64_t fenceId)
{
    bool retired;

    IOLockLock(fQueueLock);
    ctrlReapLocked();
    retired = !fenceBusyLocked(fenceId);
    IOLockUnlock(fQueueLock);
    return retired;
}

bool
IOVirtIOGPU::waitFence(uint64_t fenceId, uint64_t timeoutUs)
{
    uint64_t spins = 0;
    bool retired;

    IOLockLock(fQueueLock);
    for (;;) {
        ctrlReapLocked();
        retired = !fenceBusyLocked(fenceId);
        if (retired || spins >= timeoutUs)
            break;
        ctrlWaitLocked(&spins, timeoutUs);
    }
    IOLockUnlock(fQueueLock);
    return retired;
}

// block until some request completes after generation seenGen, for clients that poll fences
uint64_t
IOVirtIOGPU::waitProgress(uint64_t seenGen, uint64_t timeoutUs)
{
    uint64_t spins = 0;
    uint64_t gen;

    IOLockLock(fQueueLock);
    for (;;) {
        ctrlReapLocked();
        gen = fCompletionGen;
        if (gen != seenGen || spins >= timeoutUs)
            break;
        ctrlWaitLocked(&spins, timeoutUs);
    }
    IOLockUnlock(fQueueLock);
    return gen;
}

// an msi-x vector when the platform offers one (nub sources after intx at 0), else intx. its own work loop,
// as the framebuffer's gate may be held by a thread waiting on a completion
bool
IOVirtIOGPU::setupInterrupt()
{
    int type = 0, source = -1;

    for (int i = 1; i < 8 && fPCIDevice->getInterruptType(i, &type) == kIOReturnSuccess; i++) {
        if (type & kIOInterruptTypePCIMessaged) {
            source = i;
            break;
        }
    }
    if (source < 0 && fPCIDevice->getInterruptType(0, &type) == kIOReturnSuccess)
        source = 0;
    if (source < 0)
        return false;

    fIrqLoop = IOWorkLoop::workLoop();
    if (!fIrqLoop)
        return false;
    fIrqSource = IOInterruptEventSource::interruptEventSource(this,
        OSMemberFunctionCast(IOInterruptEventAction, this, &IOVirtIOGPU::interruptOccurred), fPCIDevice, source);
    if (!fIrqSource || fIrqLoop->addEventSource(fIrqSource) != kIOReturnSuccess) {
        OSSafeReleaseNULL(fIrqSource);
        return false;
    }
    // enabling a messaged source turns msi-x on, after which the device needs its vectors
    fIrqSource->enable();
    fIrqMsix = source > 0;
    fIrqSourceIndex = source;
    if (fIrqMsix && !fTransport.setMsixVectors(&fControlQ, (uint16_t)(source - 1), IOVirtIOTransport::kNoVector)) {
        fIrqSource->disable();
        fIrqLoop->removeEventSource(fIrqSource);
        OSSafeReleaseNULL(fIrqSource);
        return false;
    }
    fUseIrq = true;
    IOLog("IOVirtIOGPU: completions on %s\n", fIrqMsix ? "msi-x" : "intx");
    return true;
}

// runs on fIrqLoop, a level triggered line stays masked until this returns
void
IOVirtIOGPU::interruptOccurred(IOInterruptEventSource *, int)
{
    fIrqCount++;
    // with intx reading the isr acknowledges it and drops the line, an msi-x vector is ours alone
    if (!fIrqMsix && !(fTransport.readIsr() & 1))
        return;
    if (!fQueueLock)
        return;
    IOLockLock(fQueueLock);
    ctrlReapLocked();
    IOLockUnlock(fQueueLock);
}

bool
IOVirtIOGPU::getCapset(uint32_t capsetId, uint32_t version, void *out, uint32_t *ioSize)
{
    for (uint32_t i = 0; i < fCapsetCount; i++) {
        if (fCapsets[i].id != capsetId)
            continue;
        if (version > fCapsets[i].version)
            return false;
        uint32_t size = fCapsets[i].size;
        if (size > *ioSize)
            size = *ioSize;
        if (!gpuGetCapset(capsetId, version, out, size))
            return false;
        *ioSize = size;
        return true;
    }
    return false;
}

uint32_t
IOVirtIOGPU::capsetMask() const
{
    uint32_t mask = 0;
    for (uint32_t i = 0; i < fCapsetCount; i++)
        if (fCapsets[i].id < 32)
            mask |= 1u << fCapsets[i].id;
    return mask;
}

// first fit over the host visible window, page aligned so each blob maps into tasks on its own pages
bool
IOVirtIOGPU::hostmemAlloc(uint32_t resId, uint64_t size, uint64_t *outOffset)
{
    if (!fHostRanges || !size)
        return false;
    size = (size + PAGE_MASK) & ~(uint64_t)PAGE_MASK;

    IOLockLock(fCtrlLock);
    bool ok = false;
    if (fHostRangeCount < kMaxHostRanges) {
        uint64_t at = 0;
        uint32_t i = 0;
        for (; i < fHostRangeCount; i++) {
            if (fHostRanges[i].offset - at >= size)
                break;
            at = fHostRanges[i].offset + fHostRanges[i].size;
        }
        if (at + size <= fHostmemSize) {
            memmove(&fHostRanges[i + 1], &fHostRanges[i], (fHostRangeCount - i) * sizeof(HostRange));
            fHostRanges[i].offset = at;
            fHostRanges[i].size = size;
            fHostRanges[i].resId = resId;
            fHostRangeCount++;
            *outOffset = at;
            ok = true;
        }
    }
    IOLockUnlock(fCtrlLock);
    return ok;
}

void
IOVirtIOGPU::hostmemFree(uint32_t resId)
{
    IOLockLock(fCtrlLock);
    for (uint32_t i = 0; i < fHostRangeCount; i++) {
        if (fHostRanges[i].resId != resId)
            continue;
        memmove(&fHostRanges[i], &fHostRanges[i + 1], (fHostRangeCount - i - 1) * sizeof(HostRange));
        fHostRangeCount--;
        break;
    }
    IOLockUnlock(fCtrlLock);
}

// where IOMemoryDescriptor keeps a descriptor's own cache mode (kIOMemoryBufferCacheShift, not in the kext headers)
enum { kDescriptorCacheShift = 28 };

// caller holds fBlobLock
IOVirtIOGPU::Blob *
IOVirtIOGPU::findBlobLocked(uint32_t resId)
{
    for (uint32_t i = 0; i < fBlobCount; i++)
        if (fBlobs[i].resId == resId)
            return &fBlobs[i];
    return NULL;
}

// host side of the last ref: unmap the blob from the window, then unref it
void
IOVirtIOGPU::blobDestroy(const Blob &b)
{
    if (b.window) {
        gpuUnmapBlob(b.resId);
        hostmemFree(b.resId);
        b.window->release();
    }
    gpuResourceUnref(b.resId);
}

bool
IOVirtIOGPU::blobAdd(uint32_t resId, uint32_t blobMem, uint32_t blobFlags, uint64_t size)
{
    bool ok = false;

    if (!fBlobLock)
        return false;
    IOLockLock(fBlobLock);
    if (fBlobCount < kMaxBlobs && !findBlobLocked(resId)) {
        Blob *b = &fBlobs[fBlobCount++];
        bzero(b, sizeof(*b));
        b->resId = resId;
        b->refs = 1;
        b->blobMem = blobMem;
        b->blobFlags = blobFlags;
        b->size = size;
        ok = true;
    }
    IOLockUnlock(fBlobLock);
    return ok;
}

void
IOVirtIOGPU::blobRelease(uint32_t resId)
{
    Blob dead;
    bool last = false;

    if (!fBlobLock)
        return;
    IOLockLock(fBlobLock);
    Blob *b = findBlobLocked(resId);
    if (b && --b->refs == 0) {
        dead = *b;
        *b = fBlobs[--fBlobCount];
        last = true;
    }
    IOLockUnlock(fBlobLock);
    if (last)
        blobDestroy(dead);
}

// reserve window space, have the host map the blob there, and describe that physical range so every
// task maps it with the cache mode the host asked for
bool
IOVirtIOGPU::blobMap(uint32_t resId, uint32_t *outMapInfo)
{
    bool ok = false;

    if (!fBlobLock)
        return false;
    IOLockLock(fBlobLock);
    Blob *b = findBlobLocked(resId);
    if (b && b->window)
        ok = true;
    else if (b) {
        uint64_t offset = 0;
        uint32_t info = 0;
        uint64_t len = (b->size + PAGE_MASK) & ~(uint64_t)PAGE_MASK;
        if (hostmemAlloc(resId, len, &offset)) {
            if (gpuMapBlob(resId, offset, &info)) {
                // the cache mode goes into the descriptor itself: a physical range otherwise takes the default
                // for its address, which for a BAR is uncached device memory, whatever the mapping asks for
                IOOptionBits cache;
                switch (info & PD_VENUS_MAP_CACHE_MASK) {
                case PD_VENUS_MAP_CACHE_CACHED: cache = kIOCopybackCache; break;
                case PD_VENUS_MAP_CACHE_WC:     cache = kIOWriteCombineCache; break;
                default:                        cache = kIOInhibitCache; break;
                }
                b->window = IOMemoryDescriptor::withAddressRange(fHostmemPhys + offset, len,
                                                                 kIODirectionInOut | (cache << kDescriptorCacheShift),
                                                                 TASK_NULL);
                if (b->window) {
                    b->mapInfo = info;
                    ok = true;
                } else
                    gpuUnmapBlob(resId);
            }
            if (!ok)
                hostmemFree(resId);
        }
    }
    if (ok && outMapInfo)
        *outMapInfo = b->mapInfo;
    IOLockUnlock(fBlobLock);
    return ok;
}

IOMemoryDescriptor *
IOVirtIOGPU::blobWindow(uint32_t resId, uint32_t *outMapInfo)
{
    IOMemoryDescriptor *w = NULL;

    if (!fBlobLock)
        return NULL;
    IOLockLock(fBlobLock);
    Blob *b = findBlobLocked(resId);
    if (b && b->window) {
        w = b->window;
        w->retain();
        if (outMapInfo)
            *outMapInfo = b->mapInfo;
    }
    IOLockUnlock(fBlobLock);
    return w;
}

// one token per blob and exporting connection, so exporting again gives the same token
bool
IOVirtIOGPU::blobExport(uint32_t resId, const void *owner, uint64_t *outToken)
{
    BlobExport *slot = NULL;
    bool ok = false;

    if (!fBlobLock)
        return false;
    IOLockLock(fBlobLock);
    Blob *b = findBlobLocked(resId);
    for (uint32_t i = 0; b && i < kMaxBlobExports; i++) {
        BlobExport *e = &fBlobExports[i];
        if (e->token && e->resId == resId && e->owner == owner) {
            *outToken = e->token;
            ok = true;
            break;
        }
        if (!slot && !e->token)
            slot = e;
    }
    if (b && !ok && slot) {
        do {
            read_random(&slot->token, sizeof(slot->token));
        } while (!slot->token);
        slot->resId = resId;
        slot->owner = owner;
        b->refs++;
        *outToken = slot->token;
        ok = true;
    }
    IOLockUnlock(fBlobLock);
    return ok;
}

bool
IOVirtIOGPU::blobImport(uint64_t token, uint32_t *outResId, uint32_t *outBlobMem, uint32_t *outBlobFlags,
                        uint64_t *outSize)
{
    bool ok = false;

    if (!fBlobLock || !token)
        return false;
    IOLockLock(fBlobLock);
    for (uint32_t i = 0; i < kMaxBlobExports; i++) {
        if (fBlobExports[i].token != token)
            continue;
        Blob *b = findBlobLocked(fBlobExports[i].resId);
        if (b) {
            b->refs++;
            *outResId = b->resId;
            *outBlobMem = b->blobMem;
            *outBlobFlags = b->blobFlags;
            *outSize = b->size;
            ok = true;
        }
        break;
    }
    IOLockUnlock(fBlobLock);
    return ok;
}

void
IOVirtIOGPU::blobDropExports(const void *owner)
{
    if (!fBlobLock)
        return;
    for (;;) {
        uint32_t resId = 0;

        IOLockLock(fBlobLock);
        for (uint32_t i = 0; i < kMaxBlobExports; i++) {
            if (fBlobExports[i].token && fBlobExports[i].owner == owner) {
                resId = fBlobExports[i].resId;
                bzero(&fBlobExports[i], sizeof(fBlobExports[i]));
                break;
            }
        }
        IOLockUnlock(fBlobLock);
        if (!resId)
            return;
        blobRelease(resId);
    }
}

uint32_t
IOVirtIOGPU::allocContextId()
{
    IOLockLock(fCtrlLock);
    uint32_t id = fNextCtxId++;
    IOLockUnlock(fCtrlLock);
    return id;
}

uint32_t
IOVirtIOGPU::allocResourceId()
{
    IOLockLock(fCtrlLock);
    uint32_t id = fNextResId++;
    IOLockUnlock(fCtrlLock);
    return id;
}

uint64_t
IOVirtIOGPU::allocFenceId()
{
    IOLockLock(fCtrlLock);
    uint64_t id = fNextFenceId++;
    IOLockUnlock(fCtrlLock);
    return id;
}

void
IOVirtIOGPU::checkDisplayEvents()
{
    volatile uint8_t *cfg = fTransport.deviceConfig();
    if (!cfg)
        return;

    uint32_t events = *(volatile uint32_t *)(cfg + kGpuCfgEventsRead);
    if (!(events & VIRTIO_GPU_EVENT_DISPLAY))
        return;

    // Clear first: a reconfiguration that happens while we are re-reading the
    // modes must leave the bit set for the next tick rather than be swallowed.
    *(volatile uint32_t *)(cfg + kGpuCfgEventsClear) = VIRTIO_GPU_EVENT_DISPLAY;
    OSSynchronizeIO();

    uint32_t width = 0, height = 0;
    if (!gpuGetDisplayInfo(&width, &height)) {
        DEBUG("display event, but no scanout is enabled yet\n");
        return;
    }

    if (width > fScanoutWidth)   width = fScanoutWidth;
    if (height > fScanoutHeight) height = fScanoutHeight;
    if (!width || !height)
        return;

    DEBUG("display event: re-pointing scanout 0 at resource %u (%ux%u)\n",
          fScanoutResourceId, width, height);

    if (!gpuSetScanout(0, fScanoutResourceId, width, height)) {
        DEBUG("display event: SET_SCANOUT failed\n");
        return;
    }

    if (fScanoutResourceId == fResourceId && fFbBase && !fNativePresent)
        gpuTransferToHost2D(fResourceId, 0, 0, width, height);
    gpuResourceFlush(fScanoutResourceId, 0, 0, width, height);
}

void
IOVirtIOGPU::flushCallback(thread_call_param_t self, thread_call_param_t)
{
    ((IOVirtIOGPU *)self)->scheduleFlush();
}

void
IOVirtIOGPU::scheduleFlush()
{
    checkDisplayEvents();

    IOLockLock(fCtrlLock);
    bool pending = fPresentPending;
    uint32_t x1 = fPresentX1, y1 = fPresentY1;
    uint32_t x2 = fPresentX2, y2 = fPresentY2;
    fPresentPending = false;

    bool nativePresent = fNativePresent;
    IOLockUnlock(fCtrlLock);

    // Only the driver's own framebuffer needs its pixels pushed to the host;
    // a client resource the scanout has been pointed at already lives there.
    if (fScanoutResourceId != fResourceId) {
        gpuResourceFlush(fScanoutResourceId, 0, 0, fWidth, fHeight);
    } else if (fFbBase && pending) {
        gpuTransferToHost2D(fResourceId, x1, y1, x2 - x1, y2 - y1);
        gpuResourceFlush(fResourceId, x1, y1, x2 - x1, y2 - y1);
    } else if (fFbBase && !nativePresent) {
        gpuTransferToHost2D(fResourceId, 0, 0, fWidth, fHeight);
        gpuResourceFlush(fResourceId, 0, 0, fWidth, fHeight);
    }

    AbsoluteTime deadline;
    clock_interval_to_deadline(kFlushIntervalMs, kMillisecondScale, &deadline);
    thread_call_enter_delayed(fFlushCall, deadline);
}

bool
IOVirtIOGPU::start(IOService *provider)
{
    DEBUG("start provider=%p\n", provider);

    /* IOFramebuffer::start() creates its IOFBController through the
     * dependent-ID path. VirtIO has no firmware framebuffer dependency, so
     * provide a stable ID based on the PCI provider before entering the base
     * class. Without this, inherited user-client calls can run with a
     * missing controller and fault while Xorg queries the display mode. */
    if (!getProperty(kIOFBDependentIDKey)) {
        uint64_t dependentID = provider ? provider->getRegistryEntryID() : 0;
        OSNumber *number = OSNumber::withNumber(dependentID, 64);
        if (number) {
            setProperty(kIOFBDependentIDKey, number);
            number->release();
        } else {
            DEBUG("failed to allocate framebuffer dependent ID\n");
        }
    }

    if (!super::start(provider)) {
        DEBUG("super::start failed\n");
        return false;
    }

    // Must exist before the first sendCommand (control-queue serialization)
    // and before any user client allocates ids.
    fCtrlLock = IOLockAlloc();
    fQueueLock = IOLockAlloc();
    if (!fCtrlLock || !fQueueLock)
        return false;
    fNextCtxId   = 1;
    fNextResId   = 0x100;  // above the 2D scanout resource (id 1)
    fNextFenceId = 1;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        DEBUG("failed to open PCI device\n");
        return false;
    }

    if (!fTransport.attach(fPCIDevice)) {
        DEBUG("failed to find/map virtio-pci capabilities\n");
        return false;
    }

    // Ack VIRGL only when the device actually offers it - negotiateFeatures
    // writes driver bits verbatim, so requesting an unoffered bit makes the
    // device clear FEATURES_OK. Plain virtio-gpu-pci offers no VIRGL and
    // still gets a clean 2D bring-up with driver features 0.
    uint32_t devFeat = fTransport.deviceFeaturesLow();
    uint32_t drvFeat = devFeat & (VIRTIO_GPU_F_VIRGL | VIRTIO_GPU_F_RESOURCE_BLOB |
                                  VIRTIO_GPU_F_CONTEXT_INIT);
    if (!fTransport.negotiateFeatures(drvFeat)) {
        DEBUG("device rejected feature negotiation\n");
        return false;
    }
    DEBUG("features: device=0x%08x acked=0x%08x (virgl %s)\n",
          devFeat, drvFeat, drvFeat ? "on" : "off");

    fFeatures = drvFeat;
    if (!fTransport.initQueue(&fControlQ, /*queue 0 = controlq*/ 0, 256)) {
        DEBUG("failed to init control virtqueue\n");
        return false;
    }
    fTransport.enableFreeList(&fControlQ);

    // The cursor queue is optional: without it the compositor keeps drawing
    // the pointer into the framebuffer itself, which still works.
    fCursorQOK = fTransport.initQueue(&fCursorQ, /*queue 1 = cursorq*/ 1, 16);
    if (!fCursorQOK)
        DEBUG("no cursor virtqueue; falling back to a software cursor\n");

    // a request is at most three descriptors (header, payload, response)
    fSlotCount = fControlQ.queueSize / 3;
    if (fSlotCount > kCtrlSlots)
        fSlotCount = kCtrlSlots;
    for (unsigned i = 0; i < fSlotCount; i++) {
        CtrlSlot *s = &fSlots[i];
        s->mem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
            kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
            kCtrlSlotBytes, 0xFFFFFFFFULL);
        if (!s->mem) {
            DEBUG("failed to allocate control request slots\n");
            return false;
        }
        s->virt = (uint8_t *)s->mem->getBytesNoCopy();
        s->phys = s->mem->getPhysicalAddress();
        bzero(s->virt, kCtrlSlotBytes);
        s->state = kSlotFree;
    }

    if (!setupInterrupt())
        IOLog("IOVirtIOGPU: no interrupt, polling for completions\n");

    fTransport.setDriverOk();

    fWidth = kDefaultWidth;
    fHeight = kDefaultHeight;
    fNativePresent = false;
    fPresentPending = false;
    fConsolePaused = false;
    fPresentX1 = fPresentY1 = fPresentX2 = fPresentY2 = 0;
    gpuGetDisplayInfo(&fWidth, &fHeight); // best-effort, keep defaults on failure
    fPitch = fWidth * 4;
    fResourceId = 1;
    fScanoutResourceId = fResourceId;
    fScanoutWidth = fWidth;
    fScanoutHeight = fHeight;

    size_t fbSize = (size_t)fPitch * fHeight;
    fFbMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        fbSize, 0xFFFFFFFFULL);
    if (!fFbMem) {
        DEBUG("failed to allocate %ux%u framebuffer\n", fWidth, fHeight);
        return false;
    }
    fFbBase = fFbMem->getBytesNoCopy();
    fFbPhys = fFbMem->getPhysicalAddress();
    bzero(fFbBase, fbSize);

    if (!gpuCreateResource2D(fResourceId, fWidth, fHeight) ||
        !gpuAttachBacking(fResourceId, fFbPhys, (uint32_t)fbSize) ||
        !gpuSetScanout(0, fResourceId, fWidth, fHeight)) {
        DEBUG("failed to set up 2D scanout resource\n");
        return false;
    }
    gpuTransferToHost2D(fResourceId, 0, 0, fWidth, fHeight);
    gpuResourceFlush(fResourceId, 0, 0, fWidth, fHeight);

    DEBUG("virtio-gpu scanout ready: %ux%u pitch=%u fb=%p (phys 0x%llx)\n",
          fWidth, fHeight, fPitch, fFbBase, fFbPhys);

    if (fCursorQOK && !gpuSetupCursorResource())
        fCursorQOK = false; // compositor keeps its software cursor

    if (drvFeat & VIRTIO_GPU_F_VIRGL) {
        gpuProbeVirgl();
    }
    probeVenus();

    IOPlatformExpert *pe = getPlatform();
#if !defined(__arm64__) && !defined(__aarch64__)
    if (pe) {
        PE_Video consoleInfo;
        consoleInfo.v_baseAddr = (unsigned long)fFbBase;
        consoleInfo.v_width = fWidth;
        consoleInfo.v_height = fHeight;
        consoleInfo.v_depth = 32;
        consoleInfo.v_rowBytes = fPitch;
        consoleInfo.v_display = kPEGraphicsMode;
        consoleInfo.v_offset = 0;
        consoleInfo.v_length = 0;
        consoleInfo.v_rotate = 0;
        consoleInfo.v_scale = kPEScaleFactor1x;

        IOReturn ret = pe->setConsoleInfo(&consoleInfo, kPEGraphicsMode);
        if (ret != kIOReturnSuccess) {
            DEBUG("setConsoleInfo failed: %d\n", ret);
        } else {
            DEBUG("kernel graphics console initialized on virtio-gpu\n");
            boolean_t useGopConsole = false;
            if (PE_parse_boot_argn("gopconsole", &useGopConsole, sizeof(useGopConsole)) && useGopConsole) {
                int oldConsole = switch_to_video_console();
                pe->setConsoleInfo(&consoleInfo, kPEAcquireScreen);
                pe->setConsoleInfo(&consoleInfo, kPETextScreen);
                DEBUG("active console switched to virtio-gpu video, old console=%d\n", oldConsole);

                vc_progress_set(FALSE, 0);
                vc_progress_set(TRUE, 0);
                DEBUG("gopprogress: overlaid kernel progress meter on text console\n");
            }
        }
    }
#else
    (void)pe;
#endif

    fFlushCall = thread_call_allocate(flushCallback, this);
    if (fFlushCall)
        scheduleFlush();

    registerService();
    return true;
}

void
IOVirtIOGPU::stop(IOService *provider)
{
    DEBUG("stop %p\n", provider);

    // Leaving the console parked would take the machine's last output path
    // with the driver.
    setConsoleDrawing(true);

    if (fFlushCall) {
        thread_call_cancel(fFlushCall);
        thread_call_free(fFlushCall);
        fFlushCall = NULL;
    }
    if (fIrqSource) {
        fIrqSource->disable();
        if (fIrqLoop)
            fIrqLoop->removeEventSource(fIrqSource);
        OSSafeReleaseNULL(fIrqSource);
    }
    OSSafeReleaseNULL(fIrqLoop);
    fUseIrq = false;
    if (fFbMem) { fFbMem->release(); fFbMem = NULL; }
    for (unsigned i = 0; i < fSlotCount; i++) {
        if (fSlots[i].payload) { fSlots[i].payload->release(); fSlots[i].payload = NULL; }
        if (fSlots[i].mem) { fSlots[i].mem->release(); fSlots[i].mem = NULL; }
    }
    fSlotCount = 0;
    // clients are gone by now, so whatever is left is only exports nobody dropped
    for (uint32_t i = 0; fBlobs && i < fBlobCount; i++)
        blobDestroy(fBlobs[i]);
    fBlobCount = 0;
    if (fBlobs) { IOFree(fBlobs, sizeof(Blob) * kMaxBlobs); fBlobs = NULL; }
    if (fBlobExports) { IOFree(fBlobExports, sizeof(BlobExport) * kMaxBlobExports); fBlobExports = NULL; }
    if (fBlobLock) { IOLockFree(fBlobLock); fBlobLock = NULL; }
    if (fHostRanges) {
        IOFree(fHostRanges, sizeof(HostRange) * kMaxHostRanges);
        fHostRanges = NULL;
    }
    if (fCursorMem) { fCursorMem->release(); fCursorMem = NULL; }
    fCursorVirt = NULL;
    fCursorImageBase = NULL;
    if (fCursorQOK)
        fTransport.freeQueue(&fCursorQ);
    fTransport.freeQueue(&fControlQ);
    fTransport.detach();
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }
    if (fCtrlLock) { IOLockFree(fCtrlLock); fCtrlLock = NULL; }
    if (fQueueLock) { IOLockFree(fQueueLock); fQueueLock = NULL; }
    if (fCursorLock) { IOLockFree(fCursorLock); fCursorLock = NULL; }

    super::stop(provider);
}

IOReturn
IOVirtIOGPU::newUserClient(task_t owningTask, void *securityID, UInt32 type,
                           IOUserClient **handler)
{
    /* Type 0 is the normal framebuffer connection. The inherited
     * IOFramebufferUserClient assumes a firmware-style controller and faults
     * during IOServiceOpen for this PCI-backed framebuffer. */
    if (type == 0) {
        IOVirtIOGPUUserClient *uc = IOVirtIOGPUUserClient::withOwner(
            this, owningTask, true);
        if (!uc)
            return kIOReturnNoMemory;
        if (!uc->attach(this) || !uc->start(this)) {
            uc->detach(this);
            uc->release();
            return kIOReturnError;
        }
        *handler = uc;
        return kIOReturnSuccess;
    }

    // The PDSurface protocol does not depend on virgl, so it is offered
    // whether or not 3D came up.
    if (type == kPDSurfaceConnectType) {
        IOVirtIOGPUSurfaceClient *sc =
            IOVirtIOGPUSurfaceClient::withOwner(this, owningTask);
        if (!sc)
            return kIOReturnNoMemory;
        if (!sc->attach(this) || !sc->start(this)) {
            sc->detach(this);
            sc->release();
            return kIOReturnError;
        }
        *handler = sc;
        return kIOReturnSuccess;
    }

    if (type == kIOVirtIOGPUVenusConnectType) {
        if (!fVenusOK)
            return kIOReturnUnsupported;
        IOVirtIOGPUVenusClient *vc = IOVirtIOGPUVenusClient::withOwner(this, owningTask);
        if (!vc)
            return kIOReturnNoMemory;
        if (!vc->attach(this) || !vc->start(this)) {
            vc->detach(this);
            vc->release();
            return kIOReturnError;
        }
        *handler = vc;
        return kIOReturnSuccess;
    }

    // Only intercept our 3D connect type, every other type belongs to
    // IOFramebuffer's own user-client machinery (the 2D scanout path).
    if (type != kIOVirtIOGPU3DConnectType)
        return super::newUserClient(owningTask, securityID, type, handler);

    if (!fVirglOK)
        return kIOReturnUnsupported;

    IOVirtIOGPUUserClient *uc = IOVirtIOGPUUserClient::withOwner(this, owningTask);
    if (!uc)
        return kIOReturnNoMemory;
    if (!uc->attach(this) || !uc->start(this)) {
        uc->detach(this);
        uc->release();
        return kIOReturnError;
    }
    *handler = uc;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPU::enableController()
{
    return kIOReturnSuccess;
}

const char *
IOVirtIOGPU::getPixelFormats()
{
    return IO32BitDirectPixels;
}

IOReturn
IOVirtIOGPU::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
    *displayMode = kDisplayModeID;
    *depth = kDepth;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPU::setDisplayMode(IODisplayModeID, IOIndex)
{
    return kIOReturnSuccess;
}

IODeviceMemory *
IOVirtIOGPU::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange((mach_vm_address_t)fFbPhys, (IOByteCount)fPitch * fHeight);
}

IODeviceMemory *
IOVirtIOGPU::getVRAMRange(void)
{
    return getApertureRange(kIOFBSystemAperture);
}

IOBufferMemoryDescriptor *
IOVirtIOGPU::copyFramebufferMemory()
{
    if (!fFbMem)
        return NULL;
    fFbMem->retain();
    return fFbMem;
}

IOReturn
IOVirtIOGPU::getInformationForDisplayMode(IODisplayModeID, IODisplayModeInformation *info)
{
    bzero(info, sizeof(*info));
    info->nominalWidth  = fWidth;
    info->nominalHeight = fHeight;
    info->refreshRate   = 30 << 16; // matches the poll-flush interval
    info->maxDepthIndex = kDepth;
    return kIOReturnSuccess;
}

UInt64
IOVirtIOGPU::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
    return (UInt64)(uintptr_t)getPixelFormats();
}

IOItemCount
IOVirtIOGPU::getDisplayModeCount(void)
{
    return 1;
}

IOReturn
IOVirtIOGPU::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    *allDisplayModes = kDisplayModeID;
    return kIOReturnSuccess;
}

IOReturn
IOVirtIOGPU::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                 IOPixelAperture aperture, IOPixelInformation *info)
{
    if (aperture || depth || (displayMode != kDisplayModeID))
        return kIOReturnUnsupportedMode;

    bzero(info, sizeof(*info));
    info->activeWidth   = fWidth;
    info->activeHeight  = fHeight;
    info->bytesPerRow   = fPitch;
    info->bytesPerPlane = 0;

    // B8G8R8A8 in memory == 0x00RRGGBB as a 32-bit value, matching
    // IOGOPFramebuffer's IO32BitDirectPixels layout.
    strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
    info->pixelType        = kIORGBDirectPixels;
    info->componentMasks[0] = 0x00ff0000;
    info->componentMasks[1] = 0x0000ff00;
    info->componentMasks[2] = 0x000000ff;
    info->bitsPerPixel     = 32;
    info->componentCount   = 3;
    info->bitsPerComponent = 8;

    return kIOReturnSuccess;
}
