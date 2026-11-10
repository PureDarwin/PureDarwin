// SPDX-License-Identifier: MIT
// hdmi scanout on the tegra x1: dc a or b, plld2, sor1 in tmds mode, edid over ddc, one IOFramebuffer

#include "PDTegraDisplay.h"

#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IOUserClient.h>
#include <kern/clock.h>
#include <pexpert/pexpert.h>

extern "C" {
#include "tegra210_gpu.h"
#include "tegra_i2c_poll.h"
#include <arm/nvidia/tegra_pmc.h>
}

#define super IOFramebuffer
OSDefineMetaClassAndStructors(PDTegraDisplay, IOFramebuffer);

#define kModeID			1
#define kDepth			0

// block addresses from the address map (tegra x1 trm, 2.x)
#define kPhysDCA		0x54200000ULL
#define kPhysDCB		0x54240000ULL
#define kPhysSOR1		0x54580000ULL
#define kPhysDPAUX1		0x54040000ULL
#define kPhysCAR		0x60006000ULL
#define kPhysGPIO8		0x6000d700ULL
#define kPhysI2C2		0x7000c400ULL
#define kPhysI2C4		0x7000c700ULL
#define kPhysMC			0x70019000ULL

// car clock and reset indexes, l=0 h=32 u=64 v=96 w=128 x=160 y=192 (tegra x1 trm 5.2.2-5.2.116)
#define CLK_HOST1X		28
#define CLK_DISP1		27
#define CLK_DISP2		26
#define CLK_I2C2		(32 + 22)
#define CLK_I2C4		(96 + 7)
#define CLK_SOR1		(160 + 23)
#define CLK_DPAUX1		(192 + 15)
#define CLK_SOR_SAFE		(192 + 30)

// car clock sources: source at 31:29 (tegra x1 trm 5.2.62, 5.2.63, 5.2.130, 5.2.139)
#define CAR_SRC_DISP1		0x138
#define CAR_SRC_DISP2		0x13c
#define CAR_SRC_I2C2		0x198
#define CAR_SRC_I2C4		0x3c4
#define CAR_SRC_SOR1		0x410
#define CAR_SRC_PLLD2_OUT0	(5u << 29)
// sor1: sel1 picks the clock switch over the 24 MHz safe clock, sel0 the brick's own clock
#define CAR_SOR1_SEL1		(1u << 15)
#define CAR_SOR1_SEL0		(1u << 14)

// plld2 (tegra x1 trm 5.2.173, 5.2.174, 5.2.213)
#define CAR_PLLD2_BASE		0x4b8
#define CAR_PLLD2_MISC		0x4bc
#define CAR_PLLD2_SS_CFG	0x570
#define PLLD2_BYPASS		(1u << 31)
#define PLLD2_ENABLE		(1u << 30)
#define PLLD2_REF_DIS		(1u << 29)
#define PLLD2_LOCK		(1u << 27)
#define PLLD2_REF_SRC_MASK	(3u << 25)
#define PLLD2_LOCK_OVERRIDE	(1u << 24)
#define PLLD2_PDIV_SHIFT	19
#define PLLD2_IDDQ		(1u << 18)
#define PLLD2_NDIV_SHIFT	8
#define PLLD2_MISC_EN_LCKDET	(1u << 30)
#define PLLD2_SS_EN_SDM		(1u << 31)
#define PLLD2_SS_EN_SSC		(1u << 30)
// vco and comparison limits from freebsd tegra210_clk_pll.c (plld2_set_freq)
#define PLLD2_VCO_MIN		750000000ULL
#define PLLD2_VCO_MAX		1500000000ULL
#define PLLD2_PFD_MIN		12000000ULL
#define PLLD2_PFD_MAX		38400000ULL

// pmc words (tegra x1 trm 12.6.111, 12.6.112): code 1 takes the hdmi pad out of deep power down
#define PMC_IO_DPD_REQ		0x1b8
#define PMC_IO_DPD_STATUS	0x1bc
#define PMC_DPD_CODE_OFF	(1u << 30)
#define PMC_DPD_HDMI		(1u << 28)
#define PMC_PWRGATE_STATUS	0x38
#define PMC_CLAMP_STATUS	0x2c

// mc (tegra x1 trm 18.11.1): dc and dcb flush bits in hotreset ctrl 0, the smmu client words
#define MC_SMMU_CONFIG		0x010
#define MC_HOTRESET_CTRL	0x200
#define MC_HOTRESET_STATUS	0x204
#define MC_SMMU_DC_ASID		0x240
#define MC_SMMU_DCB_ASID	0x244

// dpaux1 drives the ddc pads (tegra x1 trm 9.x table 39, 12.2.7.3 and 27.11.20-21)
#define DPAUX_HYBRID_PADCTL	0x124
#define DPAUX_HYBRID_SPARE	0x134
#define DPAUX_PAD_I2C		0xc001

// netbsd tegra_i2c.c init: 20.4 MHz source over 26 * 8 is about 100 kHz
#define I2C_CNFG		0x00
#define I2C_SL_CNFG		0x20
#define I2C_FIFO_CONTROL	0x5c
#define I2C_INTERRUPT_MASK	0x64
#define I2C_CLK_DIVISOR		0x6c
#define I2C_BUS_CONFIG_LOAD	0x8c
#define I2C_SRC_DIV_20M4	19

// ddc and the hdmi 5 V switch: the tca9539 at 0x74 on gen2 i2c, pin 12 per the l4t tree
#define DDC_EDID_ADDR		0x50
#define TCA9539_ADDR		0x74
#define TCA9539_OUT1		0x03
#define TCA9539_CFG1		0x07
#define TCA9539_HDMI_BIT	(1u << 4)

// hpd on gpio cc1: bank 7 is gpio-8, port 0 in_0 at 0x30 (tegra x1 trm 9.x gpio)
#define GPIO_IN_0		0x30
#define GPIO_HPD_BIT		(1u << 1)

// dc words (tegra x1 trm 24.5-24.11), window a by its direct range at 0xb80 and 0xbc0
#define DC_CMD_DISPLAY_COMMAND	0x032
#define DC_CMD_POWER_CONTROL	0x036
#define DC_CMD_INT_STATUS	0x037
#define DC_CMD_STATE_ACCESS	0x040
#define DC_CMD_STATE_CONTROL	0x041
#define DC_CMD_WINDOW_HEADER	0x042
#define DC_DISP_SIGNAL_OPTIONS0	0x400
#define DC_DISP_WIN_OPTIONS	0x402
#define DC_DISP_TIMING_OPTIONS	0x405
#define DC_DISP_REF_TO_SYNC	0x406
#define DC_DISP_SYNC_WIDTH	0x407
#define DC_DISP_BACK_PORCH	0x408
#define DC_DISP_ACTIVE		0x409
#define DC_DISP_FRONT_PORCH	0x40a
#define DC_DISP_H_PULSE2_CTRL	0x415
#define DC_DISP_H_PULSE2_POS_A	0x416
#define DC_DISP_CLOCK_CONTROL	0x42e
#define DC_DISP_INTERFACE_CTRL	0x42f
#define DC_DISP_COLOR_CONTROL	0x430
#define DC_DISP_BLEND_BG_COLOR	0x4e4
#define DC_WIN_OPTIONS		0xb80
#define DC_WIN_BYTE_SWAP	0xb81
#define DC_WIN_COLOR_DEPTH	0xb83
#define DC_WIN_POSITION		0xb84
#define DC_WIN_SIZE		0xb85
#define DC_WIN_PRESCALED_SIZE	0xb86
#define DC_WIN_H_INITIAL_DDA	0xb87
#define DC_WIN_V_INITIAL_DDA	0xb88
#define DC_WIN_DDA_INCREMENT	0xb89
#define DC_WIN_LINE_STRIDE	0xb8a
#define DC_WINBUF_START_ADDR	0xbc0
#define DC_WINBUF_H_OFFSET	0xbc6
#define DC_WINBUF_V_OFFSET	0xbc8
#define DC_WINBUF_SURFACE_KIND	0xbcb
#define DC_WINBUF_START_ADDR_HI	0xbcd

