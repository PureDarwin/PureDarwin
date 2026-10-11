#include "PDRadeonCard.h"
#include "PDRadeonFramebuffer.h"

#include <IOKit/IODeviceMemory.h>
#include <kern/clock.h>

#define ATOM_BIG_ENDIAN 0
#include "atombios.h"

#define CONFIG_MEMSIZE          0x5428
#define SRBM_STATUS             0x0e50
#define VGA_RENDER_CONTROL      0x0300
#define VGA_MEMORY_BASE_ADDRESS 0x0310
#define VGA_HDP_CONTROL         0x0328
#define HDP_DEBUG1              0x2f34
#define HDP_NONSURFACE_BASE     0x2c04
#define HDP_NONSURFACE_INFO     0x2c08
#define HDP_NONSURFACE_SIZE     0x2c0c
#define MC_VM_FB_LOCATION       0x2024
#define MC_VM_AGP_TOP           0x2028
#define MC_VM_AGP_BOT           0x202c
#define MC_VM_AGP_BASE          0x2030
#define MC_VM_SYS_AP_LOW        0x2034
#define MC_VM_SYS_AP_HIGH       0x2038
#define MC_VM_SYS_AP_DEFAULT    0x203c
#define R700_MC_CITF_CNTL       0x25c0
#define R600_BIF_FB_EN          0x5490
#define BIOS_0_SCRATCH          0x1724
#define BIOS_2_SCRATCH          0x172c
#define BIOS_3_SCRATCH          0x1730
#define D1CRTC_CONTROL          0x6080
#define D2CRTC_CONTROL          0x6880
#define D1CRTC_FRAME_COUNT      0x60a4
#define AVIVO_DC_GPIO_HPD_A     0x7e94

static inline uint16_t rd16(const uint8_t *b, uint32_t o) { return b[o] | (b[o + 1] << 8); }

static void cardRegWrite(struct atom_card *c, uint32_t reg, uint32_t v) { ((RadeonCard *)c->priv)->wreg(reg * 4, v); }
static uint32_t cardRegRead(struct atom_card *c, uint32_t reg) { return ((RadeonCard *)c->priv)->rreg(reg * 4); }

static void cardIoWrite(struct atom_card *c, uint32_t reg, uint32_t v)
{
    RadeonCard *self = (RadeonCard *)c->priv;
    uint32_t off = reg * 4;

    if (off < 256) {
        self->pci->ioWrite32(off, v, self->ioMap);
        return;
    }

    self->pci->ioWrite32(0, off, self->ioMap);
    self->pci->ioWrite32(4, v, self->ioMap);
}

static uint32_t cardIoRead(struct atom_card *c, uint32_t reg)
{
    RadeonCard *self = (RadeonCard *)c->priv;
    uint32_t off = reg * 4;

    if (off < 256)
        return self->pci->ioRead32(off, self->ioMap);

    self->pci->ioWrite32(0, off, self->ioMap);
    return self->pci->ioRead32(4, self->ioMap);
}

static void cardMcWrite(struct atom_card *, uint32_t reg, uint32_t) { LOG("atom mc write %x unsupported\n", reg); }
static uint32_t cardMcRead(struct atom_card *, uint32_t reg) { LOG("atom mc read %x unsupported\n", reg); return 0; }
static void cardPllWrite(struct atom_card *, uint32_t reg, uint32_t) { LOG("atom pll write %x unsupported\n", reg); }
static uint32_t cardPllRead(struct atom_card *, uint32_t reg) { LOG("atom pll read %x unsupported\n", reg); return 0; }

bool RadeonCard::mapResources(void)
{
    regMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress2, kIOMapInhibitCache);
    if (!regMap) {
        LOG("cannot map BAR2 registers\n");
        return false;
    }
    regs = (volatile uint8_t *)regMap->getVirtualAddress();

    ioMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress4);
    if (!ioMap)
        LOG("cannot map BAR4 io\n");

    vramMap = pci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0, kIOMapWriteCombineCache);
    if (!vramMap) {
        LOG("cannot map vram bar\n");
        return false;
    }
    vramCpu = (uint8_t *)vramMap->getVirtualAddress();
    IODeviceMemory *bar0 = pci->getDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    vramBase = bar0 ? bar0->getPhysicalAddress() : 0;

    LOG("bar2 regs phys=0x%llx len=0x%llx, vram bar phys=0x%llx len=0x%llx, io bar %s\n",
        (uint64_t)regMap->getPhysicalAddress(), (uint64_t)regMap->getLength(), vramBase,
        (uint64_t)vramMap->getLength(), ioMap ? "mapped" : "missing");
    return true;
}

// the configurator can leave the rom bar on top of the boot flash, so find a hole in the 32-bit window
bool RadeonCard::findRomHole(uint32_t size, uint32_t *outAddr)
{
    struct range { uint64_t base, len; } used[96];
    uint32_t n = 0;
    OSIterator *it = owner->getMatchingServices(owner->serviceMatching("IOPCIDevice"));

    if (!it)
        return false;

    OSObject *o;
    while ((o = it->getNextObject())) {
        IOPCIDevice *d = OSDynamicCast(IOPCIDevice, o);
        if (!d)
            continue;
        for (unsigned i = 0; i < d->getDeviceMemoryCount() && n < 96; i++) {
            IODeviceMemory *m = d->getDeviceMemoryWithIndex(i);
            if (!m)
                continue;
            used[n].base = m->getPhysicalAddress();
            used[n].len = m->getLength();
            n++;
        }
    }
    it->release();

    for (uint64_t a = 0xfe000000ULL - size; a >= 0xc0000000ULL; a -= size) {
        bool ok = true;
        for (uint32_t i = 0; i < n; i++)
            if (a < used[i].base + used[i].len && used[i].base < a + size)
                ok = false;
        if (ok) {
            *outAddr = (uint32_t)a;
            return true;
        }
    }
    return false;
}

bool RadeonCard::loadRom(void)
{
    IOMemoryMap *map = NULL;
    IODeviceMemory *mem = NULL;
    uint32_t save, len = 0x20000, size, base;
    bool moved = false;

    save = pci->configRead32(kIOPCIConfigExpansionROMBase);
    base = save & ~0x7ffu;

    mem = pci->getDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);
    if (mem)
        len = (uint32_t)mem->getLength();

    for (int attempt = 0; attempt < 2; attempt++) {
        if (attempt == 1) {
            if (!findRomHole(len, &base)) {
                LOG("no free window for the rom\n");
                return false;
            }
            moved = true;
        }

        pci->configWrite32(kIOPCIConfigExpansionROMBase, base | 1);
        mem = IODeviceMemory::withRange(base, len);
        map = mem ? mem->map(kIOMapInhibitCache) : NULL;
        if (mem)
            mem->release();
        if (!map)
            continue;

        volatile uint8_t *r = (volatile uint8_t *)map->getVirtualAddress();
        if (r[0] == 0x55 && r[1] == 0xaa)
            break;

        map->release();
        map = NULL;
    }

    if (!map) {
        pci->configWrite32(kIOPCIConfigExpansionROMBase, save);
        return false;
    }

    volatile uint8_t *r = (volatile uint8_t *)map->getVirtualAddress();
    size = r[2] * 512;
    if (!size || size > len)
        size = len;

    rom = (uint8_t *)IOMallocZero(size + 4096);
    if (!rom) {
        pci->configWrite32(kIOPCIConfigExpansionROMBase, save);
        map->release();
        return false;
    }

    for (uint32_t i = 0; i < size; i += 4)
        *(uint32_t *)(rom + i) = *(volatile uint32_t *)(r + i);
    romSize = size;

    pci->configWrite32(kIOPCIConfigExpansionROMBase, save);
    map->release();
    LOG("rom copied from 0x%x%s, %u bytes\n", base, moved ? " (relocated)" : "", size);
    return true;
}

