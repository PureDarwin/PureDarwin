#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/kaby_lake/cdclk.hpp"
#include "src/kaby_lake/pcode.hpp"
#include "src/pci.hpp"
#include "src/regs.hpp"

namespace {

struct {
	uint8_t select;
	int mb;
} graphics_mode_select_table[] = {
	{ 0, 0 }, { 1, 32 }, { 2, 64 }, { 3, 96 }, { 4, 128 }, { 5, 160 }, { 6, 192 }, { 7, 224 },
	{ 8, 256 }, { 9, 288 }, { 10, 320 }, { 11, 352 }, { 12, 384 }, { 13, 416 }, { 14, 448 }, { 15, 480 },
	{ 16, 512 }, { 32, 1024 }, { 48, 1536 }, { 64, 2048 },
	{ 240, 4 }, { 241, 8 }, { 242, 12 }, { 243, 16 }, { 244, 20 }, { 245, 24 }, { 246, 28 }, { 247, 32 },
	{ 248, 36 }, { 249, 40 }, { 250, 44 }, { 251, 48 }, { 252, 52 }, { 253, 56 }, { 254, 60 },
};

uint32_t transcoders[4] = { TRANSCODER_A_BASE, TRANSCODER_B_BASE, TRANSCODER_C_BASE, TRANSCODER_EDP_BASE };

constexpr size_t PORT_COUNT = 4;

static struct port_info {
	uint32_t strap_reg;
	uint32_t strap_mask;
	uint32_t hpd_enable_mask;
	uint32_t hpd_status_mask;
} ports[PORT_COUNT] = {
	{ DDI_BUF_CTL(0), (1 << 0), (1 << 28), (3 << 24), },
	{ SFUSE_STRAP, (1 << 2), (1 << 4), (3 << 0), },
	{ SFUSE_STRAP, (1 << 1), (1 << 12), (3 << 8), },
	{ SFUSE_STRAP, (1 << 0), (1 << 20), (3 << 16), },
};

} // namespace