#define DC_CTRL_MODE_C_DISPLAY	(1u << 5)
// pw0-pw4 together as the trm asks, plus pm0 and pm1
#define DC_POWER_ALL		0x00050155u
#define DC_WRITE_MUX_ACTIVE	(1u << 2)
#define DC_READ_MUX_ACTIVE	(1u << 0)
#define DC_GENERAL_ACT_REQ	(1u << 0)
#define DC_WIN_A_ACT_REQ	(1u << 1)
#define DC_GENERAL_UPDATE	(1u << 8)
#define DC_WIN_A_UPDATE		(1u << 9)
#define DC_WINDOW_A_SELECT	(1u << 4)
#define DC_SOR1_TIMING_HDMI	(1u << 27)
#define DC_SOR1_ENABLE		(1u << 26)
#define DC_H_PULSE2_ENABLE	(1u << 12)
// h pulse 2: last point end a at 11:8, qualified by vertical active at 7:6
#define DC_H_PULSE2_CTRL_HDMI	((1u << 8) | (2u << 6))
#define DC_BASE_COLOR_888	8u
#define DC_WIN_ENABLE		(1u << 30)
// t_a8r8g8b8: alpha, red, green, blue from msb to lsb of a 32-bit word
#define DC_WIN_DEPTH_A8R8G8B8	12u
#define DC_DDA_ONE		0x1000u

// sor1 byte offsets (tegra x1 trm 27.8)
#define SOR_SUPER_STATE0	0x004
#define SOR_SUPER_STATE1	0x008
#define SOR_STATE0		0x00c
#define SOR_STATE1		0x010
#define SOR_HEAD_STATE0(h)	(0x014 + 4 * (h))
#define SOR_HEAD_STATE1(h)	(0x01c + 4 * (h))
#define SOR_HEAD_STATE2(h)	(0x024 + 4 * (h))
#define SOR_HEAD_STATE3(h)	(0x02c + 4 * (h))
#define SOR_HEAD_STATE4(h)	(0x034 + 4 * (h))
#define SOR_CLK_CNTRL		0x04c
#define SOR_PWR			0x054
#define SOR_PLL0		0x05c
#define SOR_PLL1		0x060
#define SOR_PLL2		0x064
#define SOR_PLL3		0x068
#define SOR_CSTM		0x06c
#define SOR_SEQ_CTL		0x080
#define SOR_LANE_SEQ_CTL	0x084
#define SOR_SEQ_INST(n)		(0x088 + 4 * (n))
#define SOR_XBAR_CTRL		0x128
#define SOR_DP_PADCTL0		0x170
#define SOR_DP_SPARE0		0x180
#define SOR_PLL4		0x1c8
#define SOR_HDMI_CTRL		0x300
#define SOR_REFCLK		0x398
#define SOR_INPUT_CONTROL	0x3a0

#define SOR_SUPER_ATTACHED	(1u << 3)
#define SOR_SUPER_NORMAL	(1u << 2)
#define SOR_SUPER_AWAKE		2u
#define SOR_STATE1_OWNER_HEAD0	1u
#define SOR_STATE1_CRC_COMPLETE	(1u << 6)
#define SOR_STATE1_SINGLE_TMDS_A (1u << 8)
#define SOR_STATE1_HSYNC_NEG	(1u << 12)
#define SOR_STATE1_VSYNC_NEG	(1u << 13)
#define SOR_STATE1_BPP_24_444	(5u << 17)
#define SOR_PLL0_PWR_OFF	(1u << 0)
#define SOR_PLL0_VCOPD		(1u << 2)
#define SOR_PLL2_AUX2		(1u << 18)
#define SOR_PLL2_AUX6_BANDGAP_PD (1u << 22)
#define SOR_PLL2_AUX7_PORT_PD	(1u << 23)
#define SOR_PLL2_AUX8_CAPPD	(1u << 24)
#define SOR_PLL3_PLLVDD_3V3	(1u << 13)
#define SOR_PADCTL_PU_TXD_ALL	0xfu
// lane sequencer: trigger, sequence down, power up, 15 us between lanes
#define SOR_LANE_SEQ_PU_HDMI	((1u << 31) | (1u << 20) | (15u << 12))
#define SOR_LANE_SEQ_PENDING	(1u << 31)
// tmds below 340 MHz: link speed g2_7 at 6:2, clock select single_pclk at 1:0
#define SOR_CLK_CNTRL_HDMI	(10u << 2)
#define SOR_SPARE_SEQ_ENABLE	(1u << 0)
#define SOR_SPARE_MACRO_SORCLK	(1u << 2)
#define SOR_SPARE_PREAMBLE_CYA	(1u << 3)
// pd_pc and pd_pc_alt at 8, pu entry points at 0
#define SOR_SEQ_CTL_HDMI	((8u << 12) | (8u << 8))
// wait one vsync, halt, pins a and b low, pwm out low, outputs enabled
#define SOR_SEQ_INST_HDMI	((1u << 23) | (1u << 15) | (2u << 12) | 1u)

static IOMemoryMap *
map_phys(uint64_t phys, uint64_t len, volatile uint32_t **out)
{
	IOMemoryDescriptor *md = IOMemoryDescriptor::withPhysicalAddress(phys, len, kIODirectionInOut);
	IOMemoryMap *map = md != NULL ? md->map(kIOMapAnywhere | kIOMapInhibitCache) : NULL;

	OSSafeReleaseNULL(md);
	*out = map != NULL ? (volatile uint32_t *)map->getVirtualAddress() : NULL;
	return map;
}

// pmc words through the firmware's sip call: freebsd tegra210_pmc.c PMC_SMC, read 0xaa, write 0xbb
static uint32_t
pmc_read(uint32_t off)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xaa;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = 0;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
	return x0 == 0 ? (uint32_t)x1 : 0xdeadbeef;
}

