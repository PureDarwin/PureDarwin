/*
 * IOVirtIOGPU: minimal virtio-gpu 2D scanout driver for QEMU's
 * -device virtio-gpu-pci. Uses IOVirtIOFamily's IOVirtIOTransport for
 * the virtio-1.0 PCI transport (capability walking, handshake, split
 * virtqueues, polling-only completion - no MSI-X/interrupts), and layers
 * the virtio-gpu 2D control protocol on top:
 *   GET_DISPLAY_INFO -> RESOURCE_CREATE_2D -> RESOURCE_ATTACH_BACKING ->
 *   SET_SCANOUT, then TRANSFER_TO_HOST_2D + RESOURCE_FLUSH on a timer.
 * Register/protocol layout from the public VIRTIO 1.1 spec (PCI
 * transport, section 4.1; virtio-gpu device, section 5.7) - no Apple
 * source exists for this device class.
 */

#include "IOVirtIOGPU.h"
#include "IOVirtIOGPU3DShared.h"
#include "IOVirtIOGPUUserClient.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <kern/thread_call.h>
#include <sys/errno.h>

extern "C" int switch_to_video_console(void);
extern "C" boolean_t PE_parse_boot_argn(const char *arg_string, void *arg_ptr, int max_arg);
extern "C" void vc_progress_set(boolean_t enable, uint32_t vc_delay);

#define super IOFramebuffer
OSDefineMetaClassAndStructors(IOVirtIOGPU, IOFramebuffer);

#define kDisplayModeID 1
#define kDepth         0
#define kDefaultWidth  1024
#define kDefaultHeight 768
#define kFlushIntervalMs 33   // ~30 Hz

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

    VIRTIO_GPU_RESP_OK_NODATA        = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO  = 0x1101,
    VIRTIO_GPU_RESP_OK_CAPSET_INFO   = 0x1102,
    VIRTIO_GPU_RESP_OK_CAPSET        = 0x1103,
};

// virtio-gpu device feature bits (low word). VIRGL = 3D/OpenGL support.
enum { VIRTIO_GPU_F_VIRGL = (1u << 0) };

// Control-header flag: request a fence for this command. In the split
// virtqueue the used-ring writeback for a fenced command lands only after
// the host retires the GPU work, so polling the completion (sendCommand
// already does) is the fence wait - no separate fence queue needed in v1.
enum { VIRTIO_GPU_FLAG_FENCE = (1u << 0) };

// Capset ids the host advertises. VIRGL2 is virglrenderer's modern GL
// capability set; VIRGL(1) is the legacy one. We probe VIRGL2 first.
enum {
    VIRTIO_GPU_CAPSET_VIRGL  = 1,
    VIRTIO_GPU_CAPSET_VIRGL2 = 2,
};

enum { VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM = 1 };

struct VGpuCtrlHdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
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
// The virgl2 capset is ~700 bytes; the resp half of the 4KB scratch is
// 2048 bytes, so it fits without growing fCmdMem.

// 3D/virgl command payloads (spec sec 5.7). Context-scoped commands carry
// the context id in hdr.ctx_id; a fenced command sets VIRTIO_GPU_FLAG_FENCE
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
    // transitional) is device id 0x1050; legacy/transitional is 0x1010 -
    // match both, QEMU's default is the modern id.
    if (vendor != 0x1af4)
        return NULL;
    if (device != 0x1050 && device != 0x1010)
        return NULL;

    if (score)
        *score = 6000; // outrank IOGOPFramebuffer's EFI GOP path when both match
    return this;
}

