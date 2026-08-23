#include "RavynHDAudio.h"
#include "RavynHDAudioEngine.h"

#include <miscfs/devfs/devfs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/uio.h>
#include <IOKit/IOLib.h>
#include <IOKit/IODeviceMemory.h>

#define super IOAudioDevice
OSDefineMetaClassAndStructors(RavynHDAudio, IOAudioDevice);

// ---------------------------------------------------------------------
// HDA controller register offsets (Intel HD Audio spec, byte offsets from
// BAR0). Matches the layout Linux's sound/pci/hda/hda_controller.h
// (AZX_REG_*) uses; written from public spec knowledge, not derived from
// any Apple/Linux source.
// ---------------------------------------------------------------------
#define HDA_GCAP        0x00   // 16-bit RO: [15:12]=OSS [11:8]=ISS [7:3]=BSS [2:1]=NSDO [0]=64OK
#define HDA_VMIN        0x02
#define HDA_VMAJ        0x03
#define HDA_GCTL        0x08   // 32-bit: bit0=CRST
#define HDA_WAKEEN      0x0C
#define HDA_STATESTS    0x0E   // 16-bit RWC: bit per codec address present
#define HDA_INTCTL      0x20   // 32-bit: bit31=GIE bit30=CIE
#define HDA_INTSTS      0x24
#define HDA_CORBLBASE   0x40
#define HDA_CORBUBASE   0x44
#define HDA_CORBWP      0x48   // 16-bit
#define HDA_CORBRP      0x4A   // 16-bit, bit15=reset
#define HDA_CORBCTL     0x4C   // 8-bit: bit0=CMEIE bit1=CORBRUN (HDA spec 3.3.22)
#define HDA_CORBSTS     0x4D
#define HDA_CORBSIZE    0x4E   // 8-bit: [1:0]=size select
#define HDA_RIRBLBASE   0x50
#define HDA_RIRBUBASE   0x54
#define HDA_RIRBWP      0x58   // 16-bit, write bit15=1 to reset
#define HDA_RINTCNT     0x5A   // 16-bit: N responses before an interrupt
#define HDA_RINTCNT     0x5A
#define HDA_RIRBCTL     0x5C   // 8-bit: bit0=RINTCTL bit1=RIRBDMAEN
#define HDA_RIRBSTS     0x5D
#define HDA_RIRBSIZE    0x5E
#define HDA_SD_BASE     0x80
#define HDA_SD_SIZE     0x20

// Per-stream-descriptor offsets (relative to HDA_SD_BASE + n*HDA_SD_SIZE)
#define SD_CTL_STS      0x00   // 32-bit: [23:0]=CTL [31:24]=STS
#define SD_LPIB         0x04
#define SD_CBL          0x08
#define SD_LVI          0x0C   // 16-bit
#define SD_FORMAT       0x12   // 16-bit
#define SD_BDPL         0x18
#define SD_BDPU         0x1C

#define SD_CTL_SRST     (1u << 0)
#define SD_CTL_RUN      (1u << 1)
#define SD_CTL_IOCE     (1u << 2)
#define SD_CTL_STRM_SHIFT 20

// Codec verbs (see Intel HDA spec section 7.3). 12-bit-ID verbs carry an
// 8-bit payload in the low byte; 4-bit-ID verbs carry a 16-bit payload.
#define VERB_GET_PARAM              0xF00
#define VERB_SET_CONNECT_SELECT     0x701
#define VERB_SET_CHANNEL_STREAMID   0x706
#define VERB_SET_PIN_WIDGET_CTL     0x707
#define VERB_SET_POWER_STATE        0x705
#define VERB_SET_EAPD_BTL           0x70C
#define VERB_SET_STREAM_FORMAT_ID4  0x2   // 4-bit form: (0x2<<16)|format16
#define VERB_SET_AMP_GAIN_MUTE_ID4  0x3   // 4-bit form: (0x3<<16)|payload16

#define PARAM_NODE_COUNT       0x04
#define PARAM_FUNCTION_GROUP   0x05
#define PARAM_AUDIO_WIDGET_CAP 0x09
#define PARAM_PIN_CAP          0x0C

#define AW_TYPE_OUTPUT   0x0   // DAC
#define AW_TYPE_PIN      0x4   // Pin Complex

// 48000 Hz / 16-bit / stereo. Format field encoding (16-bit):
//   bit14: base (0=48kHz, 1=44.1kHz)
//   bits[13:11]: multiplier, bits[10:8]: divisor (000 = x1/rn1)
//   bits[6:4]: bits per sample (001=16-bit)
//   bits[3:0]: number of channels - 1
#define HDA_FORMAT_48K_16BIT_STEREO  0x0011