static void
pmc_write(uint32_t off, uint32_t val)
{
	register uint64_t x0 __asm__("x0") = 0xc2fffe00ull;
	register uint64_t x1 __asm__("x1") = 0xbb;
	register uint64_t x2 __asm__("x2") = off;
	register uint64_t x3 __asm__("x3") = val;

	__asm__ volatile ("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3) : : "memory");
}

static bool
compat_has(OSData *compat, const char *want)
{
	const char *s = compat != NULL ? (const char *)compat->getBytesNoCopy() : NULL;
	unsigned int len = compat != NULL ? compat->getLength() : 0;

	for (unsigned int o = 0; s != NULL && o < len; o += strnlen(s + o, len - o) + 1) {
		if (strncmp(s + o, want, len - o) == 0) return true;
	}
	return false;
}

static uint32_t
boot_arg(const char *name, uint32_t def)
{
	uint32_t v = def;

	PE_parse_boot_argn(name, &v, sizeof(v));
	return v;
}

IOService *
PDTegraDisplay::probe(IOService *provider, SInt32 *score)
{
	IODeviceMemory *mem = provider->getDeviceMemoryWithIndex(0);
	uint64_t want = boot_arg("pdtegradc", 0) == 1 ? kPhysDCB : kPhysDCA;

	// off unless asked for, so other tx1 runs that carry this kext are not disturbed
	if (boot_arg("pdtegradisp", 0) == 0) return NULL;
	IOLog("PDTegraDisplay: probe %s at 0x%llx\n", provider->getName(),
	    mem != NULL ? (unsigned long long)mem->getPhysicalAddress() : 0ULL);
	if (mem == NULL || mem->getPhysicalAddress() != want) return NULL;
	return super::probe(provider, score);
}

uint32_t
PDTegraDisplay::dtCell(OSData *d, unsigned int i) const
{
	uint32_t v;

	if (d == NULL || d->getLength() < (i + 1) * 4) return 0;
	v = ((const uint32_t *)d->getBytesNoCopy())[i];
	return fDTBigEndian ? OSSwapBigToHostInt32(v) : v;
}

IORegistryEntry *
PDTegraDisplay::dtChild(IORegistryEntry *parent, const char *name) const
{
	OSIterator *it = parent != NULL ? parent->getChildIterator(gIODTPlane) : NULL;
	IORegistryEntry *e, *found = NULL;

	while (found == NULL && it != NULL && (e = OSDynamicCast(IORegistryEntry, it->getNextObject())) != NULL) {
		if (strcmp(e->getName(gIODTPlane), name) == 0) found = e;
	}
	OSSafeReleaseNULL(it);
	return found;
}

bool
PDTegraDisplay::mapBlocks(IOService *provider)
{
	IORegistryEntry *root = IORegistryEntry::fromPath("/", gIODTPlane);
	OSIterator *it = root != NULL ? IORegistryIterator::iterateOver(root, gIODTPlane, kIORegistryIterateRecursively) : NULL;
	IORegistryEntry *e;

	fDCMap = provider->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	fDC = fDCMap != NULL ? (volatile uint32_t *)fDCMap->getVirtualAddress() : NULL;
	fSORMap = map_phys(kPhysSOR1, 0x1000, &fSOR);
	fCARMap = map_phys(kPhysCAR, 0x1000, &fCAR);
	fAuxMap = map_phys(kPhysDPAUX1, 0x1000, &fAux);
	fI2C2Map = map_phys(kPhysI2C2, 0x100, &fI2C2);
	fI2C4Map = map_phys(kPhysI2C4, 0x100, &fI2C4);
	fMCMap = map_phys(kPhysMC, 0x1000, &fMC);
	fGPIOMap = map_phys(kPhysGPIO8, 0x100, &fGPIO);

	// the sor1 node carries the board's lane crossbar and pad tuning
	while (fSORNode == NULL && it != NULL && (e = OSDynamicCast(IORegistryEntry, it->getNextObject())) != NULL) {
		if (compat_has(OSDynamicCast(OSData, e->getProperty("compatible")), "nvidia,tegra210-sor1")) {
			fSORNode = e;
			fSORNode->retain();
		}
	}
	OSSafeReleaseNULL(it);
	OSSafeReleaseNULL(root);
	if (fSORNode != NULL) {
		OSData *reg = OSDynamicCast(OSData, fSORNode->getProperty("reg"));
		const uint8_t *p = reg != NULL && reg->getLength() >= 8 ? (const uint8_t *)reg->getBytesNoCopy() : NULL;
		uint64_t be = 0;

		for (int i = 0; p != NULL && i < 8; i++) {
			be = be << 8 | p[i];
		}
		fDTBigEndian = be == kPhysSOR1;
		IOLog("PDTegraDisplay: sor1 node %s, cells %s-endian\n", fSORNode->getName(gIODTPlane),
		    fDTBigEndian ? "big" : "little");
	}
	return fDC != NULL && fSOR != NULL && fCAR != NULL && fAux != NULL && fI2C2 != NULL &&
	    fI2C4 != NULL && fMC != NULL && fGPIO != NULL;
}

void
PDTegraDisplay::dumpState(const char *when)
{
	IOLog("PDTegraDisplay: %s car enb l 0x%08x x 0x%08x y 0x%08x rst l 0x%08x x 0x%08x y 0x%08x\n", when,
	    car(0x010), car(0x280), car(0x298), car(0x004), car(0x28c), car(0x2a4));
	IOLog("PDTegraDisplay: %s plld2 0x%08x misc 0x%08x ss 0x%08x disp1 0x%08x disp2 0x%08x sor1 0x%08x\n", when,
	    car(CAR_PLLD2_BASE), car(CAR_PLLD2_MISC), car(CAR_PLLD2_SS_CFG), car(CAR_SRC_DISP1), car(CAR_SRC_DISP2),
	    car(CAR_SRC_SOR1));
	IOLog("PDTegraDisplay: %s pmc pwrgate 0x%08x clamp 0x%08x dpd 0x%08x\n", when,
	    pmc_read(PMC_PWRGATE_STATUS), pmc_read(PMC_CLAMP_STATUS), pmc_read(PMC_IO_DPD_STATUS));
	IOLog("PDTegraDisplay: %s mc smmu 0x%08x hotreset 0x%08x/0x%08x dc asid 0x%08x dcb asid 0x%08x\n", when,
	    fMC[MC_SMMU_CONFIG / 4], fMC[MC_HOTRESET_CTRL / 4], fMC[MC_HOTRESET_STATUS / 4],
	    fMC[MC_SMMU_DC_ASID / 4], fMC[MC_SMMU_DCB_ASID / 4]);
}

// the trm's dis and sor ungating order (tegra x1 trm 12.2.7.1-3): power, clocks, clamps, resets, mc flush
bool
PDTegraDisplay::powerUp(void)
{
	enum tegra_powergate_id dis = fHead == 0 ? TEGRA_POWERGATE_DIS : TEGRA_POWERGATE_DISB;
	uint32_t dispClk = fHead == 0 ? CLK_DISP1 : CLK_DISP2;
	uint32_t flush = fHead == 0 ? (1u << 2) : (1u << 3);
	int rv;

	// host1x carries the display register bus and dpaux access
	tegra210_pgate_set_gate(CLK_HOST1X, true);
	tegra210_hwreset_by_idx(CLK_HOST1X, false);

	if (!tegra_powergate_is_powered(dis)) {
		tegra210_hwreset_by_idx(dispClk, true);
		rv = tegra_powergate_power_on(dis);
		if (rv != 0) {
			IOLog("PDTegraDisplay: display partition %d did not power up (%d)\n", dis, rv);
			return false;
		}
	}
	tegra210_pgate_set_gate(dispClk, true);
	tegra_powergate_remove_clamping(dis);
	IODelay(10);
	tegra210_hwreset_by_idx(dispClk, false);
	fMC[MC_HOTRESET_CTRL / 4] &= ~flush;

	if (!tegra_powergate_is_powered(TEGRA_POWERGATE_SOR)) {
		tegra210_hwreset_by_idx(CLK_SOR1, true);
		tegra210_hwreset_by_idx(CLK_DPAUX1, true);
		rv = tegra_powergate_power_on(TEGRA_POWERGATE_SOR);
		if (rv != 0) {
			IOLog("PDTegraDisplay: sor partition did not power up (%d)\n", rv);
			return false;
		}
	}
	// sor1 starts on the safe clock, its source already pointing at plld2_out0 for the switch later
	carw(CAR_SRC_SOR1, CAR_SRC_PLLD2_OUT0);
	tegra210_pgate_set_gate(CLK_SOR_SAFE, true);
	tegra210_pgate_set_gate(CLK_SOR1, true);
	tegra210_pgate_set_gate(CLK_DPAUX1, true);
	tegra_powergate_remove_clamping(TEGRA_POWERGATE_SOR);
	IODelay(10);
	tegra210_hwreset_by_idx(CLK_SOR1, false);
	tegra210_hwreset_by_idx(CLK_DPAUX1, false);
	return true;
}

// a controller left unclocked by the firmware gets the netbsd tegra_i2c.c set-up at 100 kHz
bool
PDTegraDisplay::i2cInit(volatile uint32_t *regs, uint32_t clkIdx, uint32_t srcOff, const char *name)
{
	bool on = false;
	int retry;

	tegra210_pgate_get_gate(clkIdx, &on);
	if (on && tegra210_hwreset_is_asserted(clkIdx) == 0) {
		IOLog("PDTegraDisplay: %s already clocked, cnfg 0x%08x div 0x%08x\n", name, regs[I2C_CNFG / 4],
		    regs[I2C_CLK_DIVISOR / 4]);
		return true;
	}
	tegra210_hwreset_by_idx(clkIdx, true);
	carw(srcOff, I2C_SRC_DIV_20M4);
	tegra210_pgate_set_gate(clkIdx, true);
	IODelay(5);
	tegra210_hwreset_by_idx(clkIdx, false);
	IODelay(5);

	regs[I2C_CLK_DIVISOR / 4] = (0x19u << 16) | 0x1u;
	regs[I2C_INTERRUPT_MASK / 4] = 0;
	// new master fsm and packet mode
	regs[I2C_CNFG / 4] = (1u << 11) | (1u << 10);
	regs[I2C_SL_CNFG / 4] |= 1u << 2;
	regs[I2C_FIFO_CONTROL / 4] = 7u << 5;
	regs[I2C_BUS_CONFIG_LOAD / 4] = 1u;
	__asm__ volatile ("dsb sy" ::: "memory");
	for (retry = 1000; retry > 0 && regs[I2C_BUS_CONFIG_LOAD / 4] != 0; retry--) {
		IODelay(10);
	}
	IOLog("PDTegraDisplay: %s set up, config load %s\n", name, retry > 0 ? "done" : "timed out");
	return retry > 0;
}

// the l4t tree's vdd-hdmi rail: the board's hdmi 5 V, which the sink needs to see a source
void
PDTegraDisplay::hdmi5V(void)
{
	struct tegra_i2c_poll bus = { fI2C2, 1 };
	uint8_t out = 0, cfg = 0, reg;
	uint8_t buf[2];
	int e1, e2;

	if (boot_arg("pdhdmi5v", 1) == 0) return;
	if (!i2cInit(fI2C2, CLK_I2C2, CAR_SRC_I2C2, "gen2 i2c")) return;
	reg = TCA9539_OUT1;
	e1 = tegra_i2c_poll_exec(&bus, TCA9539_ADDR, &reg, 1, &out, 1, true);
	reg = TCA9539_CFG1;
	e2 = tegra_i2c_poll_exec(&bus, TCA9539_ADDR, &reg, 1, &cfg, 1, true);
	IOLog("PDTegraDisplay: tca9539 out1 0x%02x cfg1 0x%02x (%d %d)\n", out, cfg, e1, e2);
	if (e1 != 0 || e2 != 0) return;

	// output register first, so the pin comes up high when it turns into an output
	buf[0] = TCA9539_OUT1;
	buf[1] = out | TCA9539_HDMI_BIT;
	e1 = tegra_i2c_poll_exec(&bus, TCA9539_ADDR, NULL, 0, buf, 2, false);
	buf[0] = TCA9539_CFG1;
	buf[1] = cfg & ~TCA9539_HDMI_BIT;
	e2 = tegra_i2c_poll_exec(&bus, TCA9539_ADDR, NULL, 0, buf, 2, false);
	IOLog("PDTegraDisplay: hdmi 5 V on (%d %d)\n", e1, e2);
	IOSleep(5);
}

void
PDTegraDisplay::readEDID(void)
{
	struct tegra_i2c_poll bus = { fI2C4, 3 };
	uint8_t off = 0, sum = 0;
	int err = -1;

	IOLog("PDTegraDisplay: hpd %s\n", (fGPIO[GPIO_IN_0 / 4] & GPIO_HPD_BIT) ? "high" : "low");
	fAux[DPAUX_HYBRID_PADCTL / 4] = DPAUX_PAD_I2C;
	fAux[DPAUX_HYBRID_SPARE / 4] = 0;
	__asm__ volatile ("dsb sy" ::: "memory");
	if (!i2cInit(fI2C4, CLK_I2C4, CAR_SRC_I2C4, "ddc i2c4")) return;

	for (int attempt = 0; attempt < 3; attempt++) {
		err = tegra_i2c_poll_exec(&bus, DDC_EDID_ADDR, &off, 1, fEDID, sizeof(fEDID), true);
		if (err == 0) break;
		IOSleep(20);
	}
	if (err != 0) {
		IOLog("PDTegraDisplay: no edid over ddc (%d)\n", err);
		return;
	}
	for (unsigned int i = 0; i < sizeof(fEDID); i++) {
		sum += fEDID[i];
	}
	fHaveEDID = sum == 0 && fEDID[0] == 0x00 && fEDID[1] == 0xff;
	for (unsigned int i = 0; i < sizeof(fEDID); i += 32) {
		IOLog("PDTegraDisplay: edid %02x: %08x %08x %08x %08x %08x %08x %08x %08x\n", i,
		    OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i]), OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 4]),
		    OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 8]), OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 12]),
		    OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 16]), OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 20]),
		    OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 24]), OSSwapBigToHostInt32(*(uint32_t *)&fEDID[i + 28]));
	}
	IOLog("PDTegraDisplay: edid checksum %s\n", fHaveEDID ? "good" : "bad");
	if (fHaveEDID) setProperty("IODisplayEDID", fEDID, sizeof(fEDID));
}

