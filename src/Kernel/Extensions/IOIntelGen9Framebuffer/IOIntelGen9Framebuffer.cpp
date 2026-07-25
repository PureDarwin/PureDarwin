#include "IOIntelGen9Framebuffer.h"

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOLib.h>

extern "C" {
#include <lil/intel.h>
}

#define kDisplayModeID 1
#define kDepth         0

static bool gDebug;
static bool gDebugChecked;
extern "C" boolean_t PE_parse_boot_argn(const char *, void *, int);
extern "C" int switch_to_video_console(void);

// lil_panic recovery state (defined in lil_imports.cpp). lilBringup arms this
// with __builtin_setjmp so a lil_assert failure unwinds here instead of
// panicking the whole machine (falls back to IOGOPFramebuffer).
extern "C" void *gLilPanicBuf[5];
extern "C" int gLilPanicArmed;

static bool debug_enabled()
{
    if (!gDebugChecked) {
        PE_parse_boot_argn("gen9_debug", &gDebug, sizeof(gDebug));
        gDebugChecked = true;
    }
    return gDebug;
}

#define DEBUG(x, ...) do { \
    if (debug_enabled()) kprintf("IOIntelGen9Framebuffer: " x, ##__VA_ARGS__); \
} while (0)

#define super IOFramebuffer
OSDefineMetaClassAndStructors(IOIntelGen9Framebuffer, IOFramebuffer);

// The ISA/LPC bridge (PCI 0:1f.0) device id is what lil's pch::get_gen() keys
// off. Walk the provider's parent PCI bus for the 0x0601 (ISA bridge) function.
uint16_t
IOIntelGen9Framebuffer::findPchDeviceId(void)
{
    OSIterator *it = getMatchingServices(serviceMatching("IOPCIDevice"));
    uint16_t found = 0;
    if (it) {
        OSObject *o;
        while ((o = it->getNextObject())) {
            IOPCIDevice *d = OSDynamicCast(IOPCIDevice, o);
            if (!d)
                continue;
            uint32_t classCode = d->configRead32(0x08) >> 8; // class/subclass/progif
            if ((classCode & 0xFFFF00) == 0x060100) {         // ISA bridge
                found = (uint16_t)(d->configRead32(0x00) >> 16);
                break;
            }
        }
        it->release();
    }
    DEBUG("PCH device id = 0x%04x\n", found);
    return found;
}

bool
IOIntelGen9Framebuffer::mapScanoutGtt(void)
{
    fGpuFbAddr = 0; // low GTT is free before we bring up any other surface

    const uint32_t size = (uint32_t)round_page((uint64_t)fPitch * fHeight);

    // Kaby Lake GTT entries carry a 39-bit, page-aligned host address.
    const mach_vm_address_t phys_mask = 0x0000007FFFFFF000ULL;
    fFbBacking = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task,
        kIOMemoryPhysicallyContiguous | kIODirectionInOut | kIOMapWriteCombineCache,
        size, phys_mask);
    if (!fFbBacking) {
        DEBUG("failed to allocate %u-byte scanout backing\n", size);
        return false;
    }
    fFbBacking->prepare();

    IOByteCount segLen = 0;
    addr64_t base_phys = fFbBacking->getPhysicalSegment(0, &segLen, kIOMemoryMapperNone);
    if (!base_phys) {
        DEBUG("scanout backing has no physical segment\n");
        return false;
    }

    // Program one GTT PTE per page. The buffer is contiguous, so pages are
    // base_phys + i*PAGE_SIZE; vmem_map writes the PTE at gpu_addr.
    if (!fGpu->vmem_map) {
        DEBUG("gpu has no vmem_map (unsupported gen?)\n");
        return false;
    }
    for (uint32_t off = 0; off < size; off += PAGE_SIZE)
        fGpu->vmem_map(fGpu, (uint64_t)(base_phys + off), fGpuFbAddr + off);

    // CPU view: the BAR2 aperture (gpu->vram) at the same GTT offset.
    fFbBase = (uint8_t *)(fGpu->vram + fGpuFbAddr);

    // Console view: the backing's own physical address. The CPU writing there
    // hits the exact pages the display engine fetches through GTT@fGpuFbAddr, so
    // there's no need for the BAR2 aperture physical (which reports 0 here).
    fFbPhys = (uint64_t)base_phys;

    DEBUG("scanout: %u bytes, backing phys=0x%llx gtt@0x%x aperture=%p phys=0x%llx\n",
          size, (uint64_t)base_phys, fGpuFbAddr, fFbBase, fFbPhys);
    return true;
}