bool
IOVirtIOGPU::sendCommand(const void *cmd, size_t cmdLen, void *resp, size_t respLen)
{
    if (!fCmdVirt || cmdLen + respLen > 4096) {
        DEBUG("sendCommand: no scratch buffer or command too large\n");
        return false;
    }

    // Serialize the single control queue against the 2D flush timer and
    // concurrent user-client 3D commands.
    if (fCtrlLock) IOLockLock(fCtrlLock);

    uint8_t *reqBase  = (uint8_t *)fCmdVirt;
    uint8_t *respBase = reqBase + 2048; // fixed halves of the one page
    memcpy(reqBase, cmd, cmdLen);
    bzero(respBase, respLen);

    VirtIOChainEntry chain[2] = {
        { fCmdPhys,        (uint32_t)cmdLen,  false },
        { fCmdPhys + 2048, (uint32_t)respLen, true  },
    };
    fTransport.addDescChain(&fControlQ, chain, 2);
    fTransport.notify(&fControlQ);

    bool ok = fTransport.pollForCompletion(&fControlQ, 1000);
    if (ok)
        memcpy(resp, respBase, respLen);
    else
        DEBUG("sendCommand: timed out waiting for device\n");

    if (fCtrlLock) IOLockUnlock(fCtrlLock);
    return ok;
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
IOVirtIOGPU::gpuTransferToHost2D(uint32_t resourceId, uint32_t width, uint32_t height)
{
    VGpuTransferToHost2D req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    req.r.width = width;
    req.r.height = height;
    req.resource_id = resourceId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpuResourceFlush(uint32_t resourceId, uint32_t width, uint32_t height)
{
    VGpuResourceFlush req; bzero(&req, sizeof(req));
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    req.r.width = width;
    req.r.height = height;
    req.resource_id = resourceId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
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
    // resp half of the scratch (2048 - hdr) so we never overrun fCmdMem.
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
    for (uint32_t idx = 0; idx < 8; idx++) {
        uint32_t cid, cver, csize;
        if (!gpuGetCapsetInfo(idx, &cid, &cver, &csize))
            break; // no more capsets
        DEBUG("virgl capset[%u]: id=%u max_version=%u max_size=%u\n",
              idx, cid, cver, csize);
        if (cid == VIRTIO_GPU_CAPSET_VIRGL2 ||
            (cid == VIRTIO_GPU_CAPSET_VIRGL && !found)) {
            id = cid; ver = cver; size = csize;
            found = true;
            if (cid == VIRTIO_GPU_CAPSET_VIRGL2)
                break; // prefer virgl2, stop once we have it
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
    VGpuCtxResource req; bzero(&req, sizeof(req)); // {hdr; resource_id; padding}
    req.hdr.type = VIRTIO_GPU_CMD_RESOURCE_UNREF;
    req.resource_id = resId;

    VGpuCtrlHdr resp; bzero(&resp, sizeof(resp));
    return sendCommand(&req, sizeof(req), &resp, sizeof(resp)) &&
           resp.type == VIRTIO_GPU_RESP_OK_NODATA;
}

bool
IOVirtIOGPU::gpu3DSubmit(uint32_t ctxId, uint64_t cmdPhys, uint32_t cmdLen, uint64_t fenceId)
{
    // SUBMIT_3D = ctrl hdr + size field (in scratch), then `cmdLen` bytes of
    // virgl command stream (a separate physical buffer), then a writable resp
    // hdr. A 3-descriptor chain; the cmd stream itself is produced by Mesa.
    if (!fCmdVirt || cmdLen == 0)
        return false;

    VGpuCmdSubmit3D *hdr = (VGpuCmdSubmit3D *)fCmdVirt;
    bzero(hdr, sizeof(*hdr));
    hdr->hdr.type = VIRTIO_GPU_CMD_SUBMIT_3D;
    hdr->hdr.ctx_id = ctxId;
    if (fenceId) {
        hdr->hdr.flags |= VIRTIO_GPU_FLAG_FENCE;
        hdr->hdr.fence_id = fenceId;
    }
    hdr->size = cmdLen;

    uint8_t *respBase = (uint8_t *)fCmdVirt + 2048;

    if (fCtrlLock) IOLockLock(fCtrlLock);
    bzero(respBase, sizeof(VGpuCtrlHdr));

    VirtIOChainEntry chain[3] = {
        { fCmdPhys,        (uint32_t)sizeof(VGpuCmdSubmit3D), false },
        { cmdPhys,         cmdLen,                            false },
        { fCmdPhys + 2048, (uint32_t)sizeof(VGpuCtrlHdr),     true  },
    };
    fTransport.addDescChain(&fControlQ, chain, 3);
    fTransport.notify(&fControlQ);
    bool ok = fTransport.pollForCompletion(&fControlQ, 1000);
    if (!ok)
        DEBUG("gpu3DSubmit: timed out\n");
    else
        ok = ((VGpuCtrlHdr *)respBase)->type == VIRTIO_GPU_RESP_OK_NODATA;
    if (fCtrlLock) IOLockUnlock(fCtrlLock);
    return ok;
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
IOVirtIOGPU::flushCallback(thread_call_param_t self, thread_call_param_t)
{
    ((IOVirtIOGPU *)self)->scheduleFlush();
}

void
IOVirtIOGPU::scheduleFlush()
{
    if (fFbBase) {
        gpuTransferToHost2D(fResourceId, fWidth, fHeight);
        gpuResourceFlush(fResourceId, fWidth, fHeight);
    }

    AbsoluteTime deadline;
    clock_interval_to_deadline(kFlushIntervalMs, kMillisecondScale, &deadline);
    thread_call_enter_delayed(fFlushCall, deadline);
}

bool
IOVirtIOGPU::start(IOService *provider)
{
    DEBUG("start provider=%p\n", provider);

    if (!super::start(provider)) {
        DEBUG("super::start failed\n");
        return false;
    }

    // Must exist before the first sendCommand (control-queue serialization)
    // and before any user client allocates ids.
    fCtrlLock = IOLockAlloc();
    if (!fCtrlLock)
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
    uint32_t drvFeat = (devFeat & VIRTIO_GPU_F_VIRGL);
    if (!fTransport.negotiateFeatures(drvFeat)) {
        DEBUG("device rejected feature negotiation\n");
        return false;
    }
    DEBUG("features: device=0x%08x acked=0x%08x (virgl %s)\n",
          devFeat, drvFeat, drvFeat ? "on" : "off");

    if (!fTransport.initQueue(&fControlQ, /*queue 0 = controlq*/ 0, 64)) {
        DEBUG("failed to init control virtqueue\n");
        return false;
    }

    fCmdMem = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        4096, 0xFFFFFFFFULL);
    if (!fCmdMem) {
        DEBUG("failed to allocate command scratch buffer\n");
        return false;
    }
    fCmdVirt = fCmdMem->getBytesNoCopy();
    fCmdPhys = fCmdMem->getPhysicalAddress();
    bzero(fCmdVirt, 4096);

    fTransport.setDriverOk();

    fWidth = kDefaultWidth;
    fHeight = kDefaultHeight;
    gpuGetDisplayInfo(&fWidth, &fHeight); // best-effort; keep defaults on failure
    fPitch = fWidth * 4;
    fResourceId = 1;

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
    gpuTransferToHost2D(fResourceId, fWidth, fHeight);
    gpuResourceFlush(fResourceId, fWidth, fHeight);

    DEBUG("virtio-gpu scanout ready: %ux%u pitch=%u fb=%p (phys 0x%llx)\n",
          fWidth, fHeight, fPitch, fFbBase, fFbPhys);

    if (drvFeat & VIRTIO_GPU_F_VIRGL) {
        gpuProbeVirgl();
    }

    IOPlatformExpert *pe = getPlatform();
    if (pe) {
        PE_Video consoleInfo;
        consoleInfo.v_baseAddr = (unsigned long)fFbPhys | 1; // force mapping
        consoleInfo.v_width = fWidth;
        consoleInfo.v_height = fHeight;
        consoleInfo.v_depth = 32;
        consoleInfo.v_rowBytes = fPitch;
        consoleInfo.v_display = GRAPHICS_MODE;
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

    if (fFlushCall) {
        thread_call_cancel(fFlushCall);
        thread_call_free(fFlushCall);
        fFlushCall = NULL;
    }
    if (fFbMem) { fFbMem->release(); fFbMem = NULL; }
    if (fCmdMem) { fCmdMem->release(); fCmdMem = NULL; }
    fTransport.freeQueue(&fControlQ);
    fTransport.detach();
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }
    if (fCtrlLock) { IOLockFree(fCtrlLock); fCtrlLock = NULL; }

    super::stop(provider);
}

IOReturn
IOVirtIOGPU::newUserClient(task_t owningTask, void *securityID, UInt32 type,
                           IOUserClient **handler)
{
    // Only intercept our 3D connect type; every other type belongs to
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