// the first detailed timing of the base block is the preferred mode (vesa e-edid 1.4, 3.10.2)
void
PDTegraDisplay::pickMode(void)
{
	// cea-861 1920x1080p60 when there is no usable edid
	static const struct pd_tegra_mode k1080p = { 148500, 1920, 88, 44, 148, 1080, 4, 5, 36, true, true };
	static const struct pd_tegra_mode k720p = { 74250, 1280, 110, 40, 220, 720, 5, 5, 20, true, true };
	const uint8_t *d = &fEDID[0x36];
	struct pd_tegra_mode m;
	uint32_t hblank, vblank;

	// 1080p scans out scrambled so far (disp8-12) and 720p clean (disp13), so 720p unless pdhdmi720=0
	fMode = boot_arg("pdhdmi720", 1) ? k720p : k1080p;
	if (!fHaveEDID || boot_arg("pdhdmi720", 1) || (d[0] | d[1]) == 0) {
		IOLog("PDTegraDisplay: fixed mode %ux%u, %u kHz\n", fMode.hactive, fMode.vactive, fMode.clock_khz);
		return;
	}
	m.clock_khz = (uint32_t)(d[0] | d[1] << 8) * 10;
	m.hactive = d[2] | (d[4] >> 4) << 8;
	hblank = d[3] | (d[4] & 0xf) << 8;
	m.vactive = d[5] | (d[7] >> 4) << 8;
	vblank = d[6] | (d[7] & 0xf) << 8;
	m.hfp = d[8] | ((d[11] >> 6) & 3) << 8;
	m.hsw = d[9] | ((d[11] >> 4) & 3) << 8;
	m.vfp = (d[10] >> 4) | ((d[11] >> 2) & 3) << 4;
	m.vsw = (d[10] & 0xf) | (d[11] & 3) << 4;
	m.hbp = hblank - m.hfp - m.hsw;
	m.vbp = vblank - m.vfp - m.vsw;
	// digital separate sync carries the polarities in bits 2 and 1
	m.vpos = (d[17] & 0x18) == 0x18 ? (d[17] & 0x4) != 0 : true;
	m.hpos = (d[17] & 0x18) == 0x18 ? (d[17] & 0x2) != 0 : true;
	IOLog("PDTegraDisplay: edid preferred %ux%u, %u kHz, h %u/%u/%u v %u/%u/%u, %c%c\n", m.hactive, m.vactive,
	    m.clock_khz, m.hfp, m.hsw, m.hbp, m.vfp, m.vsw, m.vbp, m.hpos ? '+' : '-', m.vpos ? '+' : '-');

	// single-link tmds without deep colour tops out well below this, interlaced is not handled
	if (m.clock_khz > 165000 || m.clock_khz < 20000 || (d[17] & 0x80) || hblank <= m.hfp + m.hsw ||
	    vblank <= m.vfp + m.vsw) {
		IOLog("PDTegraDisplay: preferred mode not usable, falling back to %ux%u\n", fMode.hactive, fMode.vactive);
		return;
	}
	fMode = m;
}

// plld2 runs at twice the pixel clock, the fixed /2 after it feeds the display and sor1 (tegra x1 trm 5.1)
bool
PDTegraDisplay::setPixelClock(void)
{
	static const uint32_t pdivs[][2] = {
		{ 1, 0 }, { 2, 1 }, { 3, 2 }, { 4, 3 }, { 5, 4 }, { 6, 5 }, { 8, 6 }, { 9, 7 }, { 10, 8 },
		{ 12, 9 }, { 15, 10 }, { 16, 11 }, { 18, 12 }, { 20, 13 }, { 24, 14 }, { 30, 15 }, { 32, 16 },
	};
	uint64_t fin = tegra210_osc_div_freq(), target = (uint64_t)fMode.clock_khz * 2000;
	uint64_t bestErr = ~0ULL, vco, pfd, got;
	uint32_t bm = 0, bn = 0, bp = 0, bpv = 0, base, n;
	int i;

	for (unsigned int pi = 0; pi < sizeof(pdivs) / sizeof(pdivs[0]); pi++) {
		uint32_t p = pdivs[pi][0];

		if (target * p < PLLD2_VCO_MIN) continue;
		if (target * p > PLLD2_VCO_MAX) break;
		for (uint32_t m = 1; m < 256; m++) {
			pfd = fin / m;
			if (pfd < PLLD2_PFD_MIN) break;
			if (pfd > PLLD2_PFD_MAX) continue;
			n = (uint32_t)((target * p * m + fin / 2) / fin);
			if (n == 0 || n > 255) continue;
			vco = fin * n / m;
			if (vco < PLLD2_VCO_MIN || vco > PLLD2_VCO_MAX) continue;
			got = vco / p;
			if ((got > target ? got - target : target - got) < bestErr) {
				bestErr = got > target ? got - target : target - got;
				bm = m;
				bn = n;
				bp = p;
				bpv = pdivs[pi][1];
			}
		}
	}
	if (bm == 0 || bestErr * 200 > target) {
		IOLog("PDTegraDisplay: no plld2 setting within 0.5%% of %llu Hz (ref %llu)\n", target, fin);
		return false;
	}

	// the trm's pll start: out of iddq, wait, program while disabled, enable, wait for lock (5.1.5)
	base = car(CAR_PLLD2_BASE);
	base &= ~(PLLD2_ENABLE | PLLD2_IDDQ);
	carw(CAR_PLLD2_BASE, base);
	IODelay(5);
	carw(CAR_PLLD2_SS_CFG, car(CAR_PLLD2_SS_CFG) & ~(PLLD2_SS_EN_SDM | PLLD2_SS_EN_SSC));
	base &= ~(PLLD2_BYPASS | PLLD2_REF_DIS | PLLD2_REF_SRC_MASK | PLLD2_LOCK_OVERRIDE | (0x1fu << PLLD2_PDIV_SHIFT) |
	    (0xffu << PLLD2_NDIV_SHIFT) | 0xffu);
	base |= bpv << PLLD2_PDIV_SHIFT | bn << PLLD2_NDIV_SHIFT | bm;
	carw(CAR_PLLD2_BASE, base);
	carw(CAR_PLLD2_MISC, car(CAR_PLLD2_MISC) | PLLD2_MISC_EN_LCKDET);
	carw(CAR_PLLD2_BASE, base | PLLD2_ENABLE);
	for (i = 500; i > 0 && (car(CAR_PLLD2_BASE) & PLLD2_LOCK) == 0; i--) {
		IODelay(10);
	}
	got = fin * bn / bm / bp / 2;
	IOLog("PDTegraDisplay: plld2 m %u n %u p %u from %llu Hz, pixel clock %llu Hz, %s (base 0x%08x)\n", bm, bn, bp,
	    fin, got, i > 0 ? "locked" : "no lock", car(CAR_PLLD2_BASE));
	fMode.clock_khz = (uint32_t)(got / 1000);

	carw(fHead == 0 ? CAR_SRC_DISP1 : CAR_SRC_DISP2, CAR_SRC_PLLD2_OUT0);
	return i > 0;
}