bool RadeonCard::cardPosted(void)
{
    if (rreg(D1CRTC_CONTROL) & 1) return true;
    if (rreg(D2CRTC_CONTROL) & 1) return true;
    return rreg(CONFIG_MEMSIZE) != 0;
}

int RadeonCard::runTable(int index, void *args, size_t bytes)
{
    uint32_t ps[64];
    int r;

    bzero(ps, sizeof(ps));
    if (bytes > sizeof(ps))
        bytes = sizeof(ps);
    memcpy(ps, args, bytes);
    r = atom_execute_table(atom, index, ps, 64);
    memcpy(args, ps, bytes);

    if (r)
        LOG("table %d failed (%d)\n", index, r);
    return r;
}

bool RadeonCard::readClockInfo(void)
{
    uint16_t off;
    uint8_t frev, crev;
    int index = GetIndexIntoMasterTable(DATA, FirmwareInfo);

    if (!atom_parse_data_header(atom, index, NULL, &frev, &crev, &off))
        return false;

    ATOM_FIRMWARE_INFO_V1_4 *fw = (ATOM_FIRMWARE_INFO_V1_4 *)(rom + off);

    engineClockMhz = fw->ulDefaultEngineClock / 100;
    pll.refFreq = fw->usReferenceClock;
    pll.outMin = fw->ulMinPixelClockPLL_Output;
    if (!pll.outMin)
        pll.outMin = fw->usMinPixelClockPLL_Output;
    if (!pll.outMin)
        pll.outMin = 64800;
    pll.outMax = fw->ulMaxPixelClockPLL_Output;
    pll.inMin = fw->usMinPixelClockPLL_Input;
    pll.inMax = fw->usMaxPixelClockPLL_Input;

    LOG("firmwareinfo %u.%u: ref %u pll in %u..%u out %u..%u (10 khz)\n", frev, crev, pll.refFreq,
        pll.inMin, pll.inMax, pll.outMin, pll.outMax);
    return pll.refFreq && pll.outMax;
}

// vram starts at mc address 0, and the display engines are still off here so the mc needs no blackout
void RadeonCard::mcProgram(void)
{
    uint64_t vramEnd = vramSize - 1;
    uint32_t tmp;

    for (int i = 0, j = 0; i < 32; i++, j += 0x18) {
        wreg(0x2c14 + j, 0);
        wreg(0x2c18 + j, 0);
        wreg(0x2c1c + j, 0);
        wreg(0x2c20 + j, 0);
        wreg(0x2c24 + j, 0);
    }
    (void)rreg(HDP_DEBUG1);

    for (int i = 0; i < 100000 && (rreg(SRBM_STATUS) & 0x3f00); i++)
        IODelay(1);

    wreg(VGA_HDP_CONTROL, 1 << 4);
    wreg(MC_VM_SYS_AP_LOW, 0);
    wreg(MC_VM_SYS_AP_HIGH, (uint32_t)(vramEnd >> 12));
    wreg(MC_VM_SYS_AP_DEFAULT, 0x1000 >> 12);
    wreg(MC_VM_FB_LOCATION, (uint32_t)((((vramEnd >> 24) & 0xffff) << 16) | 0));
    wreg(HDP_NONSURFACE_BASE, 0);
    wreg(HDP_NONSURFACE_INFO, 2 << 7);
    wreg(HDP_NONSURFACE_SIZE, 0x3fffffff);
    wreg(MC_VM_AGP_BASE, 0);
    wreg(MC_VM_AGP_TOP, 0x0fffffff);
    wreg(MC_VM_AGP_BOT, 0x0fffffff);

    wreg(0x6914, 0);
    wreg(0x691c, 0);
    wreg(0x6114, 0);
    wreg(0x611c, 0);
    wreg(VGA_MEMORY_BASE_ADDRESS, 0);

    tmp = rreg(R700_MC_CITF_CNTL);
    wreg(R700_MC_CITF_CNTL, tmp & ~3u);
    wreg(R600_BIF_FB_EN, 3);

    wreg(VGA_RENDER_CONTROL, rreg(VGA_RENDER_CONTROL) & ~(3u << 16));

    LOG("mc: fb location 0x%08x citf 0x%08x bif_fb_en 0x%x\n", rreg(MC_VM_FB_LOCATION),
        rreg(R700_MC_CITF_CNTL), rreg(R600_BIF_FB_EN));
}

bool RadeonCard::init(IOService *own, IOPCIDevice *dev, bool runInit, bool trace, int first, bool test, int slack)
{
    owner = own;
    scratchSlack = slack;
    selfTest = test;
    firstCrtc = first;
    pci = dev;
    lock = IOLockAlloc();

    subVendor = pci->configRead16(0x2c);
    subDevice = pci->configRead16(0x2e);
    LOG("start %04x:%04x subsystem %04x:%04x\n", pci->configRead16(0), pci->configRead16(2), subVendor, subDevice);

    if (!mapResources())
        return false;

    if (!loadRom())
        return false;

    atomCard.priv = this;
    atomCard.reg_write = cardRegWrite;
    atomCard.reg_read = cardRegRead;
    atomCard.ioreg_write = cardIoWrite;
    atomCard.ioreg_read = cardIoRead;
    atomCard.mc_write = cardMcWrite;
    atomCard.mc_read = cardMcRead;
    atomCard.pll_write = cardPllWrite;
    atomCard.pll_read = cardPllRead;
    atomCard.quirk_pll_ctlreq = (subVendor == 0x1682 && subDevice == 0x2931);

    atom = atom_parse(&atomCard, rom, romSize);
    if (!atom) {
        LOG("atom_parse failed\n");
        return false;
    }
    LOG("atom bios \"%s\", cmd table 0x%x data table 0x%x\n", atom->name, atom->cmd_table, atom->data_table);

    if (atom_allocate_fb_scratch(atom, scratchSlack))
        return false;

    atom_trace = trace;

    bool posted = cardPosted();
    LOG("card posted: %s, run init: %s\n", posted ? "yes" : "no", runInit ? "yes" : "no");
    if (runInit && !posted) {
        LOG("running ASIC_Init%s\n", atomCard.quirk_pll_ctlreq ? " (pll_ctlreq quirk on)" : "");
        int r = atom_asic_init(atom);
        LOG("ASIC_Init returned %d, pll_ctlreq writes skipped=%u\n", r, atom->quirk_hits);
        if (r) {
            atom_trace = 0;
            return false;
        }
    }
    atom_trace = 0;

    if (!readClockInfo())
        return false;
    GET_ENGINE_CLOCK_PARAMETERS engineClock = {};
    GET_MEMORY_CLOCK_PARAMETERS memoryClock = {};
    runTable(GetIndexIntoMasterTable(COMMAND, GetEngineClock), &engineClock, sizeof(engineClock));
    runTable(GetIndexIntoMasterTable(COMMAND, GetMemoryClock), &memoryClock, sizeof(memoryClock));
    LOG("boot clocks: engine %u khz memory %u khz\n",
        engineClock.ulReturnEngineClock * 10, memoryClock.ulReturnMemoryClock * 10);

    vramSize = rreg(CONFIG_MEMSIZE);
    if (!vramSize) {
        LOG("vram size reads 0, card not initialised\n");
        return false;
    }
    mcProgram();

    if (!parseOutputs()) {
        LOG("no dvi-i outputs in the object table\n");
        return false;
    }
    hpdEnable();
    detectAll();
    assignHeads();
    return true;
}

