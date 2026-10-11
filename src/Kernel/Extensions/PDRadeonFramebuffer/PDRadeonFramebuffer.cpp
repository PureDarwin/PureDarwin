#include "PDRadeonFramebuffer.h"

#include <IOKit/IODeviceMemory.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <pexpert/pexpert.h>

extern "C" boolean_t PE_parse_boot_argn(const char *, void *, int);
extern "C" int switch_to_video_console(void);

#define super IOFramebuffer
OSDefineMetaClassAndStructors(PDRadeonFramebuffer, IOFramebuffer);

#define kDepth 0
#define kDepth16 1
#define RGB565Pixels "RRRRRGGGGGGBBBBB"

#define kCursorOffset 0xe00000u

static inline uint32_t modePitch(const RadeonMode &m, IOIndex depth = 0) { return (m.hdisp * (depth == kDepth16 ? 2 : 4) + 255) & ~255u; }

// output currently bound to this crtc
RadeonOutput *PDRadeonFramebuffer::output(void)
{
    for (int i = 0; i < fCard->nOut; i++)
        if (fCard->out[i].head == fCrtc && fCard->out[i].present)
            return &fCard->out[i];
    return NULL;
}

void PDRadeonFramebuffer::buildModes(RadeonOutput *o)
{
    int pref = 0;

    fNumModes = radeonEdidModes(o && o->hasEdid ? o->edid : NULL, fModes, kRadeonMaxModes, &pref);
    fCurMode = pref;
    LOG("crtc%d: %u modes, preferred %ux%u@%u khz%s\n", fCrtc + 1, fNumModes, fModes[pref].hdisp, fModes[pref].vdisp,
        fModes[pref].clock, o && o->hasEdid ? " (edid)" : " (built in)");
}

void PDRadeonFramebuffer::fillBlack(void)
{
    volatile uint32_t *p = (volatile uint32_t *)(fCard->vramCpu + fFbOffset);
    uint32_t words = (fPitch * fHeight) / 4;

    for (uint32_t i = 0; i < words; i++)
        p[i] = 0;
}

// the first modeset keeps what firmware showed (the boot logo), centred, later ones start black
void PDRadeonFramebuffer::fillFromFirmware(void)
{
    PE_Video fw;
    uint8_t *dst = (uint8_t *)(fCard->vramCpu + fFbOffset);

    bzero(&fw, sizeof(fw));
    fFirmwareCarried = true;
    if (fDepth == kDepth16 || PE_current_console(&fw) != 0 || !fw.v_baseAddr || fw.v_depth != 32 || !fw.v_width || !fw.v_height) {
        fillBlack();
        return;
    }

    uint64_t src = fw.v_baseAddr & ~1ULL, srcEnd = src + (uint64_t)fw.v_rowBytes * fw.v_height;
    uint64_t dstEnd = fFbPhys + (uint64_t)fPitch * fHeight;

    // already the same picture in the same place
    if (src == fFbPhys && fw.v_rowBytes == fPitch && fw.v_width == fWidth && fw.v_height == fHeight)
        return;
    if (src < dstEnd && fFbPhys < srcEnd) {
        fillBlack();
        return;
    }

    IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(src, srcEnd - src, kIODirectionIn);
    IOMemoryMap *map = md ? md->map() : NULL;

    fillBlack();
    if (map) {
        uint8_t *from = (uint8_t *)map->getVirtualAddress();
        uint32_t w = fw.v_width < fWidth ? fw.v_width : fWidth;
        uint32_t h = fw.v_height < fHeight ? fw.v_height : fHeight;
        uint32_t dx = (fWidth - w) / 2, dy = (fHeight - h) / 2;
        uint32_t sx = (fw.v_width - w) / 2, sy = (fw.v_height - h) / 2;

        for (uint32_t y = 0; y < h; y++)
            memcpy(dst + (dy + y) * fPitch + dx * 4, from + (sy + y) * fw.v_rowBytes + sx * 4, w * 4);
        map->release();
    }
    if (md)
        md->release();
}