// one scanout buffer at the current pitch, contiguous and below 16 GB
IOBufferMemoryDescriptor *
PDTegraDisplay::allocScanout(uint64_t *phys)
{
	IOBufferMemoryDescriptor *b;
	IOByteCount seg = 0;
	uint64_t size = ((uint64_t)fPitch * fHeight + 0x3fff) & ~0x3fffULL;

	// the window takes 34-bit addresses through start_addr_hi (tegra x1 trm 24.11.9)
	// write-combined pages, so no alias of them holds lines the dc cannot see
	b = IOBufferMemoryDescriptor::inTaskWithPhysicalMask(kernel_task,
	    kIODirectionInOut | kIOMemoryPhysicallyContiguous | kIOMapWriteCombineCache, size,
	    0x00000003fffff000ULL);
	if (b == NULL) {
		IOLog("PDTegraDisplay: no %llu bytes of contiguous memory for a scanout buffer\n", size);
		return NULL;
	}
	*phys = b->getPhysicalSegment(0, &seg, kIOMemoryMapperNone);
	if (b->getBytesNoCopy() == NULL || *phys == 0 || seg < size) {
		b->release();
		return NULL;
	}
	return b;
}

bool
PDTegraDisplay::allocFramebuffer(void)
{
	fWidth = fMode.hactive;
	fHeight = fMode.vactive;
	fPitch = (fWidth * 4 + 255) & ~255u;
	fBuf = allocScanout(&fFBPhys);
	if (fBuf == NULL) return false;
	// the buffer's own cached mapping, flushed after each fill since the dc does not snoop
	fFB = (volatile uint32_t *)fBuf->getBytesNoCopy();
	IOLog("PDTegraDisplay: framebuffer %ux%u pitch %u at 0x%llx, %llu bytes\n", fWidth, fHeight, fPitch,
	    fFBPhys, (unsigned long long)fBuf->getLength());
	return true;
}

// eight colour bars, a grey ramp below and a white border, in x8r8g8b8
void
PDTegraDisplay::fillPattern(void)
{
	static const uint32_t bars[8] = {
		0xffffff, 0xffff00, 0x00ffff, 0x00ff00, 0xff00ff, 0xff0000, 0x0000ff, 0x202020,
	};
	uint32_t split = fHeight * 2 / 3;

	for (uint32_t y = 0; y < fHeight; y++) {
		volatile uint32_t *row = fFB + (uint64_t)y * (fPitch / 4);

		for (uint32_t x = 0; x < fWidth; x++) {
			uint32_t g = x * 255 / (fWidth - 1);
			uint32_t c = y < split ? bars[x * 8 / fWidth] : (g << 16 | g << 8 | g);

			if (x < 8 || y < 8 || x >= fWidth - 8 || y >= fHeight - 8) c = 0xffffff;
			row[x] = c;
		}
	}
	fBuf->performOperation(kIOMemoryIncoherentIOFlush, 0, fBuf->getLength());
	__asm__ volatile ("dsb sy" ::: "memory");
}

// display controller set-up after freebsd tegra_dc.c and the trm's hdmi start (27.7.2.2), window a only
void
PDTegraDisplay::setupDC(void)
{
	const struct pd_tegra_mode *m = &fMode;
	uint32_t pulse;

	dcw(DC_CMD_STATE_ACCESS, DC_WRITE_MUX_ACTIVE | DC_READ_MUX_ACTIVE);
	dcw(DC_CMD_POWER_CONTROL, DC_POWER_ALL);
	dcw(DC_DISP_WIN_OPTIONS, DC_SOR1_ENABLE | DC_SOR1_TIMING_HDMI);

	dcw(DC_DISP_TIMING_OPTIONS, 1);
	dcw(DC_DISP_REF_TO_SYNC, 1u << 16 | 1u);
	dcw(DC_DISP_SYNC_WIDTH, m->vsw << 16 | m->hsw);
	dcw(DC_DISP_BACK_PORCH, m->vbp << 16 | m->hbp);
	dcw(DC_DISP_FRONT_PORCH, m->vfp << 16 | m->hfp);
	dcw(DC_DISP_ACTIVE, m->vactive << 16 | m->hactive);
	dcw(DC_DISP_INTERFACE_CTRL, 0);
	dcw(DC_DISP_CLOCK_CONTROL, 0);
	dcw(DC_DISP_COLOR_CONTROL, DC_BASE_COLOR_888);

	// the hdmi preamble pulse: starts 10 pixels before active, 8 wide
	pulse = 1 + m->hsw + m->hbp - 10;
	dcw(DC_DISP_SIGNAL_OPTIONS0, DC_H_PULSE2_ENABLE);
	dcw(DC_DISP_H_PULSE2_CTRL, DC_H_PULSE2_CTRL_HDMI);
	dcw(DC_DISP_H_PULSE2_POS_A, (pulse + 8) << 16 | pulse);
	// dark blue behind the window, visible even if the window never fetches
	dcw(DC_DISP_BLEND_BG_COLOR, boot_arg("pdhdmibg", 0xff600000));

	dcw(DC_CMD_WINDOW_HEADER, DC_WINDOW_A_SELECT);
	dcw(DC_WIN_COLOR_DEPTH, DC_WIN_DEPTH_A8R8G8B8);
	dcw(DC_WIN_BYTE_SWAP, 0);
	dcw(DC_WIN_POSITION, 0);
	dcw(DC_WIN_SIZE, fHeight << 16 | fWidth);
	dcw(DC_WIN_PRESCALED_SIZE, fHeight << 16 | fWidth * 4);
	dcw(DC_WIN_H_INITIAL_DDA, 0);
	dcw(DC_WIN_V_INITIAL_DDA, 0);
	dcw(DC_WIN_DDA_INCREMENT, DC_DDA_ONE << 16 | DC_DDA_ONE);
	dcw(DC_WIN_LINE_STRIDE, fPitch);
	dcw(DC_WINBUF_START_ADDR, (uint32_t)fFBPhys);
	dcw(DC_WINBUF_START_ADDR_HI, (uint32_t)(fFBPhys >> 32));
	dcw(DC_WINBUF_H_OFFSET, 0);
	dcw(DC_WINBUF_V_OFFSET, 0);
	dcw(DC_WINBUF_SURFACE_KIND, 0);
	dcw(DC_WIN_OPTIONS, boot_arg("pdhdmiwin", 1) ? DC_WIN_ENABLE : 0);

	dcw(DC_CMD_DISPLAY_COMMAND, DC_CTRL_MODE_C_DISPLAY);
	dcw(DC_CMD_STATE_CONTROL, DC_GENERAL_UPDATE | DC_WIN_A_UPDATE);
	dcw(DC_CMD_STATE_CONTROL, DC_GENERAL_ACT_REQ | DC_WIN_A_ACT_REQ);
	IOSleep(50);
	for (uint32_t w = 0xb80; w < 0xbd0; w += 8) {
		IOLog("PDTegraDisplay: dc %03x: %08x %08x %08x %08x %08x %08x %08x %08x\n", w, dc(w), dc(w + 1),
		    dc(w + 2), dc(w + 3), dc(w + 4), dc(w + 5), dc(w + 6), dc(w + 7));
	}
	for (uint32_t w = 0x400; w < 0x418; w += 8) {
		IOLog("PDTegraDisplay: dc %03x: %08x %08x %08x %08x %08x %08x %08x %08x\n", w, dc(w), dc(w + 1),
		    dc(w + 2), dc(w + 3), dc(w + 4), dc(w + 5), dc(w + 6), dc(w + 7));
	}
	IOLog("PDTegraDisplay: dc running, cmd 0x%08x state 0x%08x int 0x%08x win 0x%08x addr 0x%08x\n",
	    dc(DC_CMD_DISPLAY_COMMAND), dc(DC_CMD_STATE_CONTROL), dc(DC_CMD_INT_STATUS), dc(DC_WIN_OPTIONS),
	    dc(DC_WINBUF_START_ADDR));
}