void RadeonCard::teardown(void)
{
    pollStopNow();
    if (renderPages) {
        IOFree(renderPages, renderPageCount);
        renderPages = NULL;
    }
    if (atom) {
        atom_destroy(atom);
        atom = NULL;
    }
    if (rom) {
        IOFree(rom, romSize + 4096);
        rom = NULL;
    }
    if (regMap) regMap->release();
    if (vramMap) vramMap->release();
    if (ioMap) ioMap->release();
    regMap = vramMap = ioMap = NULL;
    regs = NULL;
    if (lock) {
        IOLockFree(lock);
        lock = NULL;
    }
}

// connector, encoder, ddc and hpd data for each dvi-i from the atom object table
bool RadeonCard::lookupDdc(RadeonOutput &o, uint8_t id)
{
    uint16_t off, size;
    int index = GetIndexIntoMasterTable(DATA, GPIO_I2C_Info);

    if (!atom_parse_data_header(atom, index, &size, NULL, NULL, &off))
        return false;

    int n = (size - 4) / sizeof(ATOM_GPIO_I2C_ASSIGMENT);
    for (int i = 0; i < n; i++) {
        ATOM_GPIO_I2C_ASSIGMENT *g = (ATOM_GPIO_I2C_ASSIGMENT *)(rom + off + 4 + i * sizeof(ATOM_GPIO_I2C_ASSIGMENT));
        if (g->sucI2cId.ucAccess != id)
            continue;

        RadeonI2c &d = o.ddc;
        d.maskClk = g->usClkMaskRegisterIndex * 4;
        d.maskData = g->usDataMaskRegisterIndex * 4;
        d.enClk = g->usClkEnRegisterIndex * 4;
        d.enData = g->usDataEnRegisterIndex * 4;
        d.yClk = g->usClkY_RegisterIndex * 4;
        d.yData = g->usDataY_RegisterIndex * 4;
        d.aClk = g->usClkA_RegisterIndex * 4;
        d.aData = g->usDataA_RegisterIndex * 4;
        d.maskClkBit = 1u << g->ucClkMaskShift;
        d.maskDataBit = 1u << g->ucDataMaskShift;
        d.enClkBit = 1u << g->ucClkEnShift;
        d.enDataBit = 1u << g->ucDataEnShift;
        d.yClkBit = 1u << g->ucClkY_Shift;
        d.yDataBit = 1u << g->ucDataY_Shift;
        d.aClkBit = 1u << g->ucClkA_Shift;
        d.aDataBit = 1u << g->ucDataA_Shift;
        d.hwCapable = g->sucI2cId.sbfAccess.bfHW_Capable;
        d.valid = d.maskClk != 0;
        return d.valid;
    }

    return false;
}

bool RadeonCard::parseOutputs(void)
{
    uint16_t oh;
    int index = GetIndexIntoMasterTable(DATA, Object_Header);

    if (!atom_parse_data_header(atom, index, NULL, NULL, NULL, &oh))
        return false;

    uint32_t ct = oh + rd16(rom, oh + 6);
    uint8_t nconn = rom[ct];
    uint32_t pathBase = oh + rd16(rom, oh + 14);
    uint8_t npath = rom[pathBase];

    nOut = 0;
    for (int i = 0; i < nconn && nOut < kRadeonMaxOutputs; i++) {
        uint32_t co = ct + 4 + i * 8;
        uint16_t conn = rd16(rom, co);

        // 1 and 2 are single and dual link dvi-i, tv-out is the 7 pin din
        if ((conn & 0xff) != 1 && (conn & 0xff) != 2)
            continue;

        RadeonOutput &o = out[nOut];
        bzero(&o, sizeof(o));
        o.connId = conn;
        o.hpd = -1;
        o.head = -1;

        uint32_t r = oh + rd16(rom, co + 4);
        while (rom[r] != 0xff && rom[r + 1]) {
            if (rom[r] == ATOM_I2C_RECORD_TYPE)
                lookupDdc(o, rom[r + 2]);
            if (rom[r] == ATOM_HPD_INT_RECORD_TYPE) {
                uint16_t goff, gsize;
                if (atom_parse_data_header(atom, GetIndexIntoMasterTable(DATA, GPIO_Pin_LUT), &gsize, NULL, NULL, &goff)) {
                    int pins = (gsize - 4) / 4;
                    for (int k = 0; k < pins; k++) {
                        uint32_t pe = goff + 4 + k * 4;
                        if (rom[pe + 3] != rom[r + 2])
                            continue;
                        uint32_t reg = rd16(rom, pe) * 4, mask = 1u << rom[pe + 2];
                        if (reg == AVIVO_DC_GPIO_HPD_A)
                            o.hpd = mask == (1u << 0) ? 0 : mask == (1u << 8) ? 1 : mask == (1u << 16) ? 2 :
                                    mask == (1u << 24) ? 3 : mask == (1u << 26) ? 4 : mask == (1u << 28) ? 5 : -1;
                        break;
                    }
                }
            }
            r += rom[r + 1];
        }

        uint32_t q = pathBase + 4;
        for (int k = 0; k < npath; k++) {
            uint16_t tag = rd16(rom, q), size = rd16(rom, q + 2), c = rd16(rom, q + 4);
            uint8_t enc = rd16(rom, q + 8) & 0xff;

            if (c == conn) {
                if ((tag == 0x0008 || tag == 0x0080) && !o.haveDig) {
                    o.haveDig = true;
                    o.digEnc = enc;
                    o.dfp2 = tag == 0x0080;
                    o.xmit = enc == 0x20 ? 1 : enc == 0x21 ? 2 : 0;
                }
                if ((tag == 0x0001 || tag == 0x0010) && !o.haveDac) {
                    o.haveDac = true;
                    o.dacEnc = enc;
                    o.dac2 = tag == 0x0010;
                }
            }
            q += size;
        }

        LOG("output %d: connector 0x%04x hpd %d ddc %s dig %s0x%02x (transmitter %u%s) dac %s0x%02x%s\n", nOut, conn,
            o.hpd, o.ddc.valid ? "yes" : "no", o.haveDig ? "" : "(none) ", o.digEnc, o.xmit, o.dfp2 ? " dfp2" : "",
            o.haveDac ? "" : "(none) ", o.dacEnc, o.dac2 ? " dac2" : "");
        nOut++;
    }

    return nOut > 0;
}

