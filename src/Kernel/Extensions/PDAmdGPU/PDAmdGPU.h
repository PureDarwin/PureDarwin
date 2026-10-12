#pragma once

#include <IOKit/IOService.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/pci/IOPCIDevice.h>

#define LOG(x, ...) kprintf("PDAmdGPU: " x, ##__VA_ARGS__)

// register offsets below are dword indices, as in the amdgpu headers
#define mmMM_INDEX              0x0
#define mmMM_DATA               0x1
#define mmMM_INDEX_HI           0x6
#define mmRCC_CONFIG_MEMSIZE    0xde3
#define mmMP0_SMN_C2PMSG_33     0x16061

// the discovery table sits in the last 64 KiB of the carveout
#define kAmdDiscoverySize       (64 * 1024)

// hardware ids from the discovery table
#define kHwMP1  1
#define kHwGC   11
#define kHwOSSSYS 40
#define kHwNBIF 108
#define kHwMP0  255
#define kHwDMU  271

struct AmdIp {
    uint16_t hw;
    uint8_t  inst, count;
    uint8_t  major, minor, rev;
    uint32_t base[8];
};

struct AmdIpTable {
    uint32_t n;
    AmdIp    ip[64];

    const AmdIp *find(uint16_t hw, uint8_t inst = 0) const;
};

// one chip's firmware images, each the unmodified linux-firmware file
struct AmdFirmwareImage {
    const uint8_t *start, *end;
};

struct AmdFirmware {
    AmdFirmwareImage toc, sdma, ce, pfp, me, mec, rlc, dmcub;
};

extern const AmdFirmware kAmdFirmwareGraniteRidge, kAmdFirmwareMendocino;

// what differs between the chips this driver knows, all register offsets are mp1 segment 0
struct AmdChip {
    const char        *name;
    uint8_t            gc[3], mp0[3], dmu[3];
    bool               bringUp;         // false: discovery is read and logged, nothing is programmed
    const AmdFirmware *fw;

    // the smu driver mailbox and the messages the driver sends through it
    uint32_t smuMsg, smuArg, smuResp;
    uint32_t msgGfxReset, msgDisallowGfxOff, msgPrepareUnload;

    // the display clock mailbox, granite ridge's message register is an absolute index past the bar
    uint32_t dispMsg, dispArg, dispResp;
    bool     dispMsgAbsolute;
};

struct CeaMode;

class PDAmdGPU : public IOService {
    OSDeclareDefaultStructors(PDAmdGPU);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;
    void systemWillShutdown(IOOptionBits specifier) override;

    uint32_t rreg(uint32_t idx) { return regs[idx]; }
    void wreg(uint32_t idx, uint32_t v) { regs[idx] = v; }
    uint32_t rregAny(uint32_t idx);
    void wregAny(uint32_t idx, uint32_t v);
    bool scanout(uint64_t *phys, uint32_t *w, uint32_t *h, uint32_t *rowBytes);
    void readVram(uint64_t pos, void *buf, uint32_t len);
    void writeVram(uint64_t pos, uint32_t v, uint32_t len);
    void copyToVram(uint64_t pos, const void *buf, uint32_t len);

    // absolute dword index of a register, from the block's base table
    uint32_t soc15(uint16_t hw, uint8_t seg, uint32_t off);
    bool waitReg(uint32_t idx, uint32_t want, uint32_t mask, uint32_t ms);
    void remapHdp(void);
    void flushHdp(void);

private:
    IOPCIDevice       *pci;
    IOMemoryMap       *regMap, *doorbellMap, *vramMap;
    volatile uint32_t *regs;
    uint32_t           regCount;
    uint64_t           vramSize;
    AmdIpTable         ips;
    const AmdChip     *chip;

    // psp ring, fence, command buffer and firmware staging, all in the carveout
    uint64_t           fbStart, fbOffset, pspBase;
    uint32_t           pspFence;
    uint64_t           ihBase, dmubBase;
    const uint8_t     *dmubInst;
    uint32_t           dmubInstSize, dmubStateSize, dmubTraceSize;
    uint8_t           *vbios;
    uint32_t           vbiosSize, dmubWptr;
    uint32_t           ihRptr;
    IOFilterInterruptEventSource *irqSource;
    volatile UInt32    irqCount;
    IOLock            *irqLock;

    void probePsp(void);
    bool pspSubmit(const uint32_t *cmd, uint32_t words, uint32_t *resp, uint32_t respWords);
    void pspLoadToc(void);
    bool pspLoadIpFw(const char *name, uint32_t type, const uint8_t *data, uint32_t size);
    void pspLoadGfx(void);
    bool pspSetupTmr(uint32_t size);
    uint32_t sdmaReg(uint32_t off);
    bool smuSend(uint32_t msg, uint32_t arg, uint32_t *out);
    void smuInit(void);
    void smuDisallowGfxOff(const char *when);
    void smuMode2Reset(const char *when);
    void sdmaTest(void);
    void dumpGfxState(const char *why);
    void gfxTest(bool kiq);
    void grbmSelect(uint32_t me, uint32_t pipe, uint32_t queue);
    bool kiqMapGfx(uint64_t gfxMqdMc, uint64_t gfxWptrMc);
    void gfxStart(void);
    bool ringDoorbell(uint32_t index, uint64_t value);
    void gfxhubInit(void);
    void rlcInit(void);
    void goldenInit(void);
    void teardown(bool reset);
    bool running;
    void ihInit(void);
    uint32_t ihDrain(const char *why);
    void ihStop(void);
    bool irqInit(void);
    bool dmubLoad(void);
    void dmubReset(void);
    bool dmubInit(void);
    void dmubStop(void);
    void dmubDump(const char *why);
    bool readVbios(void);
    bool asicPost(bool force);
    bool dmubCmd(uint32_t *cmd, bool wantReply);
    bool dmubQueryCaps(void);
    void vbiosDisplayPaths(void);
    bool dmubSetPixelClock(uint32_t pix100hz, uint8_t pll, uint8_t encoderId, uint8_t mode, uint8_t crtc);
    bool dmubTransmitter(uint8_t phy, uint8_t action, uint8_t mode, uint8_t lanes, uint32_t symclk10khz, uint8_t hpd,
                         uint8_t digfe, uint8_t connector);
    void hdmiLinkUp(void);
    void hdmiLinkDown(void);
    bool dmubDcPowerState(bool on);
    void dmubNotifyStreamMask(uint16_t mask);
    void displayQuiesce(void);
    bool displayUp;
    bool pipePowerUp(void);
    void pipePowerDown(void);
    void smuPrepareUnload(void);
    void dccgSetup(void);
    void digSetup(void);
    void otgSetup(const CeaMode &m);
    void pipeSetup(const CeaMode &m);
    void hubpSetup(const CeaMode &m, uint64_t surfMc);
    void fillTestPattern(const CeaMode &m, uint64_t vramOff);
    void displayInit(void);
    bool findRomHole(uint32_t size, uint32_t *outAddr);
    int32_t dispSmuSend(uint32_t msg, uint32_t arg);
    bool clkInit(void);
    void irqStop(void);
    bool irqWait(UInt32 seen, uint32_t ms);
    static bool irqFilter(OSObject *owner, IOFilterInterruptEventSource *src);
    static void irqAction(OSObject *owner, IOInterruptEventSource *src, int count);
    void traceFault(const char *step);
};

bool amdParseDiscovery(const uint8_t *bin, uint32_t len, AmdIpTable *out);