bool RavynHDAudio::init(OSDictionary *properties)
{
    if (!super::init(properties))
        return false;
    fPCIDevice = 0;
    fRegMap = 0;
    fRegDesc = 0;
    fRegs = 0;
    fCorbBuf = fRirbBuf = fBdlBuf = fPcmBuf = 0;
    fCorb = 0;
    fRirb = 0;
    fCorbWp = 0;
    fRirbRp = 0;
    fPcmVirt = 0;
    fPcmPhys = 0;
    fOutStreamIndex = 0;
    fWriteOffset = 0;
    fTotalWritten = 0;
    fConsumedBase = 0;
    fLastLpib = 0;
    fLock = IOSimpleLockAlloc();
    return fLock != 0;
}

void RavynHDAudio::free()
{
    if (fLock) {
        IOSimpleLockFree(fLock);
        fLock = 0;
    }
    super::free();
}

IOService *RavynHDAudio::probe(IOService *provider, SInt32 *score)
{
    IOPCIDevice *pci = OSDynamicCast(IOPCIDevice, provider);
    if (!pci)
        return NULL;

    uint16_t vendor = pci->configRead16(kIOPCIConfigVendorID);
    uint16_t device = pci->configRead16(kIOPCIConfigDeviceID);

    // QEMU's "-device intel-hda" (ICH6, 8086:2668) and "-device
    // ich9-intel-hda" (8086:293e). Also match by class code (Multimedia,
    // HD Audio Controller = 0x040300) so real Intel HDA silicon works too.
    bool knownId = (vendor == 0x8086 && (device == 0x2668 || device == 0x293E));
    uint32_t classCode = pci->configRead32(kIOPCIConfigRevisionID) >> 8;
    bool classMatch = (classCode == 0x040300);

    if (!knownId && !classMatch)
        return NULL;

    if (score)
        *score = 5000;
    return this;
}

uint32_t RavynHDAudio::sendVerb(uint8_t codecAddr, uint16_t nid, uint32_t verb, uint32_t payload)
{
    uint32_t cmd;
    if (verb <= 0xF)
        cmd = ((uint32_t)codecAddr << 28) | ((uint32_t)nid << 20) | (verb << 16) | (payload & 0xFFFF);
    else
        cmd = ((uint32_t)codecAddr << 28) | ((uint32_t)nid << 20) | ((verb & 0xFFF) << 8) | (payload & 0xFF);

    // Write the verb into the next CORB slot and advance CORBWP to kick
    // off the DMA fetch.
    fCorbWp = (fCorbWp + 1) % kHDACorbEntries;
    fCorb[fCorbWp] = cmd;
    wreg16(HDA_CORBWP, fCorbWp);

    // Poll RIRBWP for a new response. This is a simple synchronous
    // command interface (one outstanding verb at a time) - fine for
    // one-time enumeration/setup, not meant for a hot audio path.
    uint16_t rirbWp;
    int spins = 0;
    do {
        rirbWp = reg16(HDA_RIRBWP) & 0xFF;
        if (rirbWp != fRirbRp)
            break;
        IODelay(50);
    } while (++spins < 20000);

    if (rirbWp == fRirbRp) {
        IOLog("RavynHDAudio: sendVerb timeout cmd=0x%08x CORBRP=0x%04x CORBWP=0x%04x "
              "RIRBWP=0x%04x CORBCTL=0x%02x RIRBCTL=0x%02x CORBSTS=0x%02x RIRBSTS=0x%02x\n",
              cmd, reg16(HDA_CORBRP), reg16(HDA_CORBWP), reg16(HDA_RIRBWP),
              reg8(HDA_CORBCTL), reg8(HDA_RIRBCTL), reg8(HDA_CORBSTS), reg8(HDA_RIRBSTS));
        return 0;   // timed out; caller should treat 0 as "unknown"
    }

    fRirbRp = (fRirbRp + 1) % kHDARirbEntries;
    uint64_t entry = fRirb[fRirbRp];

    // Clear RIRBSTS.RINTFL (RW1C, bit0). On QEMU this is what resets the
    // internal response counter that RINTCNT gates CORB processing on
    // (see the comment in setupCorbRirb()) - without it, CORB fetching
    // stops dead again after the first response.
    wreg8(HDA_RIRBSTS, 0x01);

    return (uint32_t)(entry & 0xFFFFFFFFu);
}

bool RavynHDAudio::resetController()
{
    // GCTL.CRST: write 0 then 1 to force a full controller reset, then
    // wait for the bit to read back as 1 (out of reset).
    uint32_t gctl = reg32(HDA_GCTL);
    wreg32(HDA_GCTL, gctl & ~1u);
    IODelay(1000);
    wreg32(HDA_GCTL, gctl | 1u);

    int spins = 0;
    while (!(reg32(HDA_GCTL) & 1u) && spins < 1000) {
        IODelay(100);
        spins++;
    }
    if (!(reg32(HDA_GCTL) & 1u)) {
        IOLog("RavynHDAudio: controller reset timed out\n");
        return false;
    }

    // Give codecs time to report presence in STATESTS after reset.
    IOSleep(1);
    return true;
}

