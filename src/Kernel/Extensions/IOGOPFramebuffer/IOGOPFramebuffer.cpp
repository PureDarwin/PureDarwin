#include "IOGOPFramebuffer.h"

#include <IOKit/pci/IOPCIDevice.h>

#include <miscfs/devfs/devfs.h>
#include <kern/thread_call.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/uio.h>

#define kDisplayModeID 1
#define kDepth         0
#if defined(__i386__) || defined(__x86_64__)
#define kIOGOPFBMapCacheMode kIOMapWriteCombineCache
#else
#define kIOGOPFBMapCacheMode kIOMapInhibitCache
#endif

extern "C" int switch_to_video_console(void);
extern "C" void vc_progress_set(boolean_t enable, uint32_t vc_delay);
extern "C" int devfs_is_ready(void);
extern "C" boolean_t PE_parse_boot_argn(const char *arg_string, void *arg_ptr, int max_arg);

#define super IOFramebuffer
OSDefineMetaClassAndStructors(IOGOPFramebuffer, IOFramebuffer);

static void *gFb0Node;
static int gFb0Major = -1;
static uint8_t *gFb0Base;
static IODeviceMemory *gFb0Memory;
static IOMemoryMap *gFb0Map;
static size_t gFb0Size;
static thread_call_t gFb0RetryCall;
static IONotifier *gFb0BSDNotifier;
static unsigned gFb0RetryCount;
static bool gIOGOPDebug;
static bool gIOGOPDebugChecked;

static void fb0_publish_retry(thread_call_param_t, thread_call_param_t);
static bool fb0_iobsd_published(void *, void *, IOService *, IONotifier *);

static bool
gop_debug_enabled()
{
    if (!gIOGOPDebugChecked) {
        PE_parse_boot_argn("gop_debug", &gIOGOPDebug, sizeof(gIOGOPDebug));
        gIOGOPDebugChecked = true;
    }
    return gIOGOPDebug;
}

#define DEBUG(x, ...) do {                                      \
    if (gop_debug_enabled())                                    \
        kprintf("IOGOPFramebuffer: " x, ##__VA_ARGS__);         \
} while (0)

static void
fb0_schedule_retry()
{
    AbsoluteTime deadline;

    if (gFb0Node || !gFb0Base || !gFb0Size || gFb0RetryCount >= 30) {
        return;
    }

    if (!gFb0RetryCall) {
        gFb0RetryCall = thread_call_allocate(fb0_publish_retry, NULL);
        if (!gFb0RetryCall) {
            DEBUG("failed to allocate /dev/fb0 retry call\n");
            return;
        }
    }

    gFb0RetryCount++;
    clock_interval_to_deadline(1, kSecondScale, &deadline);
    thread_call_enter_delayed(gFb0RetryCall, deadline);
}

static int
fb0_open(dev_t, int, int, struct proc *)
{
    return gFb0Base ? 0 : ENXIO;
}

static int
fb0_close(dev_t, int, int, struct proc *)
{
    return 0;
}

static int
fb0_write(dev_t, struct uio *uio, int)
{
    off_t offset;
    user_ssize_t resid;
    size_t count;

    if (!gFb0Base || !gFb0Size) {
        return ENXIO;
    }

    offset = uio_offset(uio);
    if (offset < 0 || (uint64_t)offset >= gFb0Size) {
        return ENOSPC;
    }

    resid = uio_resid(uio);
    if (resid <= 0) {
        return 0;
    }

    count = gFb0Size - (size_t)offset;
    if ((uint64_t)resid < count) {
        count = (size_t)resid;
    }

    return uiomove((const char *)(gFb0Base + offset), (int)count, uio);
}

static struct cdevsw fb0_cdevsw =
{
    /* d_open     */ fb0_open,
    /* d_close    */ fb0_close,
    /* d_read     */ eno_rdwrt,
    /* d_write    */ fb0_write,
    /* d_ioctl    */ eno_ioctl,
    /* d_stop     */ eno_stop,
    /* d_reset    */ eno_reset,
    /* d_ttys     */ 0,
    /* d_select   */ eno_select,
    /* d_mmap     */ eno_mmap,
    /* d_strategy */ eno_strat,
    /* d_getc     */ eno_getc,
    /* d_putc     */ eno_putc,
    /* d_type     */ 0
};