static const uint32_t hpdStatus[6] = { 0x7d00, 0x7d0c, 0x7d18, 0x7d24, 0x7dc0, 0x7df4 };
static const uint32_t hpdControl[6] = { 0x7d08, 0x7d14, 0x7d20, 0x7d2c, 0x7dc8, 0x7dfc };
static const uint32_t hpdIntControl[6] = { 0x7d04, 0x7d10, 0x7d1c, 0x7d28, 0x7dc4, 0x7df8 };

void RadeonCard::hpdEnable(void)
{
    for (int i = 0; i < nOut; i++) {
        if (out[i].hpd < 0)
            continue;
        // connection timer 0x9c4, rx timer 0xfa, enable
        wreg(hpdControl[out[i].hpd], 0x9c4 | (0xfa << 16) | (1u << 28));
        LOG("hpd %d enabled for connector 0x%04x, sense %d\n", out[i].hpd, out[i].connId, hpdSense(out[i]));
    }
}

bool RadeonCard::hpdSense(RadeonOutput &o)
{
    if (o.hpd < 0)
        return false;
    return (rreg(hpdStatus[o.hpd]) & (1 << 1)) != 0;
}

// analog load detection through the atom table, result lands in the bios scratch
bool RadeonCard::dacLoadDetect(RadeonOutput &o)
{
    if (!o.haveDac)
        return false;

    DAC_LOAD_DETECTION_PS_ALLOCATION a;
    bzero(&a, sizeof(a));
    a.sDacload.usDeviceID = o.dac2 ? ATOM_DEVICE_CRT2_SUPPORT : ATOM_DEVICE_CRT1_SUPPORT;
    a.sDacload.ucDacType = o.dac2 ? ATOM_DAC_B : ATOM_DAC_A;
    if (runTable(GetIndexIntoMasterTable(COMMAND, DAC_LoadDetection), &a, sizeof(a)))
        return false;

    uint32_t s0 = rreg(BIOS_0_SCRATCH);
    return (s0 & (o.dac2 ? ATOM_S0_CRT2_MASK : ATOM_S0_CRT1_MASK)) != 0;
}

void RadeonCard::i2cPrepare(RadeonI2c &d, bool on)
{
    uint32_t t;

    if (on) {
        if (d.hwCapable)
            wreg(d.maskClk, rreg(d.maskClk) & ~(1u << 16));
        wreg(d.aClk, rreg(d.aClk) & ~d.aClkBit);
        wreg(d.aData, rreg(d.aData) & ~d.aDataBit);
        wreg(d.enClk, rreg(d.enClk) & ~d.enClkBit);
        wreg(d.enData, rreg(d.enData) & ~d.enDataBit);
        wreg(d.maskClk, rreg(d.maskClk) | d.maskClkBit);
        t = rreg(d.maskClk);
        wreg(d.maskData, rreg(d.maskData) | d.maskDataBit);
        t = rreg(d.maskData);
    } else {
        wreg(d.maskClk, rreg(d.maskClk) & ~d.maskClkBit);
        t = rreg(d.maskClk);
        wreg(d.maskData, rreg(d.maskData) & ~d.maskDataBit);
        t = rreg(d.maskData);
    }
    (void)t;
}

void RadeonCard::i2cClock(RadeonI2c &d, int v)
{
    uint32_t t = rreg(d.enClk) & ~d.enClkBit;
    wreg(d.enClk, t | (v ? 0 : d.enClkBit));
    IODelay(5);
}

void RadeonCard::i2cData(RadeonI2c &d, int v)
{
    uint32_t t = rreg(d.enData) & ~d.enDataBit;
    wreg(d.enData, t | (v ? 0 : d.enDataBit));
    IODelay(5);
}

int RadeonCard::i2cGetClock(RadeonI2c &d) { return (rreg(d.yClk) & d.yClkBit) != 0; }
int RadeonCard::i2cGetData(RadeonI2c &d) { return (rreg(d.yData) & d.yDataBit) != 0; }

void RadeonCard::i2cStart(RadeonI2c &d)
{
    i2cData(d, 1);
    i2cClock(d, 1);
    i2cData(d, 0);
    i2cClock(d, 0);
}

void RadeonCard::i2cStop(RadeonI2c &d)
{
    i2cData(d, 0);
    i2cClock(d, 1);
    i2cData(d, 1);
}

bool RadeonCard::i2cWriteByte(RadeonI2c &d, uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        i2cData(d, (b >> i) & 1);
        i2cClock(d, 1);
        i2cClock(d, 0);
    }

    i2cData(d, 1);
    i2cClock(d, 1);
    for (int i = 0; i < 100 && !i2cGetClock(d); i++)
        IODelay(10);
    bool ack = !i2cGetData(d);
    i2cClock(d, 0);
    return ack;
}

uint8_t RadeonCard::i2cReadByte(RadeonI2c &d, bool ack)
{
    uint8_t b = 0;

    i2cData(d, 1);
    for (int i = 0; i < 8; i++) {
        i2cClock(d, 1);
        for (int j = 0; j < 100 && !i2cGetClock(d); j++)
            IODelay(10);
        b = (b << 1) | i2cGetData(d);
        i2cClock(d, 0);
    }

    i2cData(d, ack ? 0 : 1);
    i2cClock(d, 1);
    i2cClock(d, 0);
    i2cData(d, 1);
    return b;
}

bool RadeonCard::readEdid(RadeonOutput &o)
{
    uint8_t buf[128];

    if (!o.ddc.valid)
        return false;

    i2cPrepare(o.ddc, true);
    i2cStart(o.ddc);
    bool ok = i2cWriteByte(o.ddc, 0xa0) && i2cWriteByte(o.ddc, 0x00);
    if (ok) {
        i2cStart(o.ddc);
        ok = i2cWriteByte(o.ddc, 0xa1);
    }
    if (ok) {
        for (int i = 0; i < 128; i++)
            buf[i] = i2cReadByte(o.ddc, i != 127);
    }
    i2cStop(o.ddc);
    i2cPrepare(o.ddc, false);

    if (!ok)
        return false;

    uint8_t sum = 0;
    for (int i = 0; i < 128; i++)
        sum += buf[i];
    static const uint8_t magic[8] = { 0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0 };
    if (sum || memcmp(buf, magic, 8))
        return false;

    memcpy(o.edid, buf, 128);
    return true;
}