// the l4t tree's sor1 prod triples read as (offset, bits kept, bits set): the only reading under which
// every triple in the tree is self-consistent (our own reading of cboot's tree)
void
PDTegraDisplay::applyProd(void)
{
	IORegistryEntry *prod = dtChild(fSORNode, "prod-settings"), *best = NULL;
	uint32_t bestMHz = ~0u, pclkMHz = (fMode.clock_khz + 999) / 1000;
	OSIterator *it;
	IORegistryEntry *e;
	const char *names[2];

	if (prod == NULL) {
		IOLog("PDTegraDisplay: sor1 has no prod-settings, pad tuning left at reset values\n");
		return;
	}
	// the generic set, then the smallest prod_c_<n>M at or above the pixel clock
	it = prod->getChildIterator(gIODTPlane);
	while (it != NULL && (e = OSDynamicCast(IORegistryEntry, it->getNextObject())) != NULL) {
		const char *n = e->getName(gIODTPlane);
		uint32_t mhz = 0;

		if (strncmp(n, "prod_c_", 7) != 0 || n[7] < '0' || n[7] > '9') continue;
		for (const char *c = n + 7; *c >= '0' && *c <= '9'; c++) {
			mhz = mhz * 10 + (uint32_t)(*c - '0');
		}
		if (mhz >= pclkMHz && mhz < bestMHz) {
			bestMHz = mhz;
			best = e;
		}
	}
	OSSafeReleaseNULL(it);
	names[0] = "prod";
	names[1] = best != NULL ? best->getName(gIODTPlane) : NULL;
	for (int s = 0; s < 2; s++) {
		IORegistryEntry *node = names[s] != NULL ? dtChild(prod, names[s]) : NULL;
		OSData *d = node != NULL ? OSDynamicCast(OSData, node->getProperty("prod")) : NULL;
		unsigned int n = d != NULL ? d->getLength() / 12 : 0;

		for (unsigned int i = 0; i < n; i++) {
			uint32_t off = dtCell(d, i * 3), keep = dtCell(d, i * 3 + 1), set = dtCell(d, i * 3 + 2);

			if (off >= 0x1000 || (off & 3)) continue;
			sorw(off, (sor(off) & keep) | set);
		}
		if (names[s] != NULL)
			IOLog("PDTegraDisplay: sor1 %s, %u settings\n", names[s], n);
	}
}

void
PDTegraDisplay::sorUpdate(uint32_t reg)
{
	sorw(reg, 0);
	sorw(reg, 1);
	sorw(reg, 0);
}

// the trm's hdmi start (tegra x1 trm 27.7.2.2): pad power, lanes, sequencer, pad clock, heads, attach
bool
PDTegraDisplay::setupSOR(void)
{
	const struct pd_tegra_mode *m = &fMode;
	uint32_t h = fHead, quarterMHz = fMode.clock_khz / 250, v, state1, i;
	OSData *xbar = fSORNode != NULL ? OSDynamicCast(OSData, fSORNode->getProperty("nvidia,xbar-ctrl")) : NULL;

	// 8.2 divider that makes the sequencer's 1 us tick from the pixel clock
	sorw(SOR_REFCLK, (quarterMHz >> 2) << 8 | (quarterMHz & 3) << 6);
	applyProd();
	// the board's lane order, five 3-bit selects from bit 2 with bypass and swap clear (27.8.27)
	if (xbar != NULL && xbar->getLength() >= 20) {
		v = 0;
		for (i = 0; i < 5; i++) {
			v |= (dtCell(xbar, i) & 7) << (2 + 3 * i);
		}
		sorw(SOR_XBAR_CTRL, v);
	}
	sorw(SOR_INPUT_CONTROL, h == 0 ? 0 : 1);

	// pad power-up with the trm's waits
	pmc_write(PMC_IO_DPD_REQ, PMC_DPD_CODE_OFF | PMC_DPD_HDMI);
	IODelay(50);
	IOLog("PDTegraDisplay: hdmi pad dpd status 0x%08x\n", pmc_read(PMC_IO_DPD_STATUS));
	sorw(SOR_PLL2, sor(SOR_PLL2) & ~SOR_PLL2_AUX6_BANDGAP_PD);
	IODelay(20);
	sorw(SOR_PLL3, sor(SOR_PLL3) | SOR_PLL3_PLLVDD_3V3);
	sorw(SOR_PLL0, sor(SOR_PLL0) & ~(SOR_PLL0_PWR_OFF | SOR_PLL0_VCOPD));
	sorw(SOR_PLL2, sor(SOR_PLL2) & ~SOR_PLL2_AUX8_CAPPD);
	IODelay(200);
	sorw(SOR_PLL2, sor(SOR_PLL2) & ~(SOR_PLL2_AUX7_PORT_PD | SOR_PLL2_AUX2));
	IODelay(20);
	sorw(SOR_DP_PADCTL0, sor(SOR_DP_PADCTL0) | SOR_PADCTL_PU_TXD_ALL);
	sorw(SOR_LANE_SEQ_CTL, SOR_LANE_SEQ_PU_HDMI);
	for (i = 100; i > 0 && (sor(SOR_LANE_SEQ_CTL) & SOR_LANE_SEQ_PENDING); i--) {
		IODelay(10);
	}
	IOLog("PDTegraDisplay: lane sequencer %s, lane_seq 0x%08x pll0 0x%08x pll2 0x%08x pll4 0x%08x\n",
	    i > 0 ? "done" : "stuck", sor(SOR_LANE_SEQ_CTL), sor(SOR_PLL0), sor(SOR_PLL2), sor(SOR_PLL4));

	// pll2 pdiv at 7:4 resets to by_2, pdhdmipdiv picks the macro's post divider for a/b tests (27.8.18)
	v = boot_arg("pdhdmipdiv", 0xff);
	if (v != 0xff) sorw(SOR_PLL2, (sor(SOR_PLL2) & ~0xf0u) | (v & 0xf) << 4);
	sorw(SOR_CLK_CNTRL, SOR_CLK_CNTRL_HDMI);
	sorw(SOR_DP_SPARE0, sor(SOR_DP_SPARE0) | SOR_SPARE_SEQ_ENABLE);
	sorw(SOR_SEQ_CTL, SOR_SEQ_CTL_HDMI);
	sorw(SOR_SEQ_INST(0), SOR_SEQ_INST_HDMI);
	sorw(SOR_SEQ_INST(8), SOR_SEQ_INST_HDMI);

	// the sor clock off while it switches to the pad's feedback clock (27.6.6)
	tegra210_pgate_set_gate(CLK_SOR1, false);
	carw(CAR_SRC_SOR1, CAR_SRC_PLLD2_OUT0 | CAR_SOR1_SEL1 | CAR_SOR1_SEL0);
	tegra210_pgate_set_gate(CLK_SOR1, true);
	IODelay(20);
	v = sor(SOR_DP_SPARE0);
	if (boot_arg("pdhdmicya", 1)) v |= SOR_SPARE_PREAMBLE_CYA;
	if (boot_arg("pdhdmimacroclk", 1)) v |= SOR_SPARE_MACRO_SORCLK;
	sorw(SOR_DP_SPARE0, v);

	// hdmi mode: rekey 56 at 6:0 and as many 32-pixel packets as fit in the blank at 20:16 (27.8.140)
	if (boot_arg("pdhdmimode", 1)) {
		uint32_t pk = (m->hfp + m->hsw + m->hbp - 56 - 18) / 32;

		sorw(SOR_HDMI_CTRL, (1u << 30) | (pk & 0x1f) << 16 | 56u);
	}
	state1 = (SOR_STATE1_OWNER_HEAD0 + h) | SOR_STATE1_SINGLE_TMDS_A | SOR_STATE1_CRC_COMPLETE | SOR_STATE1_BPP_24_444;
	if (!m->hpos) state1 |= SOR_STATE1_HSYNC_NEG;
	if (!m->vpos) state1 |= SOR_STATE1_VSYNC_NEG;
	state1 = boot_arg("pdhdmistate1", state1);
	sorw(SOR_STATE1, state1);
	sorw(SOR_HEAD_STATE0(h), 0);
	sorw(SOR_HEAD_STATE1(h), (m->vactive + m->vfp + m->vsw + m->vbp) << 16 | (m->hactive + m->hfp + m->hsw + m->hbp));
	sorw(SOR_HEAD_STATE2(h), (1 + m->vsw) << 16 | (1 + m->hsw));
	sorw(SOR_HEAD_STATE3(h), (m->vsw + m->vbp) << 16 | (m->hsw + m->hbp));
	sorw(SOR_HEAD_STATE4(h), (m->vsw + m->vbp + m->vactive) << 16 | (m->hsw + m->hbp + m->hactive));
	sorUpdate(SOR_STATE0);

	sorw(SOR_SUPER_STATE1, SOR_SUPER_ATTACHED);
	sorUpdate(SOR_SUPER_STATE0);
	IOSleep(40);
	sorw(SOR_SUPER_STATE1, SOR_SUPER_ATTACHED | SOR_SUPER_NORMAL);
	sorUpdate(SOR_SUPER_STATE0);
	IOSleep(40);
	sorw(SOR_SUPER_STATE1, SOR_SUPER_ATTACHED | SOR_SUPER_NORMAL | SOR_SUPER_AWAKE);
	sorUpdate(SOR_SUPER_STATE0);
	IOSleep(40);

	// the normal state to power up runs the sequencer, a stuck vsync wait gets a forced switch (27.8.15)
	if (boot_arg("pdhdmipwr", 1)) {
		sorw(SOR_PWR, (1u << 31) | 1u);
		for (i = 100; i > 0 && (sor(SOR_PWR) & (1u << 31)); i--) {
			IODelay(1000);
		}
		if (i == 0) sorw(SOR_SEQ_CTL, sor(SOR_SEQ_CTL) | (1u << 30));
		IOLog("PDTegraDisplay: sor1 power up %s, pwr 0x%08x\n", i > 0 ? "done" : "forced", sor(SOR_PWR));
	}
	IOSleep(100);

	IOLog("PDTegraDisplay: sor1 state1 0x%08x super 0x%08x pwr 0x%08x seq 0x%08x lane 0x%08x clk 0x%08x\n",
	    sor(SOR_STATE1), sor(SOR_SUPER_STATE1), sor(SOR_PWR), sor(SOR_SEQ_CTL), sor(SOR_LANE_SEQ_CTL),
	    sor(SOR_CLK_CNTRL));
	IOLog("PDTegraDisplay: sor1 pll0-4 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x cstm 0x%08x xbar 0x%08x spare 0x%08x\n",
	    sor(SOR_PLL0), sor(SOR_PLL1), sor(SOR_PLL2), sor(SOR_PLL3), sor(SOR_PLL4), sor(SOR_CSTM), sor(SOR_XBAR_CTRL),
	    sor(SOR_DP_SPARE0));
	IOLog("PDTegraDisplay: sor1 padctl 0x%08x refclk 0x%08x input 0x%08x hdmi 0x%08x, dc int 0x%08x\n",
	    sor(SOR_DP_PADCTL0), sor(SOR_REFCLK), sor(SOR_INPUT_CONTROL), sor(SOR_HDMI_CTRL), dc(DC_CMD_INT_STATUS));
	return true;
}