static void
fb0_publish(uint8_t *physicalBase, size_t size)
{
    if (!physicalBase || !size) {
        return;
    }

    gFb0Size = size;

    if (!gFb0Map) {
        gFb0Memory = IODeviceMemory::withRange((IOPhysicalAddress)(uintptr_t)physicalBase,
                                               (IOPhysicalLength)size);
        if (!gFb0Memory) {
            DEBUG("failed to create /dev/fb0 memory descriptor for %p size=%lu\n",
                  physicalBase, (unsigned long)size);
            return;
        }

        gFb0Map = gFb0Memory->map(kIOMapAnywhere | kIOGOPFBMapCacheMode);
        if (!gFb0Map) {
            DEBUG("failed to map /dev/fb0 physical range %p size=%lu\n",
                  physicalBase, (unsigned long)size);
            gFb0Memory->release();
            gFb0Memory = 0;
            return;
        }

        gFb0Base = (uint8_t *)(uintptr_t)gFb0Map->getAddress();
        DEBUG("mapped /dev/fb0 phys=%p virt=%p size=%lu\n",
              physicalBase, gFb0Base, (unsigned long)size);
    }

    if (gFb0Node) {
        return;
    }

    if (gFb0Major < 0) {
        gFb0Major = cdevsw_add(-1, &fb0_cdevsw);
        if (gFb0Major < 0) {
            DEBUG("cdevsw_add for /dev/fb0 failed\n");
            return;
        }
    }

    if (!devfs_is_ready()) {
        if (!gFb0BSDNotifier) {
            OSDictionary *matching = IOService::resourceMatching("IOBSD");
            if (matching) {
                gFb0BSDNotifier = IOService::addMatchingNotification(gIOPublishNotification,
                                                                     matching,
                                                                     fb0_iobsd_published,
                                                                     NULL,
                                                                     NULL);
                matching->release();
            }
        }
        fb0_schedule_retry();
        return;
    }

    gFb0Node = devfs_make_node(makedev(gFb0Major, 0), DEVFS_CHAR,
                               0, 0, 0666, "fb0");
    if (!gFb0Node) {
        DEBUG("devfs_make_node for /dev/fb0 failed, retry %u\n", gFb0RetryCount);
        fb0_schedule_retry();
    } else {
        gFb0RetryCount = 0;
        DEBUG("published /dev/fb0 size=%lu\n", (unsigned long)gFb0Size);
    }
}

static void
fb0_publish_retry(thread_call_param_t, thread_call_param_t)
{
    fb0_publish(gFb0Base, gFb0Size);
}

static bool
fb0_iobsd_published(void *, void *, IOService *, IONotifier *notifier)
{
    fb0_publish(gFb0Base, gFb0Size);

    if (gFb0Node && notifier) {
        notifier->remove();
        if (gFb0BSDNotifier == notifier) {
            gFb0BSDNotifier = 0;
        }
    }

    return true;
}

static void
fb0_unpublish()
{
    if (gFb0RetryCall) {
        thread_call_cancel(gFb0RetryCall);
    }
    if (gFb0BSDNotifier) {
        gFb0BSDNotifier->remove();
        gFb0BSDNotifier = 0;
    }
    if (gFb0Node) {
        devfs_remove(gFb0Node);
        gFb0Node = 0;
    }
    if (gFb0Major >= 0) {
        cdevsw_remove(gFb0Major, &fb0_cdevsw);
        gFb0Major = -1;
    }
    if (gFb0Map) {
        gFb0Map->release();
        gFb0Map = 0;
    }
    if (gFb0Memory) {
        gFb0Memory->release();
        gFb0Memory = 0;
    }
    gFb0Base = 0;
    gFb0Size = 0;
    gFb0RetryCount = 0;
}

// true when the firmware framebuffer lies in one of this pci device's memory BARs
static bool
gop_on_device(IOPCIDevice *pci, uint64_t phys, uint64_t size)
{
    for (IOItemCount i = 0; i < pci->getDeviceMemoryCount(); i++) {
        IODeviceMemory *bar = pci->getDeviceMemoryWithIndex(i);
        if (bar == NULL)
            continue;
        uint64_t start = bar->getPhysicalAddress(), len = bar->getLength();
        if (phys >= start && phys + size <= start + len)
            return true;
    }
    return false;
}