// one presence check, sense says whether a hpd pin (true) or a ddc ack (false) decided it
bool RadeonCard::probeOutput(RadeonOutput &o, bool *viaSense)
{
    if (o.hpd >= 0) {
        *viaSense = true;
        return hpdSense(o);
    }

    *viaSense = false;
    return readEdid(o);
}

// boot time detection: hpd, ddc and analog load, every output is looked at once
void RadeonCard::detectAll(void)
{
    bool any = false;

    for (int i = 0; i < nOut; i++) {
        RadeonOutput &o = out[i];
        bool sense = hpdSense(o);
        bool ddc = false;

        for (int t = 0; t < 3 && !ddc; t++)
            ddc = readEdid(o);
        o.hasEdid = ddc;

        bool load = false;
        if (!sense && !ddc)
            load = dacLoadDetect(o);

        o.present = sense || ddc || load;
        o.senseCount = o.present ? 2 : -3;
        LOG("output %d (0x%04x): hpd sense %d, edid %d, analog load %d -> %s\n", i, o.connId, sense, ddc, load,
            o.present ? "connected" : "empty");
        any |= o.present;
    }

    if (!any) {
        // nothing sensed, assume a plain monitor on the first port so something shows
        RadeonOutput &o = out[0];
        o.present = true;
        o.forced = true;
        o.senseCount = 2;
        LOG("no output sensed, assuming connector 0x%04x\n", o.connId);
    }
}

void RadeonCard::assignHeads(void)
{
    int next = firstCrtc;

    for (int i = 0; i < nOut; i++) {
        out[i].head = -1;
        if (out[i].present && next < 2) {
            out[i].head = next++;
            LOG("connector 0x%04x -> crtc %d\n", out[i].connId, out[i].head);
        }
    }
}

// plain search over the divider space, the vco has to land inside the pll limits
bool RadeonCard::computePll(uint32_t clockKhz, uint32_t *fb, uint32_t *ref, uint32_t *post)
{
    uint64_t target = clockKhz / 10;
    uint64_t best = ~0ULL;
    bool found = false;

    for (uint32_t r = 2; r <= 0x3ff; r++) {
        uint64_t in = pll.refFreq / r;
        if ((pll.inMin && in < pll.inMin) || (pll.inMax && in > pll.inMax))
            continue;

        for (uint32_t pd = 2; pd <= 0x7f; pd++) {
            uint64_t vco = target * pd;
            if (vco < pll.outMin || vco > pll.outMax)
                continue;

            uint64_t f = (vco * r + pll.refFreq / 2) / pll.refFreq;
            if (f < 4 || f > 0x7ff)
                continue;

            uint64_t actual = (uint64_t)pll.refFreq * f / ((uint64_t)r * pd);
            uint64_t err = actual > target ? actual - target : target - actual;
            if (err < best) {
                best = err;
                *fb = (uint32_t)f;
                *ref = r;
                *post = pd;
                found = true;
            }
        }
        if (found && best == 0)
            break;
    }

    if (found)
        LOG("pll: %u khz -> fb %u ref %u post %u (error %u x 10 khz)\n", clockKhz, *fb, *ref, *post, (uint32_t)best);
    return found;
}

void RadeonCard::scratchUpdate(int crtc, RadeonOutput &o, bool on)
{
    uint32_t s2 = rreg(BIOS_2_SCRATCH);
    uint32_t s3 = rreg(BIOS_3_SCRATCH);

    if (o.haveDac) {
        uint32_t act = o.dac2 ? ATOM_S3_CRT2_ACTIVE : ATOM_S3_CRT1_ACTIVE;
        uint32_t cmask = o.dac2 ? ATOM_S3_CRT2_CRTC_ACTIVE : ATOM_S3_CRT1_CRTC_ACTIVE;
        uint32_t dpms = o.dac2 ? ATOM_S2_CRT2_DPMS_STATE : ATOM_S2_CRT1_DPMS_STATE;
        s3 = (s3 | act) & ~cmask;
        if (crtc)
            s3 |= cmask;
        s2 = on ? s2 & ~dpms : s2 | dpms;
    }
    if (o.haveDig) {
        uint32_t act = o.dfp2 ? ATOM_S3_DFP2_ACTIVE : ATOM_S3_DFP1_ACTIVE;
        uint32_t cmask = o.dfp2 ? ATOM_S3_DFP2_CRTC_ACTIVE : ATOM_S3_DFP1_CRTC_ACTIVE;
        uint32_t dpms = o.dfp2 ? ATOM_S2_DFP2_DPMS_STATE : ATOM_S2_DFP1_DPMS_STATE;
        s3 = (s3 | act) & ~cmask;
        if (crtc)
            s3 |= cmask;
        s2 = on ? s2 & ~dpms : s2 | dpms;
    }

    wreg(BIOS_2_SCRATCH, s2);
    wreg(BIOS_3_SCRATCH, s3);
}