void PDRadeonFramebuffer::drawTestPattern(void)
{
    volatile uint32_t *fbp = (volatile uint32_t *)(fCard->vramCpu + fFbOffset);
    static const uint32_t bars[8] = { 0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x000000 };

    for (uint32_t y = 0; y < fHeight; y++) {
        volatile uint32_t *row = fbp + (y * fPitch) / 4;
        for (uint32_t x = 0; x < fWidth; x++) {
            if (y < fHeight * 2 / 3) {
                uint32_t c = bars[x * 8 / fWidth];
                if (fDepth == kDepth16) ((volatile uint16_t *)row)[x] = ((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f);
                else row[x] = c;
            } else {
                uint32_t g = x * 255 / fWidth;
                if (fDepth == kDepth16) ((volatile uint16_t *)row)[x] = ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
                else row[x] = (g << 16) | (g << 8) | g;
            }
        }
    }
}

void PDRadeonFramebuffer::publishConsole(void)
{
    IOPlatformExpert *pe = getPlatform();
    if (!pe)
        return;

    PE_Video info;
    bzero(&info, sizeof(info));
    info.v_baseAddr = (unsigned long)fFbPhys | 1;
    info.v_width = fWidth;
    info.v_height = fHeight;
    info.v_depth = fDepth == kDepth16 ? 16 : 32;
    info.v_rowBytes = fPitch;
    info.v_display = GRAPHICS_MODE;
    info.v_scale = kPEScaleFactor1x;

    // drop the old text buffers before initialize_screen changes the geometry,
    // since gc_reset_tabs uses the new column count and the narrower old buffer would be overrun
    pe->setConsoleInfo(NULL, kPEReleaseScreen);
    pe->setConsoleInfo(&info, kPEGraphicsMode);

    // verbose boots get text, the rest keep the logo and progress bar
    bool wantConsole = PE_parse_boot_argn("-v", NULL, 0);
    PE_parse_boot_argn("gopconsole", &wantConsole, sizeof(wantConsole));
    if (wantConsole) {
        switch_to_video_console();
        pe->setConsoleInfo(NULL, kPEAcquireScreen);
        pe->setConsoleInfo(NULL, kPETextScreen);
    }
}

void PDRadeonFramebuffer::notifyConnect(void)
{
    LOG("crtc%d: connection change %s\n", fCrtc + 1, fConnProc && fConnEnabled ? "delivered" : "dropped, nobody registered");
    if (fConnProc && fConnEnabled)
        fConnProc(fConnTarget, fConnRef);
}

// caller holds the card lock
bool PDRadeonFramebuffer::applyMode(uint32_t index)
{
    RadeonOutput *o = output();
    const RadeonMode &m = fModes[index];
    bool ok = true;

    fCurMode = index;
    fWidth = m.hdisp;
    fHeight = m.vdisp;
    fPitch = modePitch(m, fDepth);
    fFbPhys = fCard->vramBase + fFbOffset;

    if (!o) {
        fScanning = false;
        return true;
    }

    if (fFirmwareCarried)
        fillBlack();
    else
        fillFromFirmware();
    ok = fCard->programHead(fCrtc, *o, m, fFbOffset, fPitch, fDepth == kDepth16 ? (fFormat565 ? 16 : 15) : 32);
    fScanning = ok;
    if (ok) {
        configureScanoutPriority();
        cursorApply();
    }
    return ok;
}

bool PDRadeonFramebuffer::setupHead(IOPCIDevice *pci)
{
    RadeonOutput *o;

    // 16 bpp by default, this board's memory only sustains 32 bpp 1080p while the gpu is idle
    uint32_t bpp = 16;
    OSNumber *dnum = OSDynamicCast(OSNumber, getProperty("DefaultDepth"));
    if (dnum)
        bpp = dnum->unsigned32BitValue();
    PE_parse_boot_argn("rv730_depth", &bpp, sizeof(bpp));
    fDepth = bpp == 32 ? kDepth : kDepth16;
    fFormat565 = false;
    fFbOffset = 0x100000 + fCrtc * kRadeonHeadVram;

    IOLockLock(fCard->lock);
    o = output();
    buildModes(o);
    bool ok = applyMode(fCurMode);
    IOLockUnlock(fCard->lock);

    if (o && !ok) {
        LOG("crtc%d did not come up\n", fCrtc + 1);
        if (fPrimary)
            return false;
    }
    if (fScanning)
        drawTestPattern();

    if (!super::start(pci)) {
        LOG("IOFramebuffer::start failed for crtc%d\n", fCrtc + 1);
        return false;
    }

    if (fScanning && !fCard->consoleHead) {
        fCard->consoleHead = this;
        fConsole = true;
        publishConsole();
    }

    LOG("crtc%d: %ux%u %s, output %s\n", fCrtc + 1, fWidth, fHeight, fScanning ? "scanning out" : "idle",
        o ? "connected" : "none");
    registerService();
    return true;
}

IOService *PDRadeonFramebuffer::probe(IOService *provider, SInt32 *score)
{
    return super::probe(provider, score);
}

bool PDRadeonFramebuffer::start(IOService *provider)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);

    if (!pci)
        return false;

    if (!fCard) {
        // first instance owns the hardware
        OSBoolean *prop;
        bool runInit, trace;
        int firstCrtc = 0;
        bool selfTest, busMaster;
        int slack = 4096;
        OSNumber *num;

        fPrimary = true;
        fCrtc = 0;

        pci->retain();
        pci->open(this);
        pci->setMemoryEnable(true);
        pci->setIOEnable(true);

        prop = OSDynamicCast(OSBoolean, getProperty("RunAsicInit"));
        runInit = prop && prop->isTrue();
        PE_parse_boot_argn("rv730_init", &runInit, sizeof(runInit));
        prop = OSDynamicCast(OSBoolean, getProperty("AtomTrace"));
        trace = prop && prop->isTrue();
        PE_parse_boot_argn("rv730_trace", &trace, sizeof(trace));

        prop = OSDynamicCast(OSBoolean, getProperty("BusMaster"));
        busMaster = prop && prop->isTrue();
        pci->setBusMasterEnable(busMaster);
        num = OSDynamicCast(OSNumber, getProperty("ScratchSlack"));
        if (num)
            slack = num->unsigned32BitValue();

        prop = OSDynamicCast(OSBoolean, getProperty("SelfTest"));
        selfTest = prop && prop->isTrue();
        PE_parse_boot_argn("rv730_selftest", &selfTest, sizeof(selfTest));

        num = OSDynamicCast(OSNumber, getProperty("FirstCrtc"));
        if (num)
            firstCrtc = num->unsigned32BitValue() ? 1 : 0;

        fCard = (RadeonCard *)IOMallocZero(sizeof(RadeonCard));
        if (!fCard || !fCard->init(this, pci, runInit, trace, firstCrtc, selfTest, slack)) {
            if (fCard) {
                fCard->teardown();
                IOFree(fCard, sizeof(RadeonCard));
                fCard = NULL;
            }
            pci->close(this);
            pci->release();
            return false;
        }
        fCard->heads[0] = this;
    }

    if (!setupHead(pci)) {
        if (fPrimary) {
            fCard->teardown();
            IOFree(fCard, sizeof(RadeonCard));
            fCard = NULL;
            pci->close(this);
            pci->release();
        }
        return false;
    }

    if (fPrimary) {
        PDRadeonFramebuffer *second = OSTypeAlloc(PDRadeonFramebuffer);

        if (second && second->init()) {
            second->fCard = fCard;
            second->fCrtc = 1;
            fCard->heads[1] = second;
            if (second->attach(provider) && second->start(provider)) {
                second->release();
            } else {
                LOG("second head failed to start\n");
                fCard->heads[1] = NULL;
                second->detach(provider);
                second->release();
            }
        }
        fCard->accelInit();
        fCard->pollStart();
    }

    return true;
}