bool RavynHDAudio::setupCorbRirb()
{
    // Stop any running CORB/RIRB DMA before reprogramming.
    wreg8(HDA_CORBCTL, 0);
    wreg8(HDA_RIRBCTL, 0);

    fCorbBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        kHDACorbEntries * sizeof(uint32_t), 0xFFFFFFFFull);
    fRirbBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        kHDARirbEntries * sizeof(uint64_t), 0xFFFFFFFFull);
    if (!fCorbBuf || !fRirbBuf)
        return false;
    fCorbBuf->prepare();
    fRirbBuf->prepare();
    fCorb = (volatile uint32_t *)fCorbBuf->getBytesNoCopy();
    fRirb = (volatile uint64_t *)fRirbBuf->getBytesNoCopy();
    bzero((void *)fCorb, kHDACorbEntries * sizeof(uint32_t));
    bzero((void *)fRirb, kHDARirbEntries * sizeof(uint64_t));

    uint64_t corbPhys = fCorbBuf->getPhysicalAddress();
    uint64_t rirbPhys = fRirbBuf->getPhysicalAddress();

    wreg32(HDA_CORBLBASE, (uint32_t)(corbPhys & 0xFFFFFFFFu));
    wreg32(HDA_CORBUBASE, (uint32_t)(corbPhys >> 32));
    wreg32(HDA_RIRBLBASE, (uint32_t)(rirbPhys & 0xFFFFFFFFu));
    wreg32(HDA_RIRBUBASE, (uint32_t)(rirbPhys >> 32));

    // 32 CORB entries / 64 RIRB entries -> size-select value 1 ("16
    // entries" tier doesn't fit either count exactly on all controllers;
    // request the largest supported tier, 256 entries, and just use the
    // first kHDACorbEntries/kHDARirbEntries of it - simplest to reason
    // about and every controller we target supports 256).
    wreg8(HDA_CORBSIZE, 0x02);
    wreg8(HDA_RIRBSIZE, 0x02);

    wreg16(HDA_CORBRP, 0x8000);   // reset read pointer
    int spins = 0;
    while (!(reg16(HDA_CORBRP) & 0x8000) && spins++ < 1000) IODelay(10);
    wreg16(HDA_CORBRP, 0);
    fCorbWp = 0;
    wreg16(HDA_CORBWP, 0);

    wreg16(HDA_RIRBWP, 0x8000);   // reset write pointer
    fRirbRp = 0;

    // QEMU's intel-hda emulation (hw/audio/intel-hda.c, intel_hda_corb_run())
    // refuses to fetch ANY command from the CORB - even with CORBRUN set -
    // once its internal response counter reaches RINTCNT (both reset to 0,
    // so 0 == 0 blocks everything until this is set at least once). Set it
    // to 1: since sendVerb() only ever has one command outstanding at a
    // time, and clears RIRBSTS (which resets that internal counter back to
    // 0 in QEMU) after every response, this keeps the DMA engine
    // unblocked indefinitely rather than only for the first RINTCNT verbs.
    wreg16(HDA_RINTCNT, 1);

    // RIRBDMAEN (bit1) + RINTCTL (bit0). We don't wire up an actual
    // interrupt handler (still polling RIRBWP), but QEMU's emulation
    // (hw/audio/intel-hda.c intel_hda_response()) only ever sets
    // RIRBSTS.RINTFL when RINTCTL is enabled - and clearing that bit is
    // what resets its internal response counter that RINTCNT gates CORB
    // processing on. Without RINTCTL set here, our RIRBSTS-clear in
    // sendVerb() is clearing a bit that was never set, so the counter
    // reset never happens and every command after the first (when
    // RINTCNT=1) stalls forever.
    wreg8(HDA_RIRBCTL, 0x03);
    wreg8(HDA_CORBCTL, 0x02);     // CORBRUN (bit1), no interrupts

    IOLog("RavynHDAudio: CORB/RIRB setup: CORBSIZE=0x%02x RIRBSIZE=0x%02x "
          "CORBCTL=0x%02x RIRBCTL=0x%02x CORBRP=0x%04x CORBWP=0x%04x RIRBWP=0x%04x "
          "CORBLBASE=0x%08x RIRBLBASE=0x%08x GCTL=0x%08x INTCTL=0x%08x\n",
          reg8(HDA_CORBSIZE), reg8(HDA_RIRBSIZE),
          reg8(HDA_CORBCTL), reg8(HDA_RIRBCTL),
          reg16(HDA_CORBRP), reg16(HDA_CORBWP), reg16(HDA_RIRBWP),
          reg32(HDA_CORBLBASE), reg32(HDA_RIRBLBASE),
          reg32(HDA_GCTL), reg32(HDA_INTCTL));

    return true;
}