bool
IOIntelGen9Framebuffer::lilBringup(void)
{
    if (__builtin_setjmp(gLilPanicBuf)) {
        DEBUG("lil bringup aborted by lil_panic; falling back to GOP\n");
        return false;
    }
    gLilPanicArmed = 1;

    uint16_t pch_id = findPchDeviceId();

    if (!lil_init_gpu(&fGpu, fPCIDevice, pch_id) || !fGpu) {
        DEBUG("lil_init_gpu failed\n");
        return false;
    }
    DEBUG("lil_init_gpu ok: %u connectors, mmio=0x%lx vram=0x%lx gtt=0x%lx\n",
          fGpu->num_connectors, fGpu->mmio_start, fGpu->vram, fGpu->gtt_address);

    // Find a connected connector and its preferred mode.
    LilConnector *con = nullptr;
    LilModeInfo mode;
    bzero(&mode, sizeof(mode));
    for (uint32_t i = 0; i < fGpu->num_connectors; i++) {
        LilConnector *c = &fGpu->connectors[i];
        if (!c->is_connected || !c->is_connected(fGpu, c))
            continue;
        LilConnectorInfo ci = c->get_connector_info(fGpu, c);
        if (ci.num_modes == 0 || !ci.modes)
            continue;
        con = c;
        mode = ci.modes[0]; // preferred mode is first
        break;
    }
    if (!con) {
        DEBUG("no connected connector with modes\n");
        return false;
    }
    DEBUG("using connector %u: %dx%d\n", con->id, mode.hactive, mode.vactive);

    LilCrtc *crtc = con->crtc;
    if (!crtc) {
        DEBUG("connector has no crtc\n");
        return false;
    }
    crtc->current_mode = mode;

    fWidth  = (uint32_t)mode.hactive;
    fHeight = (uint32_t)mode.vactive;
    fPitch  = fWidth * 4;

    // Allocate the scanout backing and program the GTT so both the CPU (via the
    // BAR2 aperture) and the display engine (via GpuAddr) reach the same pages.
    if (!mapScanoutGtt()) {
        DEBUG("scanout GTT mapping failed\n");
        return false;
    }

    // Point the primary plane at the GTT address we mapped the scanout to.
    if (crtc->num_planes && crtc->planes) {
        LilPlane *plane = &crtc->planes[0];
        plane->enabled = true;
        if (plane->update_surface)
            plane->update_surface(fGpu, plane, fGpuFbAddr, fPitch);
        else
            plane->surface_address = fGpuFbAddr;
    }

    if (crtc->commit_modeset)
        crtc->commit_modeset(fGpu, crtc);

    DEBUG("modeset committed: %ux%u pitch=%u fbphys=0x%llx\n",
          fWidth, fHeight, fPitch, fFbPhys);
    gLilPanicArmed = 0; // bringup done; stop catching lil_panic
    return true;
}

IOService *
IOIntelGen9Framebuffer::probe(IOService *provider, SInt32 *score)
{
    DEBUG("probe %p\n", provider);
    return super::probe(provider, score);
}