// mirrors the radeon helper sequence with the dac and the dig encoder of the output cloned onto the crtc
bool RadeonCard::programHead(int crtc, RadeonOutput &o, const RadeonMode &m, uint32_t fbOffset, uint32_t pitch, uint32_t bitsPerPixel)
{
    // 15 means 16 bit scanout in 1555 layout, which is what the xnu text console draws
    bool x1555 = bitsPerPixel == 15;
    if (x1555) bitsPerPixel = 16;
    if (bitsPerPixel != 16 && bitsPerPixel != 32) return false;
    uint32_t fb = 0, ref = 0, post = 0;
    uint32_t adjusted = m.clock;
    uint32_t off = crtc ? 0x800 : 0;
    uint8_t acrtc = crtc ? ATOM_CRTC2 : ATOM_CRTC1;
    int idxEnableCrtc = GetIndexIntoMasterTable(COMMAND, EnableCRTC);
    int idxMemReq = GetIndexIntoMasterTable(COMMAND, EnableCRTCMemReq);
    int idxBlank = GetIndexIntoMasterTable(COMMAND, BlankCRTC);
    int idxLock = GetIndexIntoMasterTable(COMMAND, UpdateCRTC_DoubleBufferRegisters);
    int idxDacOut = o.dac2 ? GetIndexIntoMasterTable(COMMAND, DAC2OutputControl) : GetIndexIntoMasterTable(COMMAND, DAC1OutputControl);
    int idxDacEnc = o.dac2 ? GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl) : GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl);
    int idxDigEnc = crtc ? GetIndexIntoMasterTable(COMMAND, DIG2EncoderControl) : GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl);
    int idxXmit = GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl);
    uint8_t digSel = crtc ? ASIC_INT_DIG2_ENCODER_ID : ASIC_INT_DIG1_ENCODER_ID;
    uint32_t width = m.hdisp, height = m.vdisp;

    scratchUpdate(crtc, o, false);

    if (o.haveDig) {
        DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 t;
        bzero(&t, sizeof(t));
        t.ucAction = ATOM_TRANSMITTER_ACTION_INIT;
        t.usInitInfo = o.connId & 0xff;
        t.acConfig.ucTransmitterSel = o.xmit;
        runTable(idxXmit, &t, sizeof(t));
    }

    ENABLE_CRTC_PARAMETERS ec;
    BLANK_CRTC_PARAMETERS bc;

    // lock, then crtc off
    bzero(&ec, sizeof(ec));
    ec.ucCRTC = acrtc;
    ec.ucEnable = ATOM_ENABLE;
    runTable(idxLock, &ec, sizeof(ec));

    bzero(&bc, sizeof(bc));
    bc.ucCRTC = acrtc;
    bc.ucBlanking = ATOM_BLANKING;
    runTable(idxBlank, &bc, sizeof(bc));
    bzero(&ec, sizeof(ec));
    ec.ucCRTC = acrtc;
    ec.ucEnable = ATOM_DISABLE;
    runTable(idxMemReq, &ec, sizeof(ec));
    runTable(idxEnableCrtc, &ec, sizeof(ec));

    // encoder prepare: route the crtc and switch the outputs off
    if (o.haveDac) {
        SELECT_CRTC_SOURCE_PARAMETERS_V2 s;
        bzero(&s, sizeof(s));
        s.ucCRTC = acrtc;
        s.ucEncoderID = o.dac2 ? ASIC_INT_DAC2_ENCODER_ID : ASIC_INT_DAC1_ENCODER_ID;
        s.ucEncodeMode = ATOM_ENCODER_MODE_CRT;
        runTable(GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source), &s, sizeof(s));

        DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS oc;
        bzero(&oc, sizeof(oc));
        oc.ucAction = ATOM_DISABLE;
        runTable(idxDacOut, &oc, sizeof(oc));
    }
    if (o.haveDig) {
        SELECT_CRTC_SOURCE_PARAMETERS_V2 s;
        bzero(&s, sizeof(s));
        s.ucCRTC = acrtc;
        s.ucEncoderID = digSel;
        s.ucEncodeMode = ATOM_ENCODER_MODE_DVI;
        runTable(GetIndexIntoMasterTable(COMMAND, SelectCRTC_Source), &s, sizeof(s));

        DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 t;
        bzero(&t, sizeof(t));
        t.ucAction = ATOM_TRANSMITTER_ACTION_DISABLE;
        t.usPixelClock = m.clock / 10;
        t.acConfig.ucEncoderSel = crtc;
        t.acConfig.ucTransmitterSel = o.xmit;
        runTable(idxXmit, &t, sizeof(t));

        DIG_ENCODER_CONTROL_PARAMETERS e;
        bzero(&e, sizeof(e));
        e.ucAction = ATOM_DISABLE;
        e.usPixelClock = m.clock / 10;
        e.ucEncoderMode = ATOM_ENCODER_MODE_DVI;
        e.ucLaneNum = 4;
        runTable(idxDigEnc, &e, sizeof(e));
    }

    // crtc mode set: adjusted pixel clock, ppll, timing, scanout base
    {
        ADJUST_DISPLAY_PLL_PARAMETERS a;
        bzero(&a, sizeof(a));
        a.usPixelClock = m.clock / 10;
        a.ucTransmitterID = o.digEnc;
        a.ucEncodeMode = ATOM_ENCODER_MODE_DVI;
        if (!runTable(GetIndexIntoMasterTable(COMMAND, AdjustDisplayPll), &a, sizeof(a)) && a.usPixelClock)
            adjusted = a.usPixelClock * 10;
    }

    if (!computePll(adjusted, &fb, &ref, &post)) {
        LOG("no pll dividers for %u khz\n", adjusted);
        return false;
    }

    {
        PIXEL_CLOCK_PARAMETERS_V3 p;
        bzero(&p, sizeof(p));
        p.usPixelClock = m.clock / 10;
        p.usRefDiv = ref;
        p.usFbDiv = fb;
        p.ucPostDiv = post;
        p.ucFracFbDiv = 0;
        p.ucPpll = crtc ? ATOM_PPLL2 : ATOM_PPLL1;
        p.ucMiscInfo = crtc ? PIXEL_CLOCK_MISC_CRTC_SEL_CRTC2 : PIXEL_CLOCK_MISC_CRTC_SEL_CRTC1;
        p.ucTransmitterId = o.digEnc;
        p.ucEncoderMode = ATOM_ENCODER_MODE_DVI;
        runTable(GetIndexIntoMasterTable(COMMAND, SetPixelClock), &p, sizeof(p));
    }

    {
        SET_CRTC_USING_DTD_TIMING_PARAMETERS d;
        uint16_t misc = 0;
        bzero(&d, sizeof(d));
        d.usH_Size = m.hdisp;
        d.usH_Blanking_Time = m.htotal - m.hdisp;
        d.usV_Size = m.vdisp;
        d.usV_Blanking_Time = m.vtotal - m.vdisp;
        d.usH_SyncOffset = m.hsyncStart - m.hdisp;
        d.usH_SyncWidth = m.hsyncEnd - m.hsyncStart;
        d.usV_SyncOffset = m.vsyncStart - m.vdisp;
        d.usV_SyncWidth = m.vsyncEnd - m.vsyncStart;
        if (m.nvsync) misc |= ATOM_VSYNC_POLARITY;
        if (m.nhsync) misc |= ATOM_HSYNC_POLARITY;
        d.susModeMiscInfo.usAccess = misc;
        d.ucCRTC = acrtc;
        runTable(GetIndexIntoMasterTable(COMMAND, SetCRTC_UsingDTDTiming), &d, sizeof(d));
    }

    // linear scanout: ARGB8888 or RGB565, engine and memory clocks unchanged.
    wreg(crtc ? 0x0338 : 0x0330, 0);
    wreg(0x6148 + off, 0);
    wreg(crtc ? 0x6114 : 0x6914, 0);
    wreg(crtc ? 0x611c : 0x691c, 0);
    wreg(0x6110 + off, fbOffset);
    wreg(0x6118 + off, fbOffset);
    wreg(0x6104 + off, bitsPerPixel == 16 ? (x1555 ? 1u : (1u | (1u << 8))) : 2u);
    wreg(0x610c + off, 0);
    wreg(0x6108 + off, rreg(0x6108 + off) & ~0x100u);
    wreg(0x6124 + off, 0);
    wreg(0x6128 + off, 0);
    wreg(0x612c + off, 0);
    wreg(0x6130 + off, 0);
    wreg(0x6134 + off, width);
    wreg(0x6138 + off, height);
    wreg(0x6120 + off, pitch / (bitsPerPixel / 8));
    wreg(0x6100 + off, 1);
    wreg(0x652c + off, height);
    wreg(0x6580 + off, 0);
    wreg(0x6584 + off, (width << 16) | ((height + 1) & ~1u));
    wreg(0x60e4 + off, 3);

    {
        SET_CRTC_OVERSCAN_PARAMETERS ov;
        bzero(&ov, sizeof(ov));
        ov.ucCRTC = acrtc;
        runTable(GetIndexIntoMasterTable(COMMAND, SetCRTC_OverScan), &ov, sizeof(ov));

        ENABLE_SCALER_PARAMETERS sc;
        bzero(&sc, sizeof(sc));
        sc.ucScaler = crtc ? ATOM_SCALER2 : ATOM_SCALER1;
        sc.ucEnable = ATOM_SCALER_DISABLE;
        runTable(GetIndexIntoMasterTable(COMMAND, EnableScaler), &sc, sizeof(sc));
    }

    // encoder mode set
    {
        ENABLE_YUV_PARAMETERS y;
        uint32_t save = rreg(BIOS_3_SCRATCH);
        bzero(&y, sizeof(y));
        y.ucCRTC = acrtc;
        wreg(BIOS_3_SCRATCH, 0);
        runTable(GetIndexIntoMasterTable(COMMAND, EnableYUV), &y, sizeof(y));
        wreg(BIOS_3_SCRATCH, save);
    }
    if (o.haveDac) {
        DAC_ENCODER_CONTROL_PARAMETERS dc;
        bzero(&dc, sizeof(dc));
        dc.usPixelClock = m.clock / 10;
        dc.ucDacStandard = ATOM_DAC1_PS2;
        dc.ucAction = ATOM_ENABLE;
        runTable(idxDacEnc, &dc, sizeof(dc));
    }

    // identity gamma
    wreg(0x64c0 + off, 0);
    wreg(0x64c4 + off, 0);
    wreg(0x64c8 + off, 0);
    wreg(0x64cc + off, 0);
    wreg(0x64d0 + off, 0xffff);
    wreg(0x64d4 + off, 0xffff);
    wreg(0x64d8 + off, 0xffff);
    wreg(0x6480, crtc);
    wreg(0x6484, 0);
    wreg(0x649c, 0x3f);
    *(volatile uint8_t *)(regs + 0x6488) = 0;
    for (int i = 0; i < 256; i++) {
        uint32_t v = ((i << 8 | i) & 0xffc0);
        wreg(0x6494, (v << 14) | (v << 4) | (v >> 6));
    }
    wreg(0x6108 + off, (rreg(0x6108 + off) & ~1u) | (crtc ? 1u : 0u));

    // crtc commit
    bzero(&ec, sizeof(ec));
    ec.ucCRTC = acrtc;
    ec.ucEnable = ATOM_ENABLE;
    runTable(idxEnableCrtc, &ec, sizeof(ec));
    runTable(idxMemReq, &ec, sizeof(ec));
    bzero(&bc, sizeof(bc));
    bc.ucCRTC = acrtc;
    bc.ucBlanking = ATOM_BLANKING_OFF;
    runTable(idxBlank, &bc, sizeof(bc));
    bzero(&ec, sizeof(ec));
    ec.ucCRTC = acrtc;
    ec.ucEnable = ATOM_DISABLE;
    runTable(idxLock, &ec, sizeof(ec));

    // encoder commit: outputs on
    if (o.haveDac) {
        DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS oc;
        bzero(&oc, sizeof(oc));
        oc.ucAction = ATOM_ENABLE;
        runTable(idxDacOut, &oc, sizeof(oc));
    }
    if (o.haveDig) {
        DIG_ENCODER_CONTROL_PARAMETERS e;
        bzero(&e, sizeof(e));
        e.ucAction = ATOM_ENABLE;
        e.usPixelClock = m.clock / 10;
        e.ucEncoderMode = ATOM_ENCODER_MODE_DVI;
        e.ucLaneNum = 4;
        runTable(idxDigEnc, &e, sizeof(e));

        DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 t;
        bzero(&t, sizeof(t));
        t.ucAction = ATOM_TRANSMITTER_ACTION_SETUP;
        t.usPixelClock = m.clock / 10;
        t.acConfig.ucEncoderSel = crtc;
        t.acConfig.ucTransmitterSel = o.xmit;
        t.acConfig.fCoherentMode = 1;
        runTable(idxXmit, &t, sizeof(t));

        bzero(&t, sizeof(t));
        t.ucAction = ATOM_TRANSMITTER_ACTION_ENABLE;
        t.usPixelClock = m.clock / 10;
        t.acConfig.ucEncoderSel = crtc;
        t.acConfig.ucTransmitterSel = o.xmit;
        t.acConfig.fCoherentMode = 1;
        runTable(idxXmit, &t, sizeof(t));
    }
    scratchUpdate(crtc, o, true);

    // scanout check: the crtc enabled and the frame counter moving
    uint32_t f0 = rreg(D1CRTC_FRAME_COUNT + off);
    IOSleep(100);
    uint32_t f1 = rreg(D1CRTC_FRAME_COUNT + off);
    LOG("crtc%d: %ux%u control 0x%08x frame count %u -> %u pitch %u surface 0x%08x\n", crtc + 1, width, height,
        rreg(D1CRTC_CONTROL + off), f0, f1, rreg(0x6120 + off), rreg(0x6110 + off));

    bool bad = (f0 == f1) || !(rreg(D1CRTC_CONTROL + off) & 1);
    if (bad)
        LOG("crtc%d is not scanning out\n", crtc + 1);
    return !bad;
}

