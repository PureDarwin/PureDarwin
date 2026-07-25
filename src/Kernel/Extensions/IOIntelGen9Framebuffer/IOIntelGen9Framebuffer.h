#pragma once

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>

struct LilGpu;

class IOIntelGen9Framebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(IOIntelGen9Framebuffer);

private:
    IOPCIDevice *fPCIDevice;

    LilGpu      *fGpu;           // owned by lil (Base::operator new -> lil_malloc)

    // Active scanout geometry, filled after modeset.
    uint32_t     fWidth;
    uint32_t     fHeight;
    uint32_t     fPitch;
    uint64_t     fFbPhys;        // CPU-physical of the framebuffer aperture
    uint8_t     *fFbBase;        // CPU-virtual (stolen/BAR2 aperture)

    // Backing pages for the scanout, mapped into the GTT at fGpuFbAddr and
    // presented to the CPU via the BAR2 aperture at the same offset.
    IOBufferMemoryDescriptor *fFbBacking;
    uint32_t     fGpuFbAddr;     // GpuAddr (GTT offset) of the scanout surface

    bool         lilBringup(void);
    bool         mapScanoutGtt(void);
    uint16_t     findPchDeviceId(void);

public:
    virtual IOService *probe(IOService *provider, SInt32 *score) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;

    virtual IOReturn enableController() override;

    virtual const char *getPixelFormats() override;
    virtual IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode,
                                           IOIndex *depth) override;
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;

    virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory *getVRAMRange(void) override;

    virtual IOReturn getInformationForDisplayMode(
        IODisplayModeID displayMode, IODisplayModeInformation *info) override;
    virtual UInt64 getPixelFormatsForDisplayMode(
        IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOReturn getPixelInformation(
        IODisplayModeID displayMode, IOIndex depth,
        IOPixelAperture aperture, IOPixelInformation *info) override;
    virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
    virtual IOItemCount getDisplayModeCount(void) override;
};