void PDRadeonFramebuffer::stop(IOService *provider)
{
    super::stop(provider);
}

// connect or disconnect, bind the output to a crtc, program it and tell the framebuffer
void PDRadeonFramebuffer::outputChanged(RadeonCard *card, int idx)
{
    PDRadeonFramebuffer *notify = NULL;

    IOLockLock(card->lock);
    RadeonOutput &o = card->out[idx];

    if (o.present) {
        if (o.head < 0) {
            for (int c = 0; c < 2 && o.head < 0; c++) {
                bool used = false;
                for (int k = 0; k < card->nOut; k++)
                    if (card->out[k].head == c)
                        used = true;
                if (!used && card->heads[c])
                    o.head = c;
            }
        }
        if (o.head >= 0) {
            PDRadeonFramebuffer *h = card->heads[o.head];
            LOG("connector 0x%04x -> crtc %d\n", o.connId, o.head + 1);
            h->buildModes(&o);
            h->applyMode(h->fCurMode);
            if (h->fConsole && h->fScanning)
                h->publishConsole();
            notify = h;
        } else {
            LOG("connector 0x%04x has no free crtc\n", o.connId);
        }
    } else if (o.head >= 0) {
        PDRadeonFramebuffer *h = card->heads[o.head];
        card->disableHead(o.head, o);
        h->fScanning = false;
        o.head = -1;
        h->buildModes(NULL);
        h->applyMode(h->fCurMode);
        notify = h;
    }

    IOLockUnlock(card->lock);

    if (notify)
        notify->notifyConnect();
}

