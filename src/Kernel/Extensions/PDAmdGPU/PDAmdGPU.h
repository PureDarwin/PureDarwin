#pragma once

#include <IOKit/IOService.h>
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
#define kHwNBIF 108
#define kHwMP0  255

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

class PDAmdGPU : public IOService {
    OSDeclareDefaultStructors(PDAmdGPU);

public:
    bool start(IOService *provider) override;
    void stop(IOService *provider) override;

    uint32_t rreg(uint32_t idx) { return regs[idx]; }
    void wreg(uint32_t idx, uint32_t v) { regs[idx] = v; }
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
    IOMemoryMap       *regMap, *doorbellMap;
    volatile uint32_t *regs;
    uint32_t           regCount;
    uint64_t           vramSize;
    AmdIpTable         ips;

    // psp ring, fence, command buffer and firmware staging, all in the carveout
    uint64_t           fbStart, fbOffset, pspBase;
    uint32_t           pspFence;

    void probePsp(void);
    bool pspSubmit(const uint32_t *cmd, uint32_t words, uint32_t *resp, uint32_t respWords);
    void pspLoadToc(void);
    bool pspLoadIpFw(const char *name, uint32_t type, const uint8_t *data, uint32_t size);
    void pspLoadGfx(void);
    bool pspSetupTmr(uint32_t size);
    uint32_t sdmaReg(uint32_t off);
    uint32_t rregAny(uint32_t idx);
    void wregAny(uint32_t idx, uint32_t v);
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
    void teardown(void);
    void traceFault(const char *step);
};

bool amdParseDiscovery(const uint8_t *bin, uint32_t len, AmdIpTable *out);