namespace kbl::setup {

void setup(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	/* enable Bus Mastering and Memory + I/O space access */
	uint16_t command = gpu->pci_read<uint16_t>(PCI_HDR_COMMAND);
	gpu->pci_write<uint16_t>(PCI_HDR_COMMAND, command | 7); /* Bus Master | Memory Space | I/O Space */
	command = gpu->pci_read<uint16_t>(PCI_HDR_COMMAND);
	lil_assert(command & 2);

	lil_assert(gpu->pch_gen != INVALID_PCH_GEN);
	if(gpu->pch_gen == LPT) {
		/* LPT has this meme where apparently the reference clock is 125 MHz */
		gpu->ref_clock_freq = 125_MHz;
	} else {
		gpu->ref_clock_freq = 24_MHz;
	}

	if(gpu->pch_gen != NO_PCH)
		lil_log(VERBOSE, "\tPCH gen %u\n", gpu->pch_gen);

	/* read the `GMCH Graphics Control` register */
	uint8_t graphics_mode_select = gpu->pci_read<uint8_t>(0x51);
	size_t pre_allocated_memory_mb = 0;

	for(size_t i = 0; i < sizeof(graphics_mode_select_table) / sizeof(*graphics_mode_select_table); i++) {
		if(graphics_mode_select == graphics_mode_select_table[i].select) {
			pre_allocated_memory_mb = graphics_mode_select_table[i].mb;
			break;
		}
	}

	lil_log(VERBOSE, "%lu MiB pre-allocated memory\n", pre_allocated_memory_mb);

	gpu->stolen_memory_pages = (pre_allocated_memory_mb << 10) >> 2;

	uintptr_t bar0_base = 0;
    uintptr_t bar0_len = 0;
    lil_get_bar(gpu->dev, 0, &bar0_base, &bar0_len);

    gpu->mmio_start = bar0_base;
	gpu->gpio_start = 0xC0000;

	/* Half of the BAR space is registers, half is GTT PTEs */
    gpu->gtt_size = bar0_len / 2;
    gpu->gtt_address = gpu->mmio_start + (bar0_len / 2);

	uintptr_t bar2_base = 0;
    uintptr_t bar2_len = gpu->stolen_memory_pages << 12;
    lil_get_bar(gpu->dev, 2, &bar2_base, &bar2_len);
    gpu->vram = bar2_base;
}

// Gemini Lake / Broxton (gen9 LP) CD clock bringup. The CD clock is sourced from
// the DE PLL (not SKL's LCPLL1/DPLL_CTRL1). GLK's DE PLL runs off the 19.2 MHz
// non-SSC reference at ratio 33 (VCO 633.6 MHz); the CDCLK_CTL CD2X divider then
// selects the final CD clock. Follows the "Broxton Sequence for Changing CD Clock
// Frequency" (BXT PRM Vol 7 Display).
static void glk_set_cdclk(Gpu *gpu, uint32_t cdclk_khz) {
	// Map the target CD clock to a DE PLL ratio + CD2X divider. GLK VCO is always
	// 633.6 MHz (ratio 33); only the CD2X divider changes.
	uint32_t ratio = 33;          // 19.2 MHz * 33 = 633.6 MHz VCO
	uint32_t cd2x_div_sel;        // CDCLK_CTL bits 23:22 (0=/1, 2=/2, 3=/4)
	switch(cdclk_khz) {
		case 316800: cd2x_div_sel = 0; break; // CD2X 633.6, CD 316.8
		case 158400: cd2x_div_sel = 2; break; // CD2X 316.8, CD 158.4
		case 79200:  cd2x_div_sel = 3; break; // CD2X 158.4, CD 79.2
		default: lil_panic("glk_set_cdclk: unsupported CD clock");
	}

	// CD Frequency Decimal = round((cdclk_kHz - 1000) / 500), bits 10:0.
	uint32_t decimal = (cdclk_khz - 1000 + 250) / 500;

	// 1. Inform the power controller of the upcoming frequency change.
	uint32_t data0 = 0x80000000, data1 = 0, timeout = 150;
	if(!kbl::pcode::rw(gpu, &data0, &data1,
	                   pcode::Mailbox(HSW_PCODE_DE_WRITE_FREQ_REQ), &timeout))
		lil_panic("glk_set_cdclk: pcode prepare failed");

	// 2. Program and enable the DE PLL (must be disabled while changing the ratio).
	REG(DE_PLL_ENABLE) &= ~DE_PLL_ENABLE_PLL_ENABLE;
	wait_for_bit_unset(REG_PTR(DE_PLL_ENABLE), DE_PLL_ENABLE_PLL_LOCK, 200, 10);
	REG(DE_PLL_CTL) = (REG(DE_PLL_CTL) & ~DE_PLL_CTL_RATIO_MASK) | (ratio & DE_PLL_CTL_RATIO_MASK);
	REG(DE_PLL_ENABLE) |= DE_PLL_ENABLE_PLL_ENABLE;
	lil_assert(wait_for_bit_set(REG_PTR(DE_PLL_ENABLE), DE_PLL_ENABLE_PLL_LOCK, 200, 10));

	// 3. Select the CD2X divider and CD frequency decimal (preserve reserved bits).
	REG(CDCLK_CTL) = (REG(CDCLK_CTL) & ~(CDCLK_CTL_BXT_CD2X_DIV_MASK | CDCLK_CTL_DECIMAL_MASK))
	               | CDCLK_CTL_BXT_CD2X_DIV(cd2x_div_sel) | CDCLK_CTL_DECIMAL(decimal);

	// 4. Inform the power controller of the selected frequency: ceil(cdclk / 25 MHz).
	data0 = (cdclk_khz + 25000 - 1) / 25000;
	data1 = 0; timeout = 150;
	if(!kbl::pcode::rw(gpu, &data0, &data1,
	                   pcode::Mailbox(HSW_PCODE_DE_WRITE_FREQ_REQ), &timeout))
		lil_panic("glk_set_cdclk: pcode commit failed");

	gpu->cdclk_freq = cdclk_khz / 1000;

	// The boot-cdclk decode in kbl setup uses the SKL/KBL VCO decimal tables,
	// which don't contain the GLK CD frequency, so it lands at 0. Anchor the
	// boot value to what we just programmed so the DP path's "restore boot
	// cdclk" (dp.cpp) is a no-op instead of set_freq(gpu, 0) -> panic.
	gpu->boot_cdclk_freq = gpu->cdclk_freq;
}

// Broxton/Gemini Lake display init. Distinct from the SKL path: the driver drives
// PWR_WELL_CTL2 (CTL1 is BIOS-owned), there is no MISC_IO power well, the PCH reset
// handshake is disabled (not enabled), CD clock comes from the DE PLL, and PG2 is
// left for the mode set rather than enabled here (BXT PRM Vol 7 "Initialize Sequence").
static void initialize_display_glk(Gpu *gpu) {
	// Disable the PCH reset handshake.
	REG(NDE_RSTWRN_OPT) &= ~NDE_RSTWRN_OPT_RST_PCH_HANDSHAKE_EN;

	// Enable Power Well 1 (PG1) via the driver-owned CTL2.
	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG0, 5, 1));
	REG(PWR_WELL_CTL2) |= PWR_WELL_CTL2_POWER_WELL_1_REQUEST;
	lil_assert(wait_for_bit_set(REG_PTR(PWR_WELL_CTL2), PWR_WELL_CTL2_POWER_WELL_1_STATE, 10, 2));
	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG1, 5, 1));

	// Enable CD clock. 316.8 MHz covers 1080p60 with headroom.
	glk_set_cdclk(gpu, 316800);

	// Enable DBUF.
	REG(DBUF_CTL) |= DBUF_CTL_POWER_ENABLE;
	wait_for_bit_set(REG_PTR(DBUF_CTL), DBUF_CTL_POWER_STATE, 10, 2);

	// Enable Power Well 2 so the pipe / plane / transcoder / DDI register banks
	// are powered before the mode set touches them (they live in PG2).
	REG(PWR_WELL_CTL2) |= PWR_WELL_CTL2_POWER_WELL_2_REQUEST;
	lil_assert(wait_for_bit_set(REG_PTR(PWR_WELL_CTL2), PWR_WELL_CTL2_POWER_WELL_2_STATE, 20, 2));
	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG2, 5, 1));

	// DDI PHY init and MIPI are handled by the mode set path.
}