IOReturn PDRadeonFramebuffer::enableController() { return kIOReturnSuccess; }

const char *PDRadeonFramebuffer::getPixelFormats() { return IO32BitDirectPixels "\0" RGB565Pixels "\0"; }

IOReturn PDRadeonFramebuffer::getCurrentDisplayMode(IODisplayModeID *displayMode, IOIndex *depth)
{
    *displayMode = fCurMode + 1;
    *depth = fDepth;
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::setDisplayMode(IODisplayModeID displayMode, IOIndex depth)
{
    uint32_t index = displayMode - 1;

    if (depth > kDepth16 || displayMode < 1 || index >= fNumModes)
        return kIOReturnUnsupportedMode;

    if (index == fCurMode && depth == fDepth && (fScanning || !output()))
        return kIOReturnSuccess;

    IOLockLock(fCard->lock);
    IOIndex oldDepth = fDepth;
    uint32_t oldMode = fCurMode;
    fDepth = depth;
    bool ok = applyMode(index);
    if (!ok) { fDepth = oldDepth; applyMode(oldMode); }
    if (ok && fConsole && fScanning)
        publishConsole();
    IOLockUnlock(fCard->lock);

    return ok ? kIOReturnSuccess : kIOReturnError;
}

IODeviceMemory *PDRadeonFramebuffer::getApertureRange(IOPixelAperture)
{
    return IODeviceMemory::withRange((mach_vm_address_t)fFbPhys, (IOByteCount)fPitch * fHeight);
}

IODeviceMemory *PDRadeonFramebuffer::getVRAMRange(void)
{
    return getApertureRange(kIOFBSystemAperture);
}

IOReturn PDRadeonFramebuffer::getInformationForDisplayMode(IODisplayModeID displayMode, IODisplayModeInformation *info)
{
    uint32_t index = displayMode - 1;

    if (displayMode < 1 || index >= fNumModes)
        return kIOReturnUnsupportedMode;

    const RadeonMode &m = fModes[index];
    bzero(info, sizeof(*info));
    info->nominalWidth = m.hdisp;
    info->nominalHeight = m.vdisp;
    info->refreshRate = (IOFixed1616)((((uint64_t)m.clock * 1000) << 16) / ((uint64_t)m.htotal * m.vtotal));
    info->maxDepthIndex = kDepth16;
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::getTimingInfoForDisplayMode(IODisplayModeID displayMode, IOTimingInformation *info)
{
    uint32_t index = displayMode - 1;

    if (displayMode < 1 || index >= fNumModes)
        return kIOReturnUnsupportedMode;

    const RadeonMode &m = fModes[index];
    bzero(info, sizeof(*info));
    info->appleTimingID = kIOTimingIDInvalid;
    info->flags = kIODetailedTimingValid;

    IODetailedTimingInformationV2 &d = info->detailedInfo.v2;
    d.pixelClock = (uint64_t)m.clock * 1000;
    d.minPixelClock = d.pixelClock;
    d.maxPixelClock = d.pixelClock;
    d.horizontalActive = m.hdisp;
    d.horizontalBlanking = m.htotal - m.hdisp;
    d.horizontalSyncOffset = m.hsyncStart - m.hdisp;
    d.horizontalSyncPulseWidth = m.hsyncEnd - m.hsyncStart;
    d.verticalActive = m.vdisp;
    d.verticalBlanking = m.vtotal - m.vdisp;
    d.verticalSyncOffset = m.vsyncStart - m.vdisp;
    d.verticalSyncPulseWidth = m.vsyncEnd - m.vsyncStart;
    d.horizontalSyncConfig = m.nhsync ? 0 : kIOSyncPositivePolarity;
    d.verticalSyncConfig = m.nvsync ? 0 : kIOSyncPositivePolarity;
    return kIOReturnSuccess;
}

UInt64 PDRadeonFramebuffer::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
    return 0;
}

IOReturn PDRadeonFramebuffer::getPixelInformation(IODisplayModeID displayMode, IOIndex depth,
                                                  IOPixelAperture aperture, IOPixelInformation *info)
{
    uint32_t index = displayMode - 1;

    if (aperture || depth > kDepth16 || displayMode < 1 || index >= fNumModes)
        return kIOReturnUnsupportedMode;

    // the first client asking about 16 bit pixels takes over the screen, switch from the console's 1555 to 565
    if (depth == kDepth16 && fDepth == kDepth16 && !fFormat565 && fScanning) {
        IOLockLock(fCard->lock);
        fFormat565 = true;
        fCard->wreg(0x6104 + (fCrtc ? 0x800 : 0), 1u | (1u << 8));
        LOG("crtc%d: 16 bit scanout switched to rgb565\n", fCrtc + 1);
        IOLockUnlock(fCard->lock);
    }
    bzero(info, sizeof(*info));
    info->activeWidth = fModes[index].hdisp;
    info->activeHeight = fModes[index].vdisp;
    info->bytesPerRow = modePitch(fModes[index], depth);
    info->bytesPerPlane = 0;
    strlcpy(info->pixelFormat, depth == kDepth16 ? RGB565Pixels : IO32BitDirectPixels, sizeof(info->pixelFormat));
    info->pixelType = kIORGBDirectPixels;
    info->componentMasks[0] = depth == kDepth16 ? 0xf800 : 0x00ff0000;
    info->componentMasks[1] = depth == kDepth16 ? 0x07e0 : 0x0000ff00;
    info->componentMasks[2] = depth == kDepth16 ? 0x001f : 0x000000ff;
    info->bitsPerPixel = depth == kDepth16 ? 16 : 32;
    info->componentCount = 3;
    info->bitsPerComponent = depth == kDepth16 ? 6 : 8;
    return kIOReturnSuccess;
}

IOItemCount PDRadeonFramebuffer::getDisplayModeCount(void) { return fNumModes; }

IOReturn PDRadeonFramebuffer::getDisplayModes(IODisplayModeID *all)
{
    for (uint32_t i = 0; i < fNumModes; i++)
        all[i] = i + 1;
    return kIOReturnSuccess;
}

IOItemCount PDRadeonFramebuffer::getConnectionCount(void) { return 1; }

IOReturn PDRadeonFramebuffer::getAttributeForConnection(IOIndex, IOSelect attribute, uintptr_t *value)
{
    switch (attribute) {
    case kConnectionSupportsHLDDCSense:
        return kIOReturnSuccess;
    case kConnectionFlags:
        *value = 0;
        return kIOReturnSuccess;
    }
    return kIOReturnUnsupported;
}

bool PDRadeonFramebuffer::hasDDCConnect(IOIndex)
{
    RadeonOutput *o = output();
    return o && o->hasEdid;
}

IOReturn PDRadeonFramebuffer::getDDCBlock(IOIndex, UInt32 blockNumber, IOSelect blockType,
                                          IOOptionBits, UInt8 *data, IOByteCount *length)
{
    RadeonOutput *o = output();

    if (!o || !o->hasEdid || blockType != kIODDCBlockTypeEDID || blockNumber != 1)
        return kIOReturnUnsupported;

    IOByteCount n = *length < 128 ? *length : 128;
    memcpy(data, o->edid, n);
    *length = n;
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::registerForInterruptType(IOSelect type, IOFBInterruptProc proc,
                                                       OSObject *target, void *ref, void **interruptRef)
{
    if (type != kIOFBConnectInterruptType)
        return kIOReturnUnsupported;

    LOG("crtc%d: connect interrupt registered\n", fCrtc + 1);
    fConnProc = proc;
    fConnTarget = target;
    fConnRef = ref;
    fConnEnabled = true;
    *interruptRef = this;
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::unregisterInterrupt(void *interruptRef)
{
    if (interruptRef != this)
        return kIOReturnBadArgument;

    fConnProc = NULL;
    fConnEnabled = false;
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::setInterruptState(void *interruptRef, UInt32 state)
{
    if (interruptRef != this)
        return kIOReturnBadArgument;

    fConnEnabled = state != 0;
    return kIOReturnSuccess;
}

// the cursor buffer lives at the top of this head's vram slice
void PDRadeonFramebuffer::cursorApply(void)
{
    int both = 0;

    for (int c = 0; c < 2; c++)
        if (fCard->heads[c] && fCard->heads[c]->fScanning)
            both++;

    if (!fScanning || !fCurValid)
        return;

    fCard->cursorProgram(fCrtc, fFbOffset + kCursorOffset, fCurVisible, fCurX, fCurY, fCurW, fCurH,
                         fWidth, fHeight, both > 1);
}

IOReturn PDRadeonFramebuffer::getAttribute(IOSelect attribute, uintptr_t *value)
{
    if (attribute == kIOHardwareCursorAttribute) {
        *value = 1;
        return kIOReturnSuccess;
    }
    return super::getAttribute(attribute, value);
}

IOReturn PDRadeonFramebuffer::setCursorImage(void *cursorImage)
{
    IOHardwareCursorDescriptor desc;
    IOHardwareCursorInfo info;

    if (!fCurTmp)
        fCurTmp = (uint8_t *)IOMallocZero(64 * 64 * 4);
    if (!fCurTmp)
        return kIOReturnNoMemory;

    bzero(&desc, sizeof(desc));
    desc.majorVersion = kHardwareCursorDescriptorMajorVersion;
    desc.minorVersion = kHardwareCursorDescriptorMinorVersion;
    desc.height = 64;
    desc.width = 64;
    desc.bitDepth = 32;

    bzero(&info, sizeof(info));
    info.majorVersion = kHardwareCursorInfoMajorVersion;
    info.minorVersion = kHardwareCursorInfoMinorVersion;
    info.hardwareCursorData = fCurTmp;

    if (!convertCursorImage(cursorImage, &desc, &info)) {
        // testing pass or an image the hardware cannot show
        return kIOReturnUnsupported;
    }

    uint32_t w = info.cursorWidth, h = info.cursorHeight;
    if (!w || !h || w > 64 || h > 64)
        return kIOReturnUnsupported;

    volatile uint32_t *dst = (volatile uint32_t *)(fCard->vramCpu + fFbOffset + kCursorOffset);
    const uint32_t *src = (const uint32_t *)fCurTmp;

    // the hardware wants premultiplied alpha
    for (uint32_t y = 0; y < 64; y++) {
        for (uint32_t x = 0; x < 64; x++) {
            uint32_t p = 0;
            if (x < w && y < h) {
                uint32_t s = src[y * w + x];
                uint32_t a = s >> 24;
                uint32_t r = ((s >> 16) & 0xff) * a / 255;
                uint32_t g = ((s >> 8) & 0xff) * a / 255;
                uint32_t b = (s & 0xff) * a / 255;
                p = (a << 24) | (r << 16) | (g << 8) | b;
            }
            dst[y * 64 + x] = p;
        }
    }

    fCurW = w;
    fCurH = h;
    fCurValid = true;

    IOLockLock(fCard->lock);
    cursorApply();
    IOLockUnlock(fCard->lock);
    return kIOReturnSuccess;
}

IOReturn PDRadeonFramebuffer::setCursorState(SInt32 x, SInt32 y, bool visible)
{
    fCurX = x;
    fCurY = y;
    fCurVisible = visible;

    IOLockLock(fCard->lock);
    cursorApply();
    IOLockUnlock(fCard->lock);
    return kIOReturnSuccess;
}

void PDRadeonFramebuffer::fillTestCursor(void)
{
    volatile uint32_t *dst = (volatile uint32_t *)(fCard->vramCpu + fFbOffset + kCursorOffset);

    for (uint32_t y = 0; y < 64; y++)
        for (uint32_t x = 0; x < 64; x++) {
            uint32_t p = 0;
            if (x < 32 && y < 32)
                p = (x < 3 || y < 3 || x > 28 || y > 28) ? 0xffffffff : 0xffff0000;
            dst[y * 64 + x] = p;
        }
    fCurW = fCurH = 32;
    fCurValid = true;
}

// boot time self test, mode cycle then a jumping cursor, only with the SelfTest property
void PDRadeonFramebuffer::selfTestStep(uint32_t tick)
{
    if (!fScanning || !output())
        return;

    if (tick == 14) {
        fillTestCursor();
        LOG("selftest crtc%d: hardware cursor on\n", fCrtc + 1);
    }

    if (tick >= 14 && tick < 40) {
        int32_t pos[6][2] = { { 100, 100 }, { (int32_t)fWidth - 140, 100 }, { (int32_t)fWidth / 2, (int32_t)fHeight / 2 },
                              { -10, -10 }, { (int32_t)fWidth - 20, (int32_t)fHeight - 20 }, { 300, (int32_t)fHeight - 100 } };
        int k = (tick - 14) % 6;
        setCursorState(pos[k][0], pos[k][1], true);
    }

    if (tick == 22 || tick == 30) {
        uint32_t target = fCurMode;
        if (tick == 22) {
            for (uint32_t i = 0; i < fNumModes; i++)
                if (fModes[i].hdisp != fModes[fCurMode].hdisp) {
                    target = i;
                    break;
                }
        } else {
            target = 0;
            for (uint32_t i = 0; i < fNumModes; i++)
                if (fModes[i].hdisp == 1920)
                    target = i;
        }
        LOG("selftest crtc%d: switching to mode %u (%ux%u)\n", fCrtc + 1, target + 1, fModes[target].hdisp, fModes[target].vdisp);
        IOReturn r = setDisplayMode(target + 1, fDepth);
        LOG("selftest crtc%d: setDisplayMode returned 0x%x\n", fCrtc + 1, r);
    }
}

/*
 * Copyright 2008 Advanced Micro Devices, Inc.
 * Copyright 2008 Red Hat Inc.
 * Copyright 2009 Jerome Glisse.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors: Dave Airlie
 *          Alex Deucher
 *          Jerome Glisse
 */

// caller holds the card lock. CPU-mapped scanout needs the same priority as flips.
void PDRadeonFramebuffer::configureScanoutPriority(void)
{
    uint32_t off=fCrtc ? 0x800 : 0;
    // RV730 uses the AVIVO display memory priorities, keeping display fetches serviced while rendering.
    // a linear surface requests one chunk per 1024 bytes with the full-line priority mark (16 pixels per unit), as rv515_bandwidth_update's single-display HIGH policy does
    unsigned width=fWidth;
    unsigned bytesPerPixel=fDepth == kDepth16 ? 2 : 4;
    unsigned depth=(width*bytesPerPixel+1023)/1024;
    if (depth<4) depth=4;
    uint32_t requests=fCard->rreg(0x6d58);
    unsigned shift=fCrtc ? 16 : 0;
    uint32_t newRequests=(requests&~(0xffffu<<shift))|(depth<<shift);
    if (!(newRequests&(0xffffu<<(16-shift)))) newRequests|=4u<<(16-shift);
    if (newRequests!=requests) {
        fCard->wreg(0x6d58,newRequests);
        LOG("display request depth 0x%08x -> 0x%08x\n",requests,fCard->rreg(0x6d58));
    }
    if (!fCrtc && !(fCard->rreg(0x6880)&1)) {
        uint32_t split=fCard->rreg(0x6520);
        uint32_t single=(split&~7u)|2u;
        if (split!=single) {
            fCard->wreg(0x6520,single);
            LOG("display line buffer 0x%08x -> 0x%08x\n",split,fCard->rreg(0x6520));
        }
    }
    for (unsigned i=0;i<2;i++) {
        uint32_t reg=0x6548+off+i*4;
        uint32_t old=fCard->rreg(reg);
        uint32_t priority=(old&~((1u<<16)|0x7fffu))|(1u<<20)|((width+15)/16);
        if (priority!=old) {
            fCard->wreg(reg,priority);
            LOG("crtc%d: display priority %c 0x%08x -> 0x%08x\n",
                fCrtc+1,'A'+i,old,fCard->rreg(reg));
        }
    }
}

// RV770 vsync page-flip sequence, serialized with mode setting and submissions.
bool PDRadeonFramebuffer::presentBuffer(uint32_t offset, uint32_t width, uint32_t height, uint32_t stride)
{
    if (!fPrimary || !fCard) return false;
    IOLockLock(fCard->lock);
    if ((fDepth != kDepth && fDepth != kDepth16) || !fScanning || width!=fWidth ||
        height!=fHeight || stride<width*(fDepth == kDepth16 ? 2 : 4) || (stride&255) || (offset&255)) {
        IOLockUnlock(fCard->lock); return false;
    }
    if (!offset) offset=fFbOffset;
    uint32_t off=fCrtc ? 0x800 : 0;
    uint32_t update=fCard->rreg(0x6144+off);
    fCard->wreg(0x6144+off,update|(1u<<16)); // graphics update lock
    fCard->wreg(0x6148+off,0); // vsync, not horizontal-retrace flips
    fCard->wreg(0x6120+off,stride/(fDepth == kDepth16 ? 2 : 4));
    fCard->wreg(fCrtc ? 0x6114 : 0x6914,0);
    fCard->wreg(fCrtc ? 0x611c : 0x691c,0);
    fCard->wreg(0x6118+off,offset);
    fCard->wreg(0x6110+off,offset);
    for (unsigned i=0;i<1000;i++) {
        if (fCard->rreg(0x6144+off)&4) break;
        IODelay(1);
    }
    fCard->wreg(0x6144+off,update&~(1u<<16));
    bool done=false;
    for (unsigned i=0;i<100;i++) {
        if (!(fCard->rreg(0x6144+off)&4)) { done=true; break; }
        IODelay(1000);
    }
    if (!done) LOG("crtc%d: page flip timeout update=0x%08x\n",fCrtc+1,fCard->rreg(0x6144+off));
    IOLockUnlock(fCard->lock);
    return done;
}