IOService *
IOGOPFramebuffer::probe(IOService *provider, SInt32 *score)
{
    DEBUG("probe %p\n", provider);

    // every display device matches, only the one whose BAR holds the firmware framebuffer drives it
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    IOPlatformExpert *pe = getPlatform();
    PE_Video video;
    if (pci != NULL && pe != NULL && pe->getConsoleInfo(&video) == kIOReturnSuccess && video.v_baseAddr != 0) {
        uint64_t phys = video.v_baseAddr & ~3UL;
        if (!gop_on_device(pci, phys, (uint64_t)video.v_rowBytes * video.v_height)) {
            IOLog("IOGOPFramebuffer: the firmware framebuffer at 0x%llx is not on %04x:%04x, leaving that gpu alone\n",
                  (unsigned long long)phys, pci->configRead16(kIOPCIConfigVendorID),
                  pci->configRead16(kIOPCIConfigDeviceID));
            return NULL;
        }
    }
    return this;
}

// the chipsets nvkm_device_ctor in linux 6.14's nouveau knows, NV04 through AD10x
static bool
nouveau_supports(uint32_t boot0, uint32_t *chipset)
{
    static const uint16_t known[] = {
        0x004, 0x005, 0x010, 0x011, 0x015, 0x017, 0x018, 0x01a, 0x01f, 0x020, 0x025, 0x028,
        0x02a, 0x030, 0x031, 0x034, 0x035, 0x036, 0x040, 0x041, 0x042, 0x043, 0x044, 0x045,
        0x046, 0x047, 0x049, 0x04a, 0x04b, 0x04c, 0x04e, 0x050, 0x063, 0x067, 0x068, 0x084,
        0x086, 0x092, 0x094, 0x096, 0x098, 0x0a0, 0x0a3, 0x0a5, 0x0a8, 0x0aa, 0x0ac, 0x0af,
        0x0c0, 0x0c1, 0x0c3, 0x0c4, 0x0c8, 0x0ce, 0x0cf, 0x0d7, 0x0d9, 0x0e4, 0x0e6, 0x0e7,
        0x0ea, 0x0f0, 0x0f1, 0x106, 0x108, 0x117, 0x118, 0x120, 0x124, 0x126, 0x12b, 0x130,
        0x132, 0x134, 0x136, 0x137, 0x138, 0x13b, 0x140, 0x162, 0x164, 0x166, 0x167, 0x168,
        0x172, 0x173, 0x174, 0x176, 0x177, 0x192, 0x193, 0x194, 0x196, 0x197,
    };

    *chipset = 0;
    if (boot0 == 0 || boot0 == 0xffffffff)
        return false;
    if ((boot0 & 0x1f000000) != 0)
        *chipset = (boot0 & 0x1ff00000) >> 20;
    else if ((boot0 & 0xff00fff0) == 0x20004000)
        *chipset = (boot0 & 0x00f00000) ? 0x05 : 0x04;
    for (unsigned i = 0; i < sizeof(known) / sizeof(known[0]); i++) {
        if (known[i] == *chipset)
            return true;
    }
    return false;
}

// boot0 from BAR 0, read only while the firmware left memory decoding on
static uint32_t
nvidia_boot0(IOPCIDevice *pci)
{
    uint32_t boot0 = 0xffffffff;

    if ((pci->configRead16(kIOPCIConfigCommand) & kIOPCICommandMemorySpace) == 0)
        return boot0;
    IOMemoryMap *map = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0, kIOMapInhibitCache);
    if (map == NULL)
        return boot0;
    if (map->getLength() >= 4)
        boot0 = *(volatile uint32_t *)map->getVirtualAddress();
    map->release();
    return boot0;
}