bool RavynHDAudio::programWidgets(uint8_t codecAddr, uint16_t afgNid)
{
    uint32_t nc = sendVerb(codecAddr, afgNid, VERB_GET_PARAM, PARAM_NODE_COUNT);
    uint16_t startNid = (nc >> 16) & 0xFF;
    uint16_t numNodes = nc & 0xFF;
    if (numNodes == 0) {
        IOLog("RavynHDAudio: AFG 0x%x reports no widgets\n", afgNid);
        return false;
    }

    uint16_t dacNid = 0, pinNid = 0;
    for (uint16_t nid = startNid; nid < startNid + numNodes; nid++) {
        uint32_t caps = sendVerb(codecAddr, nid, VERB_GET_PARAM, PARAM_AUDIO_WIDGET_CAP);
        uint32_t type = (caps >> 20) & 0xF;
        if (type == AW_TYPE_OUTPUT && !dacNid) {
            dacNid = nid;
        } else if (type == AW_TYPE_PIN && !pinNid) {
            uint32_t pincap = sendVerb(codecAddr, nid, VERB_GET_PARAM, PARAM_PIN_CAP);
            if (pincap & (1u << 4))   // Output Capable
                pinNid = nid;
        }
    }

    if (!dacNid || !pinNid) {
        IOLog("RavynHDAudio: no DAC/output-pin pair found (dac=0x%x pin=0x%x)\n", dacNid, pinNid);
        return false;
    }

    IOLog("RavynHDAudio: using DAC nid=0x%x -> pin nid=0x%x\n", dacNid, pinNid);

    // Power both widgets to D0 (full power).
    sendVerb(codecAddr, dacNid, VERB_SET_POWER_STATE, 0);
    sendVerb(codecAddr, pinNid, VERB_SET_POWER_STATE, 0);

    // Point the pin's input selector at the DAC. Most simple codecs
    // (including QEMU's emulated one) have exactly one entry in the pin's
    // connection list, so index 0 is correct without walking
    // GET_CONNECT_LIST.
    sendVerb(codecAddr, pinNid, VERB_SET_CONNECT_SELECT, 0);

    // Enable the pin as an analog output (OUT bit) and turn on the
    // external amp (EAPD) if the codec has one - harmless no-op if not.
    sendVerb(codecAddr, pinNid, VERB_SET_PIN_WIDGET_CTL, 0x40);
    sendVerb(codecAddr, pinNid, VERB_SET_EAPD_BTL, 0x02);

    // Unmute the DAC's output amp, 0 dB gain, both channels.
    sendVerb(codecAddr, dacNid, VERB_SET_AMP_GAIN_MUTE_ID4, 0xB000);
    sendVerb(codecAddr, pinNid, VERB_SET_AMP_GAIN_MUTE_ID4, 0xB000);

    // Route stream tag 1 (matches SD_CTL's stream number field, set in
    // setupOutputStream) / channel 0 to the DAC, and program its format.
    sendVerb(codecAddr, dacNid, VERB_SET_CHANNEL_STREAMID, 0x10);   // stream=1, channel=0
    sendVerb(codecAddr, dacNid, VERB_SET_STREAM_FORMAT_ID4, HDA_FORMAT_48K_16BIT_STEREO);

    return true;
}

bool RavynHDAudio::enumerateCodec(uint8_t codecAddr)
{
    uint32_t vendorId = sendVerb(codecAddr, 0, VERB_GET_PARAM, 0x00);
    uint32_t nc = sendVerb(codecAddr, 0, VERB_GET_PARAM, PARAM_NODE_COUNT);
    uint16_t startNid = (nc >> 16) & 0xFF;
    uint16_t numNodes = nc & 0xFF;
    IOLog("RavynHDAudio: codec %u vendorId=0x%08x root nodeCountRaw=0x%08x start=0x%x count=%u\n",
          codecAddr, vendorId, nc, startNid, numNodes);

    for (uint16_t nid = startNid; nid < startNid + numNodes; nid++) {
        uint32_t fg = sendVerb(codecAddr, nid, VERB_GET_PARAM, PARAM_FUNCTION_GROUP);
        IOLog("RavynHDAudio:   nid=0x%x functionGroupRaw=0x%08x\n", nid, fg);
        if ((fg & 0xFF) == 0x01) {   // Audio Function Group
            IOLog("RavynHDAudio: codec %u AFG nid=0x%x\n", codecAddr, nid);
            if (programWidgets(codecAddr, nid))
                return true;
        }
    }
    return false;
}