// outputs off, crtc off, pixel pll off
void RadeonCard::disableHead(int crtc, RadeonOutput &o)
{
    uint32_t off = crtc ? 0x800 : 0;
    uint8_t acrtc = crtc ? ATOM_CRTC2 : ATOM_CRTC1;
    ENABLE_CRTC_PARAMETERS ec;
    BLANK_CRTC_PARAMETERS bc;

    if (o.haveDig) {
        DIG_TRANSMITTER_CONTROL_PARAMETERS_V2 t;
        bzero(&t, sizeof(t));
        t.ucAction = ATOM_TRANSMITTER_ACTION_DISABLE;
        t.acConfig.ucEncoderSel = crtc;
        t.acConfig.ucTransmitterSel = o.xmit;
        runTable(GetIndexIntoMasterTable(COMMAND, UNIPHYTransmitterControl), &t, sizeof(t));

        DIG_ENCODER_CONTROL_PARAMETERS e;
        bzero(&e, sizeof(e));
        e.ucAction = ATOM_DISABLE;
        e.ucEncoderMode = ATOM_ENCODER_MODE_DVI;
        e.ucLaneNum = 4;
        runTable(crtc ? GetIndexIntoMasterTable(COMMAND, DIG2EncoderControl) : GetIndexIntoMasterTable(COMMAND, DIG1EncoderControl), &e, sizeof(e));
    }
    if (o.haveDac) {
        DISPLAY_DEVICE_OUTPUT_CONTROL_PARAMETERS oc;
        bzero(&oc, sizeof(oc));
        oc.ucAction = ATOM_DISABLE;
        runTable(o.dac2 ? GetIndexIntoMasterTable(COMMAND, DAC2OutputControl) : GetIndexIntoMasterTable(COMMAND, DAC1OutputControl), &oc, sizeof(oc));

        DAC_ENCODER_CONTROL_PARAMETERS dc;
        bzero(&dc, sizeof(dc));
        dc.ucDacStandard = ATOM_DAC1_PS2;
        dc.ucAction = ATOM_DISABLE;
        runTable(o.dac2 ? GetIndexIntoMasterTable(COMMAND, DAC2EncoderControl) : GetIndexIntoMasterTable(COMMAND, DAC1EncoderControl), &dc, sizeof(dc));
    }

    bzero(&bc, sizeof(bc));
    bc.ucCRTC = acrtc;
    bc.ucBlanking = ATOM_BLANKING;
    runTable(GetIndexIntoMasterTable(COMMAND, BlankCRTC), &bc, sizeof(bc));
    bzero(&ec, sizeof(ec));
    ec.ucCRTC = acrtc;
    ec.ucEnable = ATOM_DISABLE;
    runTable(GetIndexIntoMasterTable(COMMAND, EnableCRTCMemReq), &ec, sizeof(ec));
    runTable(GetIndexIntoMasterTable(COMMAND, EnableCRTC), &ec, sizeof(ec));

    wreg(0x6100 + off, 0);

    PIXEL_CLOCK_PARAMETERS_V3 p;
    bzero(&p, sizeof(p));
    p.ucPpll = crtc ? ATOM_PPLL2 : ATOM_PPLL1;
    p.ucMiscInfo = crtc ? PIXEL_CLOCK_MISC_CRTC_SEL_CRTC2 : PIXEL_CLOCK_MISC_CRTC_SEL_CRTC1;
    runTable(GetIndexIntoMasterTable(COMMAND, SetPixelClock), &p, sizeof(p));

    scratchUpdate(crtc, o, false);
    LOG("crtc%d off, control 0x%08x\n", crtc + 1, rreg(D1CRTC_CONTROL + off));
}