// the drm kext that would take this gpu over, NULL when none is expected to
static const char *
gop_drm_driver(IOPCIDevice *pci)
{
    uint16_t vendor = pci->configRead16(kIOPCIConfigVendorID);
    int on;

    if (vendor == 0x10de || vendor == 0x12d2) {
        uint32_t boot0, chipset;
        on = 0;
        PE_parse_boot_argn("pdnouveau", &on, sizeof(on));
        if (!on)
            return NULL;
        boot0 = nvidia_boot0(pci);
        if (!nouveau_supports(boot0, &chipset)) {
            IOLog("IOGOPFramebuffer: nvidia gpu %04x:%04x (boot0 0x%08x, chipset 0x%x) is not one nouveau "
                  "supports, keeping the firmware framebuffer\n",
                  vendor, pci->configRead16(kIOPCIConfigDeviceID), boot0, chipset);
            return NULL;
        }
        return "nouveau";
    }
    if (vendor == 0x1002) {
        on = 1;
        PE_parse_boot_argn("pdamdgpu", &on, sizeof(on));
        return on ? "amdgpu" : NULL;
    }
    if (vendor == 0x8086) {
        on = 1;
        PE_parse_boot_argn("pdi915", &on, sizeof(on));
        return on ? "i915" : NULL;
    }
    return NULL;
}

// a drm driver remaps the gpu's BARs, so the firmware framebuffer stays only until it has lit a display
void
IOGOPFramebuffer::waitForDrm(IOService *provider)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);

    if (pci == NULL || (drmName = gop_drm_driver(pci)) == NULL)
        return;

    drmWaiting = true;
    drmTimeout = thread_call_allocate(drmTimedOut, this);
    if (drmTimeout != NULL) {
        uint64_t deadline;
        retain();
        clock_interval_to_deadline(15, kSecondScale, &deadline);
        thread_call_enter_delayed(drmTimeout, deadline);
    }
    IOLog("IOGOPFramebuffer: %04x:%04x, keeping the firmware framebuffer until %s lights a display\n",
          pci->configRead16(kIOPCIConfigVendorID), pci->configRead16(kIOPCIConfigDeviceID), drmName);
    OSDictionary *match = serviceMatching("IOFramebuffer");
    if (match != NULL) {
        drmNotifier = addMatchingNotification(gIOPublishNotification, match, drmPublished, this, NULL);
        match->release();
    }
}

// each drm kext's PDDRMFramebuffer<driver> registers only after its first modeset, so this is the handover point
bool
IOGOPFramebuffer::drmPublished(void *target, void *, IOService *fb, IONotifier *)
{
    IOGOPFramebuffer *self = (IOGOPFramebuffer *)target;
    IOService *driver = fb->getProvider();

    if (strncmp(fb->getMetaClass()->getClassName(), "PDDRMFramebuffer", 16) != 0)
        return true;
    if (driver == NULL || driver->getProvider() != self->getProvider() || !self->drmWaiting)
        return true;
    self->drmWaiting = false;
    if (self->drmTimeout != NULL && thread_call_cancel(self->drmTimeout))
        self->release();
    IOLog("IOGOPFramebuffer: %s lit a display, handing the gpu over\n", self->drmName);
    fb0_unpublish();
    // the console still draws into the firmware framebuffer, which is vram the drm driver now owns
    vc_progress_set(FALSE, 0);
    self->getPlatform()->setConsoleInfo(NULL, kPEDisableScreen);
    return true;
}

void
IOGOPFramebuffer::drmTimedOut(thread_call_param_t param, thread_call_param_t)
{
    IOGOPFramebuffer *self = (IOGOPFramebuffer *)param;
    IOService *gpu = self->getProvider();
    bool attached = false;

    if (self->drmWaiting && gpu != NULL) {
        OSIterator *it = gpu->getClientIterator();
        OSObject *client;
        while (it != NULL && (client = it->getNextObject()) != NULL) {
            if (strncmp(client->getMetaClass()->getClassName(), "PDDRM", 5) == 0)
                attached = true;
        }
        OSSafeReleaseNULL(it);
        // the drm kext leaves the reason it let go of the gpu in PDDRMStatus
        OSString *status = OSDynamicCast(OSString, gpu->getProperty("PDDRMStatus"));
        bool delayed = status != NULL && status->isEqualTo("attach delayed by pddrm_delay");
        bool pending = delayed || status == NULL || status->isEqualTo("attached, no display lit yet");
        // a driver from the aux KC probes late, so look again until it has given a reason
        // a delayed attach does not use up the checks
        if (pending && (delayed || self->drmChecks++ < 4)) {
            uint64_t deadline;
            if (self->drmChecks == 1)
                IOLog("IOGOPFramebuffer: %s did not take over within 15 s, keeping the firmware framebuffer\n",
                      self->drmName);
            clock_interval_to_deadline(15, kSecondScale, &deadline);
            self->retain();
            thread_call_enter_delayed(self->drmTimeout, deadline);
        } else {
            const char *why = status != NULL ? status->getCStringNoCopy() :
                attached ? "attached, no display lit yet" : "the driver never probed this gpu";
            IOLog("IOGOPFramebuffer: %s gave up the gpu, the firmware framebuffer keeps the display\n",
                  self->drmName);
            // a compositor waiting for the gpu driver's framebuffer reads this and stops waiting
            self->setProperty("PDDRMGaveUp", kOSBooleanTrue);
            IOLog("IOGOPFramebuffer: %s: %s\n", self->drmName, why);
        }
    }
    self->release();
}