bool RavynHDAudio::setupOutputStream()
{
    uint16_t gcap = reg16(HDA_GCAP);
    uint32_t iss = (gcap >> 8) & 0xF;
    uint32_t oss = (gcap >> 12) & 0xF;
    if (oss == 0) {
        IOLog("RavynHDAudio: controller reports zero output streams\n");
        return false;
    }
    fOutStreamIndex = iss;   // first output stream follows all input streams

    uint32_t sdBase = HDA_SD_BASE + fOutStreamIndex * HDA_SD_SIZE;

    // Reset the stream descriptor: set SRST, wait for it to latch, clear
    // it, wait for it to clear.
    uint32_t ctl = *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS);
    *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) = ctl | SD_CTL_SRST;
    int spins = 0;
    while (!(*(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) & SD_CTL_SRST) && spins++ < 1000)
        IODelay(10);
    *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) &= ~SD_CTL_SRST;
    spins = 0;
    while ((*(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) & SD_CTL_SRST) && spins++ < 1000)
        IODelay(10);

    // PCM ring buffer, split into kHDABDLEntries equal chunks so the
    // controller raises a buffer-completion interrupt (which we don't
    // currently service - LPIB is polled instead by writePCM) each time
    // it finishes one chunk.
    fPcmBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionOut | kIOMemoryPhysicallyContiguous,
        kHDARingBufBytes, 0xFFFFFFFFull);
    if (!fPcmBuf)
        return false;
    fPcmBuf->prepare();
    fPcmVirt = (uint8_t *)fPcmBuf->getBytesNoCopy();
    fPcmPhys = fPcmBuf->getPhysicalAddress();
    bzero(fPcmVirt, kHDARingBufBytes);

    fBdlBuf = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(
        kernel_task, kIODirectionInOut | kIOMemoryPhysicallyContiguous,
        kHDABDLEntries * 16, 0xFFFFFFFFull);
    if (!fBdlBuf)
        return false;
    fBdlBuf->prepare();
    uint8_t *bdl = (uint8_t *)fBdlBuf->getBytesNoCopy();
    bzero(bdl, kHDABDLEntries * 16);

    uint32_t chunk = kHDARingBufBytes / kHDABDLEntries;
    for (uint32_t i = 0; i < kHDABDLEntries; i++) {
        uint64_t addr = fPcmPhys + i * chunk;
        uint8_t *e = bdl + i * 16;
        *(volatile uint64_t *)(e + 0)  = addr;
        *(volatile uint32_t *)(e + 8)  = chunk;
        *(volatile uint32_t *)(e + 12) = 1;   // IOC: interrupt on completion
    }

    uint64_t bdlPhys = fBdlBuf->getPhysicalAddress();
    wreg32(fRegs ? sdBase + SD_BDPL : 0, (uint32_t)(bdlPhys & 0xFFFFFFFFu));
    *(volatile uint32_t *)(fRegs + sdBase + SD_BDPL) = (uint32_t)(bdlPhys & 0xFFFFFFFFu);
    *(volatile uint32_t *)(fRegs + sdBase + SD_BDPU) = (uint32_t)(bdlPhys >> 32);
    *(volatile uint32_t *)(fRegs + sdBase + SD_CBL)  = kHDARingBufBytes;
    *(volatile uint16_t *)(fRegs + sdBase + SD_LVI)  = kHDABDLEntries - 1;
    *(volatile uint16_t *)(fRegs + sdBase + SD_FORMAT) = HDA_FORMAT_48K_16BIT_STEREO;

    // Set stream number (tag) = 1 in CTL bits[23:20], leave RUN clear -
    // playback starts once the codec side is programmed and userland
    // begins writing.
    ctl = *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS);
    ctl &= ~(0xFu << SD_CTL_STRM_SHIFT);
    ctl |= (1u << SD_CTL_STRM_SHIFT);
    *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) = ctl;

    return true;
}

// Read a 32/64-bit memory BAR's base and size directly from config space
// using the standard sizing dance (write all-ones, read back the mask,
// restore). Needed because QEMU's IODeviceMemory ranges are often
// mis-tagged or absent - same fallback PDE1000/RavynAHCIPort use.
static bool readMemoryBAR(IOPCIDevice *pci, UInt8 reg, uint64_t *outBase, uint64_t *outSize)
{
    if (!pci || !outBase || !outSize) return false;

    const uint16_t savedCmd = pci->configRead16(kIOPCIConfigCommand);
    const uint32_t savedLo  = pci->configRead32(reg);
    uint32_t savedHi = 0;

    if (savedLo & 0x1)              // I/O space BAR, not memory
        return false;

    const bool is64 = ((savedLo & 0x6) == 0x4);
    if (is64) {
        if (reg > kIOPCIConfigBaseAddress4) return false;
        savedHi = pci->configRead32(reg + 4);
    }

    pci->configWrite16(kIOPCIConfigCommand, savedCmd & ~(uint16_t)0x3);
    pci->configWrite32(reg, 0xffffffffU);
    if (is64) pci->configWrite32(reg + 4, 0xffffffffU);

    const uint32_t maskLo = pci->configRead32(reg);
    const uint32_t maskHi = is64 ? pci->configRead32(reg + 4) : 0xffffffffU;

    pci->configWrite32(reg, savedLo);
    if (is64) pci->configWrite32(reg + 4, savedHi);
    pci->configWrite16(kIOPCIConfigCommand, savedCmd);

    uint64_t base = savedLo & ~0x0fULL;
    uint64_t sizeMask = maskLo & ~0x0fULL;
    if (is64) {
        base |= ((uint64_t)savedHi << 32);
        sizeMask |= ((uint64_t)maskHi << 32);
    }
    if (!base || !sizeMask) return false;

    uint64_t size = (~sizeMask) + 1;
    if (!is64) size &= 0xffffffffULL;
    if (size < 0x1000) size = 0x1000;
    if (size > 0x1000000ULL) return false;   // implausible; bail

    *outBase = base;
    *outSize = size;
    return true;
}