bool
IOIntelGen9Framebuffer::start(IOService *provider)
{
    DEBUG("start %p\n", provider);

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice) {
        DEBUG("provider is not an IOPCIDevice\n");
        return false;
    }
    fPCIDevice->retain();
    fPCIDevice->open(this);
    fPCIDevice->setMemoryEnable(true);
    fPCIDevice->setBusMasterEnable(true);

    if (!super::start(provider)) {
        DEBUG("super::start failed\n");
        goto fail;
    }

    if (!lilBringup()) {
        DEBUG("lil bringup failed\n");
        goto fail;
    }

    // Publish the linear scanout as the kernel graphics console, like the other
    // PD framebuffers do (setConsoleInfo path lives in the platform expert).
    {
        IOPlatformExpert *pe = getPlatform();
        if (pe && fFbPhys) {
            PE_Video info;
            bzero(&info, sizeof(info));
            info.v_baseAddr = (unsigned long)fFbPhys | 1; // physical, force map
            info.v_width    = fWidth;
            info.v_height   = fHeight;
            info.v_depth    = 32;
            info.v_rowBytes = fPitch;
            info.v_display  = GRAPHICS_MODE;
            info.v_scale    = kPEScaleFactor1x;
            pe->setConsoleInfo(&info, kPEGraphicsMode);

            // Without this the vc console stays in graphics-boot mode (only the
            // progress meter draws, text is suppressed). Switch to the on-screen
            // text console so the boot log appears on the panel. gopconsole=1
            // forces it; default on for gen9 bringup so there's visible output.
            bool wantConsole = true;
            PE_parse_boot_argn("gopconsole", &wantConsole, sizeof(wantConsole));
            if (wantConsole) {
                switch_to_video_console();
                pe->setConsoleInfo(&info, kPEAcquireScreen);
                pe->setConsoleInfo(&info, kPETextScreen);
                DEBUG("switched to on-screen text console\n");
            }
        }
    }

    registerService();
    return true;

fail:
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }
    return false;
}

void
IOIntelGen9Framebuffer::stop(IOService *provider)
{
    DEBUG("stop %p\n", provider);
    if (fFbBacking) {
        fFbBacking->complete();
        fFbBacking->release();
        fFbBacking = NULL;
    }
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = NULL;
    }
    super::stop(provider);
}

IOReturn IOIntelGen9Framebuffer::enableController() { return kIOReturnSuccess; }

const char *IOIntelGen9Framebuffer::getPixelFormats() { return IO32BitDirectPixels; }

IOReturn
IOIntelGen9Framebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
    *displayMode = kDisplayModeID;
    *depth = kDepth;
    return kIOReturnSuccess;
}

IOReturn IOIntelGen9Framebuffer::setDisplayMode(IODisplayModeID, IOIndex)
{
    return kIOReturnSuccess;
}

IODeviceMemory *
IOIntelGen9Framebuffer::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange((mach_vm_address_t)fFbPhys, (IOByteCount)fPitch * fHeight);
}

IODeviceMemory *IOIntelGen9Framebuffer::getVRAMRange(void)
{
    return getApertureRange(kIOFBSystemAperture);
}

IOReturn
IOIntelGen9Framebuffer::getInformationForDisplayMode(IODisplayModeID, IODisplayModeInformation *info)
{
    bzero(info, sizeof(*info));
    info->nominalWidth  = fWidth;
    info->nominalHeight = fHeight;
    info->refreshRate   = 60 << 16;
    info->maxDepthIndex = kDepth;
    return kIOReturnSuccess;
}

UInt64
IOIntelGen9Framebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
    return (UInt64)(uintptr_t)getPixelFormats();
}

IOReturn
IOIntelGen9Framebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                            IOPixelAperture aperture, IOPixelInformation *info)
{
    if (aperture || depth || (displayMode != kDisplayModeID))
        return kIOReturnUnsupportedMode;

    bzero(info, sizeof(*info));
    info->activeWidth   = fWidth;
    info->activeHeight  = fHeight;
    info->bytesPerRow   = fPitch;
    info->bytesPerPlane = 0;
    strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
    info->pixelType         = kIORGBDirectPixels;
    info->componentMasks[0] = 0x00ff0000;
    info->componentMasks[1] = 0x0000ff00;
    info->componentMasks[2] = 0x000000ff;
    info->bitsPerPixel      = 32;
    info->componentCount    = 3;
    info->bitsPerComponent  = 8;
    return kIOReturnSuccess;
}

IOItemCount IOIntelGen9Framebuffer::getDisplayModeCount(void) { return 1; }

IOReturn IOIntelGen9Framebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    *allDisplayModes = kDisplayModeID;
    return kIOReturnSuccess;
}
