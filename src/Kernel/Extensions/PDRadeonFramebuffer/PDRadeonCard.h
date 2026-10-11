#pragma once

#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <kern/thread_call.h>

#include "RadeonAtom.h"

#define LOG(x, ...) kprintf("PDRadeon: " x, ##__VA_ARGS__)

#define kRadeonMaxModes   32
#define kRadeonMaxOutputs 2
#define kRadeonHeadVram   0x1000000u

class PDRadeonFramebuffer;

struct RadeonMode {
    uint32_t clock;     // khz
    uint16_t hdisp, hsyncStart, hsyncEnd, htotal;
    uint16_t vdisp, vsyncStart, vsyncEnd, vtotal;
    bool     nhsync, nvsync;
};

struct RadeonI2c {
    uint32_t maskClk, maskData, enClk, enData, yClk, yData, aClk, aData;
    uint32_t maskClkBit, maskDataBit, enClkBit, enDataBit, yClkBit, yDataBit, aClkBit, aDataBit;
    bool     valid, hwCapable;
};

// one dvi-i connector with its digital and analog encoders
struct RadeonOutput {
    uint16_t  connId;
    RadeonI2c ddc;
    int       hpd;          // 0..5 or -1
    bool      haveDig, dfp2, haveDac, dac2;
    uint8_t   digEnc, dacEnc, xmit;
    bool      present;
    bool      forced;       // assumed present without any sense
    int       senseCount;   // debounce, positive counts present reads, negative absent reads
    bool      hasEdid;
    uint8_t   edid[128];
    int       head;         // crtc driving it or -1
};

struct RadeonPll {
    uint32_t refFreq, inMin, inMax, outMin, outMax;
};

struct RadeonCard {
    IOService     *owner;
    IOPCIDevice   *pci;
    IOMemoryMap   *regMap, *vramMap, *ioMap;
    volatile uint8_t *regs;
    uint16_t       subVendor, subDevice;

    uint8_t       *rom;
    uint32_t       romSize;
    struct atom_card atomCard;
    struct atom_context *atom;

    RadeonPll      pll;
    uint32_t       engineClockMhz;
    uint64_t       vramSize, vramBase;
    uint8_t       *vramCpu;

    RadeonOutput   out[kRadeonMaxOutputs];
    int            nOut;
    PDRadeonFramebuffer *heads[2];
    PDRadeonFramebuffer *consoleHead;
    int            firstCrtc;
    bool           selfTest;
    int            scratchSlack;

    IOLock        *lock;
    thread_call_t  pollCall;
    bool           pollStop;
    uint32_t       pollTick;

    bool           accelReady;
    uint32_t       ringWrite;
    uint32_t       gpuTiling, gpuBackendMap, gpuActiveSimds;
    uint8_t       *renderPages;
    uint32_t       renderPageCount;
    uint32_t renderAlloc(uint32_t size, uint32_t alignment);
    void renderFree(uint32_t offset, uint32_t size);
    void gpuInit(void);
    bool accelInit(void);
    bool accelSubmit(const uint32_t *words, uint32_t count);

    bool           gartReady;
    IOLock        *gartLock;
    bool gartInit(void);
    bool copyOut(IOMemoryDescriptor *dst, uint32_t srcOffset, uint32_t srcPitch, uint32_t rowBytes,
                 uint32_t rows, uint32_t dstPitch);

    uint32_t rreg(uint32_t off) { return *(volatile uint32_t *)(regs + off); }
    void     wreg(uint32_t off, uint32_t v) { *(volatile uint32_t *)(regs + off) = v; }

    bool init(IOService *owner, IOPCIDevice *dev, bool runInit, bool trace, int firstCrtc, bool selfTest, int scratchSlack);
    void teardown(void);

    bool mapResources(void);
    bool loadRom(void);
    bool findRomHole(uint32_t size, uint32_t *out);
    bool cardPosted(void);
    bool readClockInfo(void);
    void mcProgram(void);
    int  runTable(int index, void *args, size_t bytes);

    bool parseOutputs(void);
    bool lookupDdc(RadeonOutput &o, uint8_t id);
    void hpdEnable(void);
    bool hpdSense(RadeonOutput &o);
    bool dacLoadDetect(RadeonOutput &o);
    bool readEdid(RadeonOutput &o);
    bool probeOutput(RadeonOutput &o, bool *viaSense);
    void detectAll(void);
    void assignHeads(void);

    void i2cPrepare(RadeonI2c &d, bool on);
    void i2cClock(RadeonI2c &d, int v);
    void i2cData(RadeonI2c &d, int v);
    int  i2cGetClock(RadeonI2c &d);
    int  i2cGetData(RadeonI2c &d);
    void i2cStart(RadeonI2c &d);
    void i2cStop(RadeonI2c &d);
    bool i2cWriteByte(RadeonI2c &d, uint8_t b);
    uint8_t i2cReadByte(RadeonI2c &d, bool ack);

    bool computePll(uint32_t clockKhz, uint32_t *fb, uint32_t *ref, uint32_t *post);
    bool programHead(int crtc, RadeonOutput &o, const RadeonMode &m, uint32_t fbOffset, uint32_t pitch, uint32_t bitsPerPixel = 32);
    void disableHead(int crtc, RadeonOutput &o);
    void scratchUpdate(int crtc, RadeonOutput &o, bool on);
    void cursorProgram(int crtc, uint32_t addr, bool show, int x, int y, uint32_t w, uint32_t h,
                       uint32_t modeW, uint32_t modeH, bool bothHeads);

    void pollStart(void);
    void pollStopNow(void);
    void pollOnce(void);
};

uint32_t radeonEdidModes(const uint8_t *edid, RadeonMode *modes, uint32_t max, int *preferred);