// Defined further down alongside the rest of the /dev/dsp0 cdevsw glue.
static void dsp0_publish();
static void dsp0_setDriver(RavynHDAudio *drv);

bool RavynHDAudio::initHardware(IOService *provider)
{
    if (!super::initHardware(provider))
        return false;

    fPCIDevice = OSDynamicCast(IOPCIDevice, provider);
    if (!fPCIDevice)
        return false;

    fPCIDevice->retain();
    if (!fPCIDevice->open(this)) {
        IOLog("RavynHDAudio: failed to open PCI device\n");
        return false;
    }
    fPCIDevice->setBusMasterEnable(true);
    fPCIDevice->setMemoryEnable(true);

    // Map BAR0 (register space). Two paths, mirroring RavynAHCIPort/PDE1000:
    //  1. The provider's IODeviceMemory range (must map kIOMapAnywhere; map()
    //     with no options attempts a fixed mapping at 0 and always fails).
    //  2. QEMU frequently mis-tags / omits the IODeviceMemory ranges, so fall
    //     back to reading BAR0 straight from config space (standard sizing
    //     dance) and building the descriptor with withPhysicalAddress.
    IODeviceMemory *bar0 = fPCIDevice->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (bar0 && bar0->getLength())
        fRegMap = bar0->map(kIOMapAnywhere);

    if (!fRegMap) {
        uint64_t base = 0, size = 0;
        if (readMemoryBAR(fPCIDevice, kIOPCIConfigBaseAddress0, &base, &size)) {
            fRegDesc = IOMemoryDescriptor::withPhysicalAddress(
                (IOPhysicalAddress)base, (IOByteCount)size,
                kIODirectionNone | kIOMemoryMapperNone);
            if (fRegDesc)
                fRegMap = fRegDesc->map(kIOMapAnywhere);
            IOLog("RavynHDAudio: BAR0 config fallback base=0x%llx size=0x%llx map=%p\n",
                (unsigned long long)base, (unsigned long long)size, fRegMap);
        }
    }
    if (!fRegMap) {
        IOLog("RavynHDAudio: failed to map BAR0\n");
        return false;
    }
    fRegs = (volatile uint8_t *)fRegMap->getVirtualAddress();

    if (!resetController())
        return false;
    if (!setupCorbRirb())
        return false;

    uint16_t statests = reg16(HDA_STATESTS);
    bool codecFound = false;
    for (int addr = 0; addr < 15; addr++) {
        if (!(statests & (1u << addr)))
            continue;
        if (enumerateCodec((uint8_t)addr)) {
            codecFound = true;
            break;
        }
    }
    if (!codecFound) {
        IOLog("RavynHDAudio: no usable codec found (STATESTS=0x%x)\n", statests);
        return false;
    }

    if (!setupOutputStream())
        return false;

    dsp0_setDriver(this);
    dsp0_publish();

    /* Hand the hardware to IOAudioFamily. The engine borrows the register
     * mapping and the DMA ring; this object stays their owner. */
    fEngine = new RavynHDAudioEngine;
    if (fEngine != NULL) {
        if (fEngine->initWithHDA(this, fRegs, fOutStreamIndex,
                                 fPcmVirt, kHDARingBufBytes) &&
            activateAudioEngine(fEngine) == kIOReturnSuccess) {
            IOLog("RavynHDAudio: IOAudioFamily engine active\n");
        } else {
            IOLog("RavynHDAudio: audio engine init failed; /dev/dsp0 only\n");
            fEngine->release();
            fEngine = NULL;
        }
    }

    registerService();
    IOLog("RavynHDAudio: ready\n");
    return true;
}

/*
 * The DMA engine has one output stream, so the legacy /dev/dsp0 path and the
 * IOAudioEngine cannot both run it. First claim wins; the loser gets told.
 */
bool RavynHDAudio::claimStream(bool forEngine)
{
    uint32_t want = forEngine ? 2u : 1u;
    bool got = false;

    IOSimpleLockLock(fLock);
    if (fStreamOwner == 0 || fStreamOwner == want) {
        fStreamOwner = want;
        got = true;
    }
    IOSimpleLockUnlock(fLock);

    if (!got) {
        IOLog("RavynHDAudio: stream busy (held by %s)\n",
              fStreamOwner == 1 ? "/dev/dsp0" : "audio engine");
    }
    return got;
}

void RavynHDAudio::releaseStream(bool forEngine)
{
    uint32_t want = forEngine ? 2u : 1u;

    IOSimpleLockLock(fLock);
    if (fStreamOwner == want) {
        fStreamOwner = 0;
    }
    IOSimpleLockUnlock(fLock);
}

