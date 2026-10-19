#include <IOKit/IOPlatformExpert.h>
#include <IOKit/graphics/IOFramebuffer.h>

class IOGOPFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(IOGOPFramebuffer);

private:
    void   *fbBase;
    UInt32  width;
    UInt32  height;
    UInt32  pitch;
    UInt32  bpp;

public:
    IOService * probe(IOService * provider, SInt32 * score) override;
    virtual bool start(IOService * provider) override;
    virtual void stop(IOService * provider) override;
    virtual void * getBaseAddress();
    virtual uint32_t getWidth();
    virtual uint32_t getHeight();
    virtual uint32_t getPitch();
    virtual uint32_t getDepth();

    virtual IOReturn enableController() override;
    // refuses clients once a gpu driver's framebuffer runs the same gpu
    virtual IOReturn newUserClient(task_t owningTask, void *security_id, UInt32 type,
                                   IOUserClient **clientH) override;

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
