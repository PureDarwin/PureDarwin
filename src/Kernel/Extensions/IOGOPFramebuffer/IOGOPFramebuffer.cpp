#include "IOGOPFramebuffer.h"

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

IOService *
IOGOPFramebuffer::probe(IOService *provider, SInt32 *score)
{
    DEBUG("probe %p\n", provider);
    return this;
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
    fbBase = (void *)bootDisplay.v_baseAddr;
    width  = bootDisplay.v_width;
    height = bootDisplay.v_height;
    pitch  = bootDisplay.v_rowBytes;
    bpp    = 32;
    fb0_publish((uint8_t *)fbBase, (size_t)pitch * height);

    DEBUG("framebuffer: %dx%d @ %p\n", width, height, fbBase);
    DEBUG("successfully started\n");

    // Register the framebuffer with the kernel console system
    PE_Video consoleInfo;
    consoleInfo.v_baseAddr   = bootDisplay.v_baseAddr | 1;  // Set low bit to force mapping
    consoleInfo.v_width      = bootDisplay.v_width;
    consoleInfo.v_height     = bootDisplay.v_height;
    consoleInfo.v_depth      = 32;  // GOP is always 32-bit
    consoleInfo.v_rowBytes   = bootDisplay.v_rowBytes;
    /* kPEGraphicsMode (pexpert.h) rather than GRAPHICS_MODE, which is only
     * defined in pexpert/i386/boot.h; both are 1, but the PE name is
     * arch-neutral so this file also builds for arm64. */
    consoleInfo.v_display    = kPEGraphicsMode;
    consoleInfo.v_offset     = 0;
    consoleInfo.v_length     = 0;  // Let kernel calculate from height * rowBytes
    consoleInfo.v_rotate     = 0;
    consoleInfo.v_scale      = kPEScaleFactor1x;

    // Initialize graphics console with this framebuffer
    // Use the public IOPlatformExpert::setConsoleInfo() method
    IOReturn ret = pe->setConsoleInfo(&consoleInfo, kPEGraphicsMode);
    if (ret != kIOReturnSuccess) {
        DEBUG("setConsoleInfo failed: %d\n", ret);
        // Don't fail - we can still register the service.
    } else {
        DEBUG("Kernel graphics console initialized\n");

        bool useGopConsole = false;
        if (PE_parse_boot_argn("gopconsole", &useGopConsole, sizeof(useGopConsole)) && useGopConsole) {
            int oldConsole = switch_to_video_console();
            pe->setConsoleInfo(&consoleInfo, kPEAcquireScreen);
            pe->setConsoleInfo(&consoleInfo, kPETextScreen);
            DEBUG("active console switched to video, old console=%d\n", oldConsole);

            vc_progress_set(FALSE, 0);
            vc_progress_set(TRUE, 0);
            DEBUG("gopprogress: overlaid kernel progress meter on text console\n");
        }
    }

    registerService();
    return true;
}

void
IOGOPFramebuffer::stop(IOService *provider)
{
    DEBUG("stop %p\n", provider);
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