bool
PDTegraDisplay::start(IOService *provider)
{
	struct tegra210_gpu_bus bus;
	uint64_t total;

	fHead = provider->getDeviceMemoryWithIndex(0)->getPhysicalAddress() == kPhysDCB ? 1 : 0;
	if (!mapBlocks(provider)) {
		IOLog("PDTegraDisplay: cannot map the display blocks\n");
		return false;
	}
	bzero(&bus, sizeof(bus));
	bus.car = fCAR;
	bus.pmc_read = pmc_read;
	bus.pmc_write = pmc_write;
	tegra210_gpu_attach(&bus);
	IOLog("PDTegraDisplay: dc %c, osc %llu Hz, osc_div %llu Hz\n", fHead == 0 ? 'a' : 'b',
	    tegra210_osc_freq(), tegra210_osc_div_freq());
	dumpState("before");

	if (!powerUp()) return false;
	hdmi5V();
	readEDID();
	pickMode();
	if (!setPixelClock() && boot_arg("pdhdminolock", 0) == 0) return false;
	if (!allocFramebuffer()) return false;
	fillPattern();
	setupDC();
	setupSOR();
	dumpState("after");

	total = (uint64_t)(fMode.hactive + fMode.hfp + fMode.hsw + fMode.hbp) *
	    (fMode.vactive + fMode.vfp + fMode.vsw + fMode.vbp);
	fRefreshHz = total != 0 ? (uint32_t)(((uint64_t)fMode.clock_khz * 1000 + total / 2) / total) : 60;
	IOLog("PDTegraDisplay: %ux%u at %u Hz on hdmi\n", fWidth, fHeight, fRefreshHz);
	// the window's underflow counter over one second (tegra x1 trm 24.11.6)
	for (int t = 0; t < 3; t++) {
		uint32_t u0 = dc(0xbca);

		IOSleep(1000);
		IOLog("PDTegraDisplay: win a underflow count 0x%08x -> 0x%08x\n", u0, dc(0xbca));
	}

	fFlipLock = IOLockAlloc();
	if (fFlipLock == NULL || !super::start(provider)) return false;
	setProperty("pd-tegra-head", fHead, 32);
	setProperty("pd-framebuffer-phys", fFBPhys, 64);
	// the dc reads dram without snooping, so user mappings of the framebuffer are write-combined
	setProperty("pd-vram-write-combine", true);
	registerService();
	return true;
}

void
PDTegraDisplay::stop(IOService *provider)
{
	super::stop(provider);
}

void
PDTegraDisplay::free(void)
{
	OSSafeReleaseNULL(fFBMap);
	OSSafeReleaseNULL(fBuf);
	for (int i = 0; i < 2; i++) {
		OSSafeReleaseNULL(fFlip[i]);
	}
	if (fFlipLock != NULL) IOLockFree(fFlipLock);
	OSSafeReleaseNULL(fDCMap);
	OSSafeReleaseNULL(fSORMap);
	OSSafeReleaseNULL(fCARMap);
	OSSafeReleaseNULL(fAuxMap);
	OSSafeReleaseNULL(fI2C2Map);
	OSSafeReleaseNULL(fI2C4Map);
	OSSafeReleaseNULL(fMCMap);
	OSSafeReleaseNULL(fGPIOMap);
	OSSafeReleaseNULL(fSORNode);
	super::free();
}

IOReturn
PDTegraDisplay::enableController(void)
{
	return kIOReturnSuccess;
}

const char *
PDTegraDisplay::getPixelFormats(void)
{
	return IO32BitDirectPixels;
}

IOItemCount
PDTegraDisplay::getDisplayModeCount(void)
{
	return 1;
}