void initialize_display(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(gpu->subgen == SUBGEN_GEMINI_LAKE) {
		initialize_display_glk(gpu);
		return;
	}

	REG(GDT_CHICKEN_BITS) &= ~0x80;
	REG(NDE_RSTWRN_OPT) |= 0x10;

	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG0, 5, 1));

	REG(HSW_PWR_WELL_CTL1) |= HSW_PWR_WELL_CTL1_POWER_WELL_1_REQUEST | HSW_PWR_WELL_CTL1_POWER_WELL_MISC_IO_REQUEST;

	lil_assert(wait_for_bit_set(REG_PTR(HSW_PWR_WELL_CTL1), HSW_PWR_WELL_CTL1_POWER_WELL_MISC_IO_STATE | HSW_PWR_WELL_CTL1_POWER_WELL_1_STATE, 10, 2));
	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG1, 5, 1));

	lil_assert(REG(HSW_PWR_WELL_CTL1) & HSW_PWR_WELL_CTL1_POWER_WELL_1_STATE);

	REG(HSW_PWR_WELL_CTL1) |= HSW_PWR_WELL_CTL1_POWER_WELL_2_REQUEST;
	lil_assert(wait_for_bit_set(REG_PTR(HSW_PWR_WELL_CTL1), HSW_PWR_WELL_CTL1_POWER_WELL_2_STATE, 10, 2));
	lil_assert(wait_for_bit_set(REG_PTR(FUSE_STATUS), FUSE_STATUS_PG2, 5, 1));

	REG(CDCLK_CTL) = (REG(CDCLK_CTL) & 0xF3FFF800) | 0x80002A1;

	REG(DPLL_CTRL1) = (REG(DPLL_CTRL1) & ~(DPLL_CTRL1_PROGRAM_ENABLE(0) | DPLL_CTRL1_LINK_RATE_MASK(0))) | DPLL_CTRL1_PROGRAM_ENABLE(0) | DPLL_CTRL1_LINK_RATE(0, DPLL_CTRL1_LINK_RATE_810_MHZ);
	REG(LCPLL1_CTL) |= LCPLL1_ENABLE;

	lil_usleep(5000);

	lil_assert(wait_for_bit_set(REG_PTR(DPLL_STATUS), DPLL_STATUS_LOCK(0), 5000, 100));

	kbl::cdclk::set_freq(gpu, 338);

	REG(DBUF_CTL) |= DBUF_CTL_POWER_ENABLE;
	wait_for_bit_set(REG_PTR(DBUF_CTL), DBUF_CTL_POWER_STATE, 10, 2);

	if((gpu->variant == ULT || gpu->variant == ULX) && (gpu->pch_gen == LPT_LP/* || gpu->pch_gen == WPT_LP */)) {
		REG(SOUTH_DSPCLK_GATE_D) |= SOUTH_DSPCLK_GATE_D_PCH_LP_PARTITION_LEVEL_DISABLE;
	}
}

void psr_disable(LilGpu *gpu) {
	lil_log(VERBOSE, "disabling PSR for all transcoders\n");

	for(size_t i = 0; i < 4; i++) {
		uint32_t srd_ctl = transcoders[i] + SRD_CTL;
		uint32_t srd_status = transcoders[i] + SRD_STATUS;

		if(REG(srd_ctl) & (1 << 31)) {
			REG(srd_ctl) &= ~(1 << 31);
			wait_for_bit_unset(REG_PTR(srd_status), 7 << 29, 10, 1);
		}
	}
}

void hotplug_enable(LilGpu *gpu) {
	lil_log(VERBOSE, "enabling HPD for all ports\n");

	for(size_t i = 0; i < PORT_COUNT; i++) {
		struct port_info *info = &ports[i];

		if(REG(info->strap_reg) & info->strap_mask) {
			REG(SHOTPLUG_CTL) = (REG(SHOTPLUG_CTL) & 0x03030303) | info->hpd_enable_mask | info->hpd_status_mask;
		}
	}
}

} // namespace kbl::setup
