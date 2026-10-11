#pragma once

#include <IOKit/IOPlatformExpert.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include <IOKit/pci/IOPCIDevice.h>

#include "PDRadeonCard.h"

// one framebuffer per crtc, the card state is shared between the two
class PDRadeonFramebuffer : public IOFramebuffer
{
    OSDeclareDefaultStructors(PDRadeonFramebuffer);

private:
    RadeonCard   *fCard;
    int           fCrtc;
    bool          fPrimary;

    RadeonMode    fModes[kRadeonMaxModes];
    uint32_t      fNumModes;
    uint32_t      fCurMode;
    bool          fScanning;
    bool          fFirmwareCarried;

    uint32_t      fWidth;
    uint32_t      fHeight;
    uint32_t      fPitch;
    IOIndex       fDepth;
    uint32_t      fFbOffset;
    uint64_t      fFbPhys;
    bool          fConsole;
    bool          fFormat565;

    IOFBInterruptProc fConnProc;
    OSObject     *fConnTarget;
    void         *fConnRef;
    bool          fConnEnabled;

    RadeonOutput *output(void);
    void          buildModes(RadeonOutput *o);
    bool          applyMode(uint32_t index);
    void          publishConsole(void);
    void          notifyConnect(void);
    void          fillBlack(void);
    void          fillFromFirmware(void);
    void          configureScanoutPriority(void);
    void          drawTestPattern(void);
    bool          setupHead(IOPCIDevice *pci);
    void          cursorApply(void);
    void          fillTestCursor(void);

    uint8_t      *fCurTmp;
    uint32_t      fCurW, fCurH;
    int32_t       fCurX, fCurY;
    bool          fCurVisible;
    bool          fCurValid;

public:
    bool presentBuffer(uint32_t offset, uint32_t width, uint32_t height, uint32_t stride);
    bool scanoutInfo(uint32_t *width, uint32_t *height, uint32_t *stride, uint32_t *offset,
                     uint32_t *bitsPerPixel = NULL) {
        if (!fPrimary || !fScanning || fDepth > 1) return false;
        *width=fWidth; *height=fHeight; *stride=fPitch; *offset=fFbOffset;
        if (bitsPerPixel) *bitsPerPixel = fDepth == 1 ? 16 : 32;
        return true;
    }
    RadeonCard   *renderCard(void) { return fPrimary && fCard && fCard->accelReady ? fCard : NULL; }
    IOReturn newUserClient(task_t task, void *token, UInt32 type, IOUserClient **client) override;
    void          selfTestStep(uint32_t tick);
    static void   outputChanged(RadeonCard *card, int outputIndex);

    virtual IOService *probe(IOService *provider, SInt32 *score) override;
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;

    virtual IOReturn enableController() override;

    virtual const char *getPixelFormats() override;
    virtual IOReturn getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth) override;
    virtual IOReturn setDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;

    virtual IODeviceMemory *getApertureRange(IOPixelAperture aperture) override;
    virtual IODeviceMemory *getVRAMRange(void) override;

    virtual IOReturn getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info) override;
    virtual IOReturn getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation *info) override;
    virtual UInt64 getPixelFormatsForDisplayMode(IODisplayModeID displayMode, IOIndex depth) override;
    virtual IOReturn getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                         IOPixelAperture aperture, IOPixelInformation *info) override;
    virtual IOReturn getDisplayModes(IODisplayModeID *allDisplayModes) override;
    virtual IOItemCount getDisplayModeCount(void) override;

    virtual IOItemCount getConnectionCount(void) override;
    virtual IOReturn getAttributeForConnection(IOIndex connectIndex, IOSelect attribute, uintptr_t *value) override;
    virtual bool hasDDCConnect(IOIndex connectIndex) override;
    virtual IOReturn getDDCBlock(IOIndex connectIndex, UInt32 blockNumber, IOSelect blockType,
                                 IOOptionBits options, UInt8 *data, IOByteCount *length) override;
    virtual IOReturn getAttribute(IOSelect attribute, uintptr_t *value) override;
    virtual IOReturn setCursorImage(void *cursorImage) override;
    virtual IOReturn setCursorState(SInt32 x, SInt32 y, bool visible) override;
    virtual IOReturn registerForInterruptType(IOSelect interruptType, IOFBInterruptProc proc,
                                              OSObject *target, void *ref, void **interruptRef) override;
    virtual IOReturn unregisterInterrupt(void *interruptRef) override;
    virtual IOReturn setInterruptState(void *interruptRef, UInt32 state) override;
};