IOReturn
PDTegraDisplay::getDisplayModes(IODisplayModeID *allDisplayModes)
{
	*allDisplayModes = kModeID;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplay::getInformationForDisplayMode(IODisplayModeID, IODisplayModeInformation *info)
{
	bzero(info, sizeof(*info));
	info->nominalWidth = fWidth;
	info->nominalHeight = fHeight;
	info->refreshRate = fRefreshHz << 16;
	info->maxDepthIndex = kDepth;
	return kIOReturnSuccess;
}

UInt64
PDTegraDisplay::getPixelFormatsForDisplayMode(IODisplayModeID, IOIndex)
{
	return 0;
}

IOReturn
PDTegraDisplay::getPixelInformation(IODisplayModeID mode, IOIndex depth, IOPixelAperture aperture,
    IOPixelInformation *info)
{
	if (mode != kModeID || depth != kDepth || aperture != kIOFBSystemAperture) return kIOReturnUnsupportedMode;
	bzero(info, sizeof(*info));
	info->activeWidth = fWidth;
	info->activeHeight = fHeight;
	info->bytesPerRow = fPitch;
	strlcpy(info->pixelFormat, IO32BitDirectPixels, sizeof(info->pixelFormat));
	info->pixelType = kIORGBDirectPixels;
	info->componentMasks[0] = 0x00ff0000;
	info->componentMasks[1] = 0x0000ff00;
	info->componentMasks[2] = 0x000000ff;
	info->bitsPerPixel = 32;
	info->componentCount = 3;
	info->bitsPerComponent = 8;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplay::getCurrentDisplayMode(IODisplayModeID *mode, IOIndex *depth)
{
	*mode = kModeID;
	*depth = kDepth;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplay::setDisplayMode(IODisplayModeID mode, IOIndex depth)
{
	return mode == kModeID && depth == kDepth ? kIOReturnSuccess : kIOReturnUnsupportedMode;
}

IODeviceMemory *
PDTegraDisplay::getApertureRange(IOPixelAperture aperture)
{
	if (aperture != kIOFBSystemAperture || fFBPhys == 0) return NULL;
	return IODeviceMemory::withRange(fFBPhys, (IOByteCount)fPitch * fHeight);
}

IODeviceMemory *
PDTegraDisplay::getVRAMRange(void)
{
	return getApertureRange(kIOFBSystemAperture);
}

// window a's next buffer: written to the assembly copy and armed, then taken by the dc
// at the next frame start, so the buffer it leaves is no longer read
void
PDTegraDisplay::scanFrom(uint64_t phys, bool log)
{
	uint64_t t0 = mach_absolute_time(), ns;
	uint32_t i;

	__asm__ volatile ("dsb sy" ::: "memory");
	// write assembly, read active (tegra x1 trm 24.6.27)
	dcw(DC_CMD_STATE_ACCESS, DC_READ_MUX_ACTIVE);
	// window a selected for the winbuf words (tegra x1 trm 24.6.29)
	dcw(DC_CMD_WINDOW_HEADER, DC_WINDOW_A_SELECT);
	// start address low and high words (tegra x1 trm 24.11.1, 24.11.9)
	dcw(DC_WINBUF_START_ADDR, (uint32_t)phys);
	dcw(DC_WINBUF_START_ADDR_HI, (uint32_t)(phys >> 32));
	// update arms window a, act_req then activates it, in two writes (tegra x1 trm 24.6.28)
	dcw(DC_CMD_STATE_CONTROL, DC_WIN_A_UPDATE);
	dcw(DC_CMD_STATE_CONTROL, DC_WIN_A_ACT_REQ);
	// act_req reads 1 while pending and 0 once taken (tegra x1 trm 24.6.28), bounded at 100 ms
	for (i = 0; i < 1000 && (dc(DC_CMD_STATE_CONTROL) & DC_WIN_A_ACT_REQ); i++) {
		IODelay(100);
	}
	absolutetime_to_nanoseconds(mach_absolute_time() - t0, &ns);
	if (i == 1000 || log)
		IOLog("PDTegraDisplay: scanout 0x%llx %s after %llu us, state 0x%08x\n", phys,
		    i == 1000 ? "not taken" : "taken", ns / 1000, dc(DC_CMD_STATE_CONTROL));
}

// a finished frame from the presenter: returns once the dc scans it out
void
PDTegraDisplay::flip(int buffer, uint64_t *seq, uint64_t *ns)
{
	IOLockLock(fFlipLock);
	scanFrom(fFlipPhys[buffer], fFlipSeq < 3);
	*seq = ++fFlipSeq;
	IOLockUnlock(fFlipLock);
	absolutetime_to_nanoseconds(mach_absolute_time(), ns);
}

// the compositor went: the console's buffer shows again
void
PDTegraDisplay::presenterGone(void)
{
	IOLockLock(fFlipLock);
	scanFrom(fFBPhys, true);
	fFlipSeq = 0;
	IOLockUnlock(fFlipLock);
}

// the presenter neuswc opens, the same contract as PDVirtualFramebuffer's
#define kPresenterType		'pdpf'

enum {
	kPresenterGetInfo = 0,		// out: width, height, pitch, buffer count
	kPresenterFlip = 1,		// in: buffer, out: sequence, time in ns
	kPresenterMethodCount = 2,
};

class PDTegraDisplayPresenter : public IOUserClient
{
	OSDeclareDefaultStructors(PDTegraDisplayPresenter);

	PDTegraDisplay *fFramebuffer;

	static IOReturn getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args);
	static IOReturn flip(OSObject *target, void *reference, IOExternalMethodArguments *args);

public:
	bool start(IOService *provider) override;
	IOReturn clientClose(void) override;
	IOReturn clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory) override;
	IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *args,
	    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference) override;
};

OSDefineMetaClassAndStructors(PDTegraDisplayPresenter, IOUserClient);

bool
PDTegraDisplayPresenter::start(IOService *provider)
{
	fFramebuffer = OSDynamicCast(PDTegraDisplay, provider);
	return fFramebuffer != NULL && IOUserClient::start(provider);
}

IOReturn
PDTegraDisplayPresenter::clientClose(void)
{
	fFramebuffer->presenterGone();
	terminate();
	return kIOReturnSuccess;
}

// by physical range like the aperture, write-combined as the dc reads dram without snooping
IOReturn
PDTegraDisplayPresenter::clientMemoryForType(UInt32 type, IOOptionBits *options, IOMemoryDescriptor **memory)
{
	IOBufferMemoryDescriptor *b = fFramebuffer->flipBuffer(type);
	IOMemoryDescriptor *m;

	if (b == NULL) return kIOReturnBadArgument;
	m = IOMemoryDescriptor::withPhysicalAddress(fFramebuffer->flipPhys(type), b->getLength(), kIODirectionInOut);
	IOLog("PDTegraDisplay: presenter maps buffer %u, %s\n", (unsigned)type, m != NULL ? "ok" : "failed");
	if (m == NULL) return kIOReturnNoMemory;
	*memory = m;
	*options = kIOMapWriteCombineCache;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplayPresenter::getInfo(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDTegraDisplay *fb = ((PDTegraDisplayPresenter *)target)->fFramebuffer;

	args->scalarOutput[0] = fb->width();
	args->scalarOutput[1] = fb->height();
	args->scalarOutput[2] = fb->pitch();
	args->scalarOutput[3] = 2;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplayPresenter::flip(OSObject *target, void *reference, IOExternalMethodArguments *args)
{
	PDTegraDisplay *fb = ((PDTegraDisplayPresenter *)target)->fFramebuffer;
	uint64_t seq, ns;

	if (args->scalarInput[0] > 1) return kIOReturnBadArgument;
	fb->flip((int)args->scalarInput[0], &seq, &ns);
	args->scalarOutput[0] = seq;
	args->scalarOutput[1] = ns;
	return kIOReturnSuccess;
}

IOReturn
PDTegraDisplayPresenter::externalMethod(uint32_t selector, IOExternalMethodArguments *args,
    IOExternalMethodDispatch *dispatch, OSObject *target, void *reference)
{
	static const IOExternalMethodDispatch methods[kPresenterMethodCount] = {
		{ &PDTegraDisplayPresenter::getInfo, 0, 0, 4, 0 },
		{ &PDTegraDisplayPresenter::flip, 1, 0, 2, 0 },
	};

	if (selector >= kPresenterMethodCount) return kIOReturnBadArgument;
	return IOUserClient::externalMethod(selector, args, (IOExternalMethodDispatch *)&methods[selector], this,
	    reference);
}

// the flip pair is made on the first presenter open and kept, cleared to black
IOReturn
PDTegraDisplay::newUserClient(task_t owningTask, void *securityID, UInt32 type, IOUserClient **handler)
{
	PDTegraDisplayPresenter *client;

	if (type != kPresenterType) return super::newUserClient(owningTask, securityID, type, handler);
	IOLockLock(fFlipLock);
	for (int i = 0; i < 2; i++) {
		if (fFlip[i] != NULL) continue;
		fFlip[i] = allocScanout(&fFlipPhys[i]);
		if (fFlip[i] == NULL) break;
		bzero(fFlip[i]->getBytesNoCopy(), fFlip[i]->getLength());
		fFlip[i]->performOperation(kIOMemoryIncoherentIOFlush, 0, fFlip[i]->getLength());
		IOLog("PDTegraDisplay: flip buffer %d at 0x%llx\n", i, fFlipPhys[i]);
	}
	IOLockUnlock(fFlipLock);
	if (fFlip[0] == NULL || fFlip[1] == NULL) {
		IOLog("PDTegraDisplay: no memory for the flip pair\n");
		return kIOReturnNoMemory;
	}

	client = OSTypeAlloc(PDTegraDisplayPresenter);
	if (client == NULL) return kIOReturnNoMemory;
	if (!client->initWithTask(owningTask, securityID, type) || !client->attach(this) || !client->start(this)) {
		client->detach(this);
		client->release();
		return kIOReturnError;
	}
	*handler = client;
	IOLog("PDTegraDisplay: presenter open\n");
	return kIOReturnSuccess;
}