void RavynHDAudio::stop(IOService *provider)
{
    dsp0_setDriver(0);
    if (fEngine) { fEngine->release(); fEngine = 0; }
    if (fRegs) {
        uint32_t sdBase = HDA_SD_BASE + fOutStreamIndex * HDA_SD_SIZE;
        *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) &= ~SD_CTL_RUN;
        wreg8(HDA_CORBCTL, 0);
        wreg8(HDA_RIRBCTL, 0);
        wreg32(HDA_GCTL, reg32(HDA_GCTL) & ~1u);
    }
    if (fPcmBuf) { fPcmBuf->complete(); fPcmBuf->release(); fPcmBuf = 0; }
    if (fBdlBuf) { fBdlBuf->complete(); fBdlBuf->release(); fBdlBuf = 0; }
    if (fCorbBuf) { fCorbBuf->complete(); fCorbBuf->release(); fCorbBuf = 0; }
    if (fRirbBuf) { fRirbBuf->complete(); fRirbBuf->release(); fRirbBuf = 0; }
    if (fRegMap) { fRegMap->release(); fRegMap = 0; }
    if (fRegDesc) { fRegDesc->release(); fRegDesc = 0; }
    if (fPCIDevice) {
        fPCIDevice->close(this);
        fPCIDevice->release();
        fPCIDevice = 0;
    }
    super::stop(provider);
}

void RavynHDAudio::stopStream()
{
    if (!fRegs)
        return;
    IOSimpleLockLock(fLock);
    uint32_t sdBase = HDA_SD_BASE + fOutStreamIndex * HDA_SD_SIZE;
    uint32_t ctl = *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS);
    *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) = ctl & ~SD_CTL_RUN;
    fWriteOffset = 0;
    fTotalWritten = 0;
    fConsumedBase = 0;
    fLastLpib = 0;
    IOSimpleLockUnlock(fLock);
}

// Copy PCM bytes into the ring buffer at the current producer offset,
// blocking (polling SD_LPIB - hardware's current read position in the
// ring) whenever the write pointer would lap the hardware's read
// position, so producer and consumer stay honestly synchronized instead
// of relying on the caller guessing the right sleep pace. (on the first
// write) starts the stream running.
size_t RavynHDAudio::writePCM(const uint8_t *data, size_t len)
{
    if (!fPcmVirt || !fRegs)
        return 0;

    uint32_t sdBase = HDA_SD_BASE + fOutStreamIndex * HDA_SD_SIZE;

    IOSimpleLockLock(fLock);

    size_t written = 0;
    while (written < len) {
        // Free space comes from absolute producer/consumer positions, NOT
        // from the raw ring-offset distance: with offsets alone, the
        // moment DMA overtakes the write pointer (one late burst from a
        // bursty producer is enough) the distance calculation inverts -
        // the ring looks almost empty, the producer starts writing BEHIND
        // the hardware read pointer, and the device spends the rest of
        // the ring replaying stale data while fresh audio lands where it
        // won't play until the next wrap. Once out of phase it never
        // recovers, which garbled all tonal audio into ~40ms smear.
        // Tracking bytes-written vs bytes-consumed (LPIB + wrap count)
        // makes underrun detectable, and the recovery below resyncs.
        uint32_t ctl = *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS);
        size_t freeSpace;
        if (!(ctl & SD_CTL_RUN)) {
            freeSpace = kHDARingBufBytes - 1;
        } else {
            uint32_t lpib = *(volatile uint32_t *)(fRegs + sdBase + SD_LPIB) % kHDARingBufBytes;
            if (lpib < fLastLpib)
                fConsumedBase += kHDARingBufBytes;
            fLastLpib = lpib;
            uint64_t consumed = fConsumedBase + lpib;

            if (consumed >= fTotalWritten) {
                // Underrun: DMA ran past everything we wrote and is now
                // playing stale ring contents. Silence the ring so the
                // stale data can't be heard again, and restart the
                // producer a small lead ahead of the hardware so the next
                // writes play promptly but can't be immediately lapped.
                const uint32_t lead = 8192; // ~42ms at 48k/16/stereo
                bzero(fPcmVirt, kHDARingBufBytes);
                fWriteOffset = (lpib + lead) % kHDARingBufBytes;
                fTotalWritten = consumed + lead;
            }

            uint64_t queued = fTotalWritten - consumed;
            freeSpace = (queued >= kHDARingBufBytes - 1)
                            ? 0
                            : (kHDARingBufBytes - 1 - (size_t)queued);
        }
        if (freeSpace == 0) {
            IOSimpleLockUnlock(fLock);
            IOSleep(5);
            IOSimpleLockLock(fLock);
            continue;
        }

        size_t chunk = kHDARingBufBytes - fWriteOffset;
        if (chunk > (len - written)) chunk = len - written;
        if (chunk > freeSpace) chunk = freeSpace;
        bcopy(data + written, fPcmVirt + fWriteOffset, chunk);
        fWriteOffset = (fWriteOffset + chunk) % kHDARingBufBytes;
        fTotalWritten += chunk;
        written += chunk;
    }

    uint32_t ctl = *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS);
    if (!(ctl & SD_CTL_RUN)) {
        fConsumedBase = 0;
        fLastLpib = 0;
        fTotalWritten = fWriteOffset; // bytes staged before the stream ran
        *(volatile uint32_t *)(fRegs + sdBase + SD_CTL_STS) = ctl | SD_CTL_RUN;
    }

    IOSimpleLockUnlock(fLock);
    return written;
}