bool
IOGOPFramebuffer::start(IOService *provider)
{
    DEBUG("start provider=%p\n", provider);

    if (!super::start(provider)) {
        DEBUG("super::start failed\n");
        return false;
    }

    PE_Video    bootDisplay;
    IOPlatformExpert *pe = getPlatform();
    if (!pe) {
        DEBUG("getPlatform() returned null\n");
        return false;
    }
    IOReturn err = pe->getConsoleInfo( &bootDisplay);
    if (err != kIOReturnSuccess) {
        DEBUG("getConsoleInfo failed: %d\n", err);
        return false;
    }
    if (bootDisplay.v_baseAddr == 0) {
        DEBUG("no framebuffer base address\n");
        return false;
    }
    /* initialize_screen() uses the low bits to tag a physical framebuffer.
     * IODeviceMemory takes the actual aligned physical address. */
    fbBase = (void *)(bootDisplay.v_baseAddr & ~3UL);
    width  = bootDisplay.v_width;
    height = bootDisplay.v_height;
    pitch  = bootDisplay.v_rowBytes;
    bpp    = 32;
    fb0_publish((uint8_t *)fbBase, (size_t)pitch * height);

    DEBUG("framebuffer: %dx%d @ %p\n", width, height, fbBase);
    DEBUG("successfully started\n");

    registerService();
    waitForDrm(provider);
    return true;
}

void
IOGOPFramebuffer::stop(IOService *provider)
{
    DEBUG("stop %p\n", provider);
    drmWaiting = false;
    if (drmNotifier != NULL) {
        drmNotifier->remove();
        drmNotifier = NULL;
    }
    if (drmTimeout != NULL) {
        if (thread_call_cancel(drmTimeout))
            release();
        thread_call_free(drmTimeout);
        drmTimeout = NULL;
    }
    fb0_unpublish();
    super::stop(provider);
}

void * IOGOPFramebuffer::getBaseAddress(void) { return fbBase; }
uint32_t IOGOPFramebuffer::getWidth(void) { return width; }
uint32_t IOGOPFramebuffer::getHeight(void) { return height; }
uint32_t IOGOPFramebuffer::getPitch(void) { return pitch; }
uint32_t IOGOPFramebuffer::getDepth(void) { return bpp; }

IOReturn
IOGOPFramebuffer::enableController()
{
    return kIOReturnSuccess;
}

const char *
IOGOPFramebuffer::getPixelFormats()
{
    const char *        ret;
    PE_Video            bootDisplay;

    getPlatform()->getConsoleInfo( &bootDisplay);

    switch (bootDisplay.v_depth)
    {
        case 8:
        default:
            ret = IO8BitIndexedPixels;
            break;
        case 15:
        case 16:
            ret = IO16BitDirectPixels;
            break;
        case 24:
        case 32:
            ret = IO32BitDirectPixels;
            break;
    }

    return (ret);
}

IOReturn
IOGOPFramebuffer::getCurrentDisplayMode(IODisplayModeID * displayMode,
                                        IOIndex         * depth)
{
    *displayMode = kDisplayModeID;
    *depth = kDepth;
    return kIOReturnSuccess;
}

IOReturn
IOGOPFramebuffer::setDisplayMode(IODisplayModeID displayMode,
                                 IOIndex depth)
{
    return kIOReturnSuccess;
}

IODeviceMemory *
IOGOPFramebuffer::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange(
        (mach_vm_address_t)fbBase,
        pitch * height
    );
}

IODeviceMemory *
IOGOPFramebuffer::getVRAMRange(void)
{
    return getApertureRange(kIOFBSystemAperture);
}