// avivo hardware cursor, 64x64 argb premultiplied in vram at addr
void RadeonCard::cursorProgram(int crtc, uint32_t addr, bool show, int x, int y, uint32_t w, uint32_t h,
                               uint32_t modeW, uint32_t modeH, bool bothHeads)
{
    uint32_t off = crtc ? 0x800 : 0;
    int xorigin = 0, yorigin = 0;
    int cw = (int)w;
    bool oob = !show;

    wreg(0x6424 + off, rreg(0x6424 + off) | (1u << 16));

    if (x < 0)
        xorigin = -x < 63 ? -x : 63;
    if (y < 0)
        yorigin = -y < 63 ? -y : 63;

    // the cursor cannot end on a 128 pixel boundary or run past the frame while both crtcs are on
    if (bothHeads && !oob) {
        int cursorEnd = x + cw;
        int frameEnd = (int)modeW;

        if (cursorEnd >= frameEnd) {
            cw -= cursorEnd - frameEnd;
            if (!(frameEnd & 0x7f))
                cw--;
        } else if (cursorEnd <= 0) {
            oob = true;
        } else if (!(cursorEnd & 0x7f)) {
            cw--;
        }
        if (cw <= 0)
            oob = true;
    }

    if (x <= -(int)w || y <= -(int)h || x >= (int)modeW || y >= (int)modeH)
        oob = true;

    if (oob) {
        wreg(0x6400 + off, 2u << 8);
    } else {
        x += xorigin;
        y += yorigin;
        wreg(0x6414 + off, ((uint32_t)x << 16) | (uint32_t)y);
        wreg(0x6418 + off, ((uint32_t)xorigin << 16) | (uint32_t)yorigin);
        wreg(0x6410 + off, ((uint32_t)(cw - 1) << 16) | (h - 1));
        wreg(crtc ? 0x640c : 0x6c0c, 0);
        wreg(0x6408 + off, addr);
        wreg(0x6400 + off, 1u | (2u << 8));
    }

    wreg(0x6424 + off, rreg(0x6424 + off) & ~(1u << 16));
}

// polling hotplug: hpd where the connector has a pin, ddc otherwise
static void pollEntry(thread_call_param_t p0, thread_call_param_t)
{
    RadeonCard *c = (RadeonCard *)p0;

    if (c->pollStop)
        return;

    c->pollOnce();

    if (!c->pollStop) {
        AbsoluteTime deadline;
        clock_interval_to_deadline(1000, kMillisecondScale, &deadline);
        thread_call_enter_delayed(c->pollCall, deadline);
    }
}

void RadeonCard::pollStart(void)
{
    AbsoluteTime deadline;

    pollStop = false;
    pollCall = thread_call_allocate(pollEntry, this);
    clock_interval_to_deadline(1000, kMillisecondScale, &deadline);
    thread_call_enter_delayed(pollCall, deadline);
}

void RadeonCard::pollStopNow(void)
{
    pollStop = true;
    if (pollCall) {
        thread_call_cancel_wait(pollCall);
        thread_call_free(pollCall);
        pollCall = NULL;
    }
}

void RadeonCard::pollOnce(void)
{
    bool changed[kRadeonMaxOutputs] = { false };

    pollTick++;
    IOLockLock(lock);

    for (int i = 0; i < nOut; i++) {
        RadeonOutput &o = out[i];
        bool sense;
        bool viaSense;

        // ddc only every other tick, it is the slow path
        if (o.hpd < 0 && (pollTick & 1))
            continue;

        sense = probeOutput(o, &viaSense);
        if (o.forced && !sense)
            continue;

        if (sense == o.present) {
            o.senseCount = sense ? 2 : -3;
            continue;
        }

        // debounce: two reads to connect, three to disconnect
        if (sense)
            o.senseCount = o.senseCount > 0 ? o.senseCount + 1 : 1;
        else
            o.senseCount = o.senseCount < 0 ? o.senseCount - 1 : -1;

        if ((sense && o.senseCount >= 2) || (!sense && o.senseCount <= -3)) {
            o.present = sense;
            o.forced = false;
            if (sense) {
                o.hasEdid = false;
                for (int t = 0; t < 3 && !o.hasEdid; t++)
                    o.hasEdid = readEdid(o);
            } else {
                o.hasEdid = false;
            }
            LOG("output %d (0x%04x) %s%s\n", i, o.connId, sense ? "connected" : "disconnected",
                o.hasEdid ? " (edid)" : "");
            changed[i] = true;
        }
    }

    IOLockUnlock(lock);

    for (int i = 0; i < nOut; i++) {
        if (!changed[i])
            continue;
        PDRadeonFramebuffer::outputChanged(this, i);
    }

    if (selfTest) {
        for (int c = 0; c < 2; c++)
            if (heads[c])
                heads[c]->selfTestStep(pollTick);
    }
}