// ---------------------------------------------------------------------
// /dev/dsp0 character device, same publish/retry pattern IOGOPFramebuffer
// uses for /dev/fb0: devfs isn't up yet when kexts start this early in
// boot, so the first dsp0_publish() call routinely no-ops and needs to be
// retried once IOResources publishes "IOBSD" (devfs_is_ready() flips
// true), with a timed fallback in case that notification races us.
// ---------------------------------------------------------------------
#include <kern/thread_call.h>

extern "C" int devfs_is_ready(void);

static RavynHDAudio *gDsp0Driver;
static void *gDsp0Node;
static int gDsp0Major = -1;
static thread_call_t gDsp0RetryCall;

static void dsp0_setDriver(RavynHDAudio *drv) { gDsp0Driver = drv; }
static IONotifier *gDsp0BSDNotifier;
static unsigned gDsp0RetryCount;

static void dsp0_publish();

static void
dsp0_schedule_retry()
{
    AbsoluteTime deadline;

    if (gDsp0Node || gDsp0RetryCount >= 30)
        return;

    if (!gDsp0RetryCall) {
        gDsp0RetryCall = thread_call_allocate(
            (thread_call_func_t)[](thread_call_param_t, thread_call_param_t) { dsp0_publish(); },
            NULL);
        if (!gDsp0RetryCall)
            return;
    }

    gDsp0RetryCount++;
    clock_interval_to_deadline(1, kSecondScale, &deadline);
    thread_call_enter_delayed(gDsp0RetryCall, deadline);
}

static bool
dsp0_iobsd_published(void *, void *, IOService *, IONotifier *notifier)
{
    dsp0_publish();
    if (gDsp0Node && notifier) {
        notifier->remove();
        if (gDsp0BSDNotifier == notifier)
            gDsp0BSDNotifier = 0;
    }
    return true;
}

static int dsp0_open(dev_t, int, int, struct proc *)
{
    if (!gDsp0Driver)
        return ENXIO;
    /* Refuse if IOAudioFamily is driving the stream. */
    if (!gDsp0Driver->claimStream(false))
        return EBUSY;
    return 0;
}

static int dsp0_close(dev_t, int, int, struct proc *)
{
    if (gDsp0Driver) {
        gDsp0Driver->stopStream();
        gDsp0Driver->releaseStream(false);
    }
    return 0;
}

static int dsp0_write(dev_t, struct uio *uio, int)
{
    if (!gDsp0Driver)
        return ENXIO;

    uint8_t buf[4096];
    int total = 0;
    while (uio_resid(uio) > 0) {
        user_ssize_t resid = uio_resid(uio);
        size_t want = sizeof(buf);
        if ((user_ssize_t)want > resid) want = (size_t)resid;
        int err = uiomove((char *)buf, (int)want, uio);
        if (err) return err;
        size_t wrote = gDsp0Driver->writePCM(buf, want);
        total += (int)wrote;
        if (wrote < want) break;
    }
    return 0;
}

static struct cdevsw dsp0_cdevsw =
{
    /* d_open     */ dsp0_open,
    /* d_close    */ dsp0_close,
    /* d_read     */ eno_rdwrt,
    /* d_write    */ dsp0_write,
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

static void dsp0_publish()
{
    if (gDsp0Node)
        return;

    if (!devfs_is_ready()) {
        if (!gDsp0BSDNotifier) {
            OSDictionary *matching = IOService::resourceMatching("IOBSD");
            if (matching) {
                gDsp0BSDNotifier = IOService::addMatchingNotification(
                    gIOPublishNotification, matching, dsp0_iobsd_published, NULL, NULL);
                matching->release();
            }
        }
        dsp0_schedule_retry();
        return;
    }

    if (gDsp0Major < 0) {
        gDsp0Major = cdevsw_add(-1, &dsp0_cdevsw);
        if (gDsp0Major < 0) {
            IOLog("RavynHDAudio: cdevsw_add for /dev/dsp0 failed\n");
            return;
        }
    }
    gDsp0Node = devfs_make_node(makedev(gDsp0Major, 0), DEVFS_CHAR,
                                 0, 0, 0666, "dsp0");
    if (gDsp0Node) {
        gDsp0RetryCount = 0;
        IOLog("RavynHDAudio: published /dev/dsp0\n");
    } else {
        IOLog("RavynHDAudio: devfs_make_node for /dev/dsp0 failed, retry %u\n", gDsp0RetryCount);
        dsp0_schedule_retry();
    }
}