IOReturn
IOGOPFramebuffer::getInformationForDisplayMode(IODisplayModeID,
                                               IODisplayModeInformation * info)
{
    bzero(info, sizeof(*info));

    info->nominalWidth  = width;
    info->nominalHeight = height;
    info->refreshRate   = 60 << 16;

    info->maxDepthIndex = kDepth;

    return kIOReturnSuccess;
}

UInt64
IOGOPFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID,
                                                IOIndex)
{
    return (UInt64)(uintptr_t)getPixelFormats();
}

IOItemCount
IOGOPFramebuffer::getDisplayModeCount(void)
{
    return 1;
}

IOReturn
IOGOPFramebuffer::getDisplayModes(IODisplayModeID * allDisplayModes)
{
    *allDisplayModes = kDisplayModeID;
    return kIOReturnSuccess;
}

IOReturn
IOGOPFramebuffer::getPixelInformation(IODisplayModeID      displayMode,
                                      IOIndex              depth,
                                      IOPixelAperture      aperture,
                                      IOPixelInformation * info)
{
    PE_Video    bootDisplay;

    if (aperture || depth || (displayMode != kDisplayModeID))
    {
        return (kIOReturnUnsupportedMode);
    }

    getPlatform()->getConsoleInfo( &bootDisplay);

    bzero( info, sizeof(*info));

    info->activeWidth           = static_cast<UInt32>(bootDisplay.v_width);
    info->activeHeight          = static_cast<UInt32>(bootDisplay.v_height);
    info->bytesPerRow           = bootDisplay.v_rowBytes & 0x7fff;
    info->bytesPerPlane         = 0;

    switch (bootDisplay.v_depth)
    {
        case 8:
        default:
            strlcpy(info->pixelFormat, IO8BitIndexedPixels, sizeof(info->pixelFormat));
            info->pixelType             = kIOCLUTPixels;
            info->componentMasks[0]     = 0xff;
            info->bitsPerPixel          = 8;
            info->componentCount        = 1;
            info->bitsPerComponent      = 8;
            break;
        case 15:
        case 16:
            strlcpy(info->pixelFormat, IO16BitDirectPixels, sizeof(info->pixelFormat));
            info->pixelType     = kIORGBDirectPixels;
            info->componentMasks[0] = 0x7c00;
            info->componentMasks[1] = 0x03e0;
            info->componentMasks[2] = 0x001f;
            info->bitsPerPixel  = 16;
            info->componentCount        = 3;
            info->bitsPerComponent      = 5;
            break;
        case 24:
        case 32:
            strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
            info->pixelType     = kIORGBDirectPixels;
            info->componentMasks[0] = 0x00ff0000;
            info->componentMasks[1] = 0x0000ff00;
            info->componentMasks[2] = 0x000000ff;
            info->bitsPerPixel  = 32;
            info->componentCount        = 3;
            info->bitsPerComponent      = 8;
            break;
    }

    return (kIOReturnSuccess);
}

// a drm driver (PDDRMFramebuffer) that drives this gpu has reprogrammed the planes
// and the aperture, so drawing into the firmware buffer no longer reaches the screen
IOReturn
IOGOPFramebuffer::newUserClient(task_t owningTask, void *security_id, UInt32 type,
    IOUserClient **clientH)
{
    IOService *gpu = getProvider();
    OSDictionary *match = serviceMatching("IOFramebuffer");
    OSIterator *it = match != NULL ? getMatchingServices(match) : NULL;
    bool replaced = false;

    OSSafeReleaseNULL(match);
    if (it != NULL) {
        IOService *drm;
        // each drm kext names its framebuffer PDDRMFramebuffer or PDDRMFramebuffer<driver>
        while (!replaced && (drm = OSDynamicCast(IOService, it->getNextObject())) != NULL) {
            IOService *driver = drm->getProvider();
            replaced = strncmp(drm->getMetaClass()->getClassName(), "PDDRMFramebuffer", 16) == 0 &&
                gpu != NULL && driver != NULL && driver->getProvider() == gpu;
        }
        it->release();
    }
    if (replaced) {
        IOLog("IOGOPFramebuffer: PDDRMFramebuffer drives this gpu now, refusing clients\n");
        return kIOReturnNotPermitted;
    }
    return IOFramebuffer::newUserClient(owningTask, security_id, type, clientH);
}
