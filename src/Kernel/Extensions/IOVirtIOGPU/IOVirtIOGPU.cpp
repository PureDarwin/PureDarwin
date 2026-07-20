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
#include <IOKit/IOLib.h>
#include <kern/thread_call.h>
#include <sys/errno.h>

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

    VIRTIO_GPU_RESP_OK_NODATA        = 0x1100,
    VIRTIO_GPU_RESP_OK_DISPLAY_INFO  = 0x1101,
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

    if (!fTransport.pollForCompletion(&fControlQ, 1000)) {
        DEBUG("sendCommand: timed out waiting for device\n");
        return false;
    }

    memcpy(resp, respBase, respLen);
    return true;
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

    // We don't need any optional virtio-gpu features (3D/virgl, EDID)
    // for a 2D-only driver, so just accept feature bits 0.
    if (!fTransport.negotiateFeatures(0)) {
        DEBUG("device rejected feature negotiation\n");
        return false;
    }

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

    super::stop(provider);
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
