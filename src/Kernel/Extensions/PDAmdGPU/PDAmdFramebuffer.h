#pragma once

#include <IOKit/graphics/IOFramebuffer.h>

class PDAmdGPU;

// the hdmi head PDAmdGPU drives, one fixed mode on the scanout surface in vram
class PDAmdFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(PDAmdFramebuffer);

private:
    PDAmdGPU *gpu;
    uint64_t  fbPhys;
    uint32_t  width, height, pitch;

public:
    virtual bool start(IOService *provider) override;

    virtual IOReturn enableController() override;
    virtual const char *getPixelFormats() override;
    virtual IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory *getVRAMRange(void) override;
    virtual IOReturn getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) override;
    virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth, IOPixelAperture aperture,
                                         IOPixelInformation *info) override;
    virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
    virtual IOItemCount getDisplayModeCount(void) override;
};
