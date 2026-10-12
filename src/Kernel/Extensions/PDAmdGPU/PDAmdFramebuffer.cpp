#include "PDAmdFramebuffer.h"
#include "PDAmdGPU.h"

#define super IOFramebuffer
OSDefineMetaClassAndStructors(PDAmdFramebuffer, IOFramebuffer);

#define kModeID     1
#define kDepth      0

bool PDAmdFramebuffer::start(IOService *provider)
{
    gpu = OSDynamicCast(PDAmdGPU, provider);
    if (!gpu || !gpu->scanout(&fbPhys, &width, &height, &pitch)) return false;
    if (!super::start(provider)) return false;

    LOG("fb: %ux%u pitch %u at phys 0x%llx\n", width, height, pitch, fbPhys);
    registerService();
    return true;
}

IOReturn PDAmdFramebuffer::enableController() { return kIOReturnSuccess; }

const char *PDAmdFramebuffer::getPixelFormats() { return IO32BitDirectPixels; }

IOReturn PDAmdFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
    *displayMode = kModeID;
    *depth = kDepth;
    return kIOReturnSuccess;
}

IOReturn PDAmdFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    return displayMode == kModeID && depth == kDepth ? kIOReturnSuccess : kIOReturnUnsupportedMode;
}

IODeviceMemory *PDAmdFramebuffer::getApertureRange(IOPixelAperture aperture)
{
    if (aperture != kIOFBSystemAperture) return NULL;
    return IODeviceMemory::withRange(fbPhys, (IOPhysicalLength)pitch * height);
}

IODeviceMemory *PDAmdFramebuffer::getVRAMRange(void) { return getApertureRange(kIOFBSystemAperture); }

IOReturn PDAmdFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info)
{
    if (displayMode != kModeID) return kIOReturnUnsupportedMode;
    bzero(info, sizeof(*info));
    info->nominalWidth = width;
    info->nominalHeight = height;
    info->refreshRate = 60 << 16;
    info->maxDepthIndex = kDepth;
    return kIOReturnSuccess;
}

UInt64 PDAmdFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex) { return 0; }

IOReturn PDAmdFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture,
                                               IOPixelInformation *info)
{
    if (aperture || depth != kDepth || displayMode != kModeID) return kIOReturnUnsupportedMode;

    // hubp scans argb8888, which is 32 bit direct pixels in memory order b, g, r, a
    bzero(info, sizeof(*info));
    info->activeWidth = width;
    info->activeHeight = height;
    info->bytesPerRow = pitch;
    strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
    info->pixelType = kIORGBDirectPixels;
    info->componentMasks[0] = 0x00ff0000;
    info->componentMasks[1] = 0x0000ff00;
    info->componentMasks[2] = 0x000000ff;
    info->bitsPerPixel = 32;
    info->componentCount = 3;
    info->bitsPerComponent = 8;
    return kIOReturnSuccess;
}

IOReturn PDAmdFramebuffer::getDisplayModes(IODisplayModeID *allDisplayModes)
{
    *allDisplayModes = kModeID;
    return kIOReturnSuccess;
}

IOItemCount PDAmdFramebuffer::getDisplayModeCount(void) { return 1; }
