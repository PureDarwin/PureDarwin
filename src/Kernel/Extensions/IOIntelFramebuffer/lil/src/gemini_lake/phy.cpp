#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/gemini_lake/phy.hpp"
#include "src/gemini_lake/glk_regs.hpp"
#include "src/regs.hpp"

namespace glk::phy {

namespace {

using namespace glk::regs;

// PORT_TX voltage-swing field layout (BXT).
constexpr uint32_t TX_MARGIN_000_MASK   = (0xFFu << 16);
static inline uint32_t TX_MARGIN_000(uint32_t v)   { return (v & 0xFF) << 16; }
constexpr uint32_t TX_UNIQ_SCALE_MASK   = (0xFFu << 8);
static inline uint32_t TX_UNIQ_SCALE(uint32_t v)   { return (v & 0xFF) << 8; }
constexpr uint32_t TX_UNIQ_SCALE_EN     = (1u << 27); // DW3 oscaledcompmethod
constexpr uint32_t TX_DEEMPH_MASK       = (0xFFu << 24);
static inline uint32_t TX_DEEMPH(uint32_t v)       { return (v & 0xFF) << 24; }
constexpr uint32_t TX1_SWING_CALC_INIT  = (1u << 30);
constexpr uint32_t TX2_SWING_CALC_INIT  = (1u << 31);

constexpr uint32_t IREF_OFFSET_MASK     = (0xFFu << 8);
static inline uint32_t IREF_OFFSET(uint32_t v)     { return (v & 0xFF) << 8; }

bool is_enabled(Gpu *gpu, enum LilDdiId ddi) {
	return (REG(cl1cm_dw0(ddi)) & PHY_POWER_GOOD) &&
	       (REG(phy_ctl_family(ddi)) & COMMON_RESET_DIS);
}

bool is_accessible(Gpu *gpu, enum LilDdiId ddi) {
	uint32_t v = REG(cl1cm_dw0(ddi));
	return (v & PHY_POWER_GOOD) && !(v & PHY_RESERVED_BIT7);
}

// Common-lane bring-up for a single PHY (identified by any DDI it serves).
bool power_up_phy(Gpu *gpu, enum LilDdiId ddi) {
	uint32_t status = REG(cl1cm_dw0(ddi));
	if(!(status & PHY_POWER_GOOD) || (status & PHY_RESERVED_BIT7)) {
		uint32_t pwr = REG(P_CR_GT_DISP_PWRON);
		lil_log(VERBOSE, "glk::phy: DDI %c power request before=0x%x mask=0x%x status=0x%x\n",
		        '0' + ddi, pwr, phy_pwron_bit(ddi), status);
		REG(P_CR_GT_DISP_PWRON) = pwr | phy_pwron_bit(ddi);
		lil_log(VERBOSE, "glk::phy: DDI %c power request after=0x%x\n",
		        '0' + ddi, REG(P_CR_GT_DISP_PWRON));
	}

	for(unsigned i = 0; i < 100; i++) {
		if(is_accessible(gpu, ddi))
			break;
		lil_usleep(100);
	}
	if(!is_accessible(gpu, ddi)) {
		lil_log(WARNING, "glk::phy: PHY for DDI %c never became accessible; PORT_CL1CM_DW0=0x%x\n",
		        '0' + ddi, REG(cl1cm_dw0(ddi)));
		return false;
	}

	// iref offsets (Rcomp reference trim).
	REG(cl1cm_dw9(ddi))  = (REG(cl1cm_dw9(ddi))  & ~IREF_OFFSET_MASK) | IREF_OFFSET(0xE4);
	REG(cl1cm_dw10(ddi)) = (REG(cl1cm_dw10(ddi)) & ~IREF_OFFSET_MASK) | IREF_OFFSET(0xE4);

	// Sustained-clock config and dynamic power-down enables.
	REG(cl1cm_dw28(ddi)) = (REG(cl1cm_dw28(ddi)) & ~0x3u) | SUS_CLK_CONFIG
	                     | OCL1_POWER_DOWN_EN | DW28_OLDO_DYNPWRDOWNEN;
	return true;
}

} // namespace

void init(LilGpu *lil_gpu, enum LilDdiId ddi) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(is_enabled(gpu, ddi)) {
		lil_log(VERBOSE, "glk::phy: PHY for DDI %c already initialized; skipping\n", '0' + ddi);
		return;
	}

	if(is_accessible(gpu, ddi)) {
		lil_log(VERBOSE, "glk::phy: DDI %c powered by firmware; completing PHY init\n", '0' + ddi);
	} else {
		lil_log(VERBOSE, "glk::phy: initializing PHY for DDI %c\n", '0' + ddi);
	}

	enum LilDdiId master = DDI_A;
	bool is_master = (ddi == DDI_A);

	if(!is_master) {
		// Ensure the master PHY1 (DDI A) is up and calibrated so its GRC code is
		// available to copy, even when no DDI A display is attached.
		if(!is_enabled(gpu, master)) {
			if(!power_up_phy(gpu, master))
				return;
			REG(phy_ctl_family(master)) |= COMMON_RESET_DIS;
		}
		if(!wait_for_bit_set(REG_PTR(ref_dw3(master)), GRC_DONE, 100, 10))
			lil_log(WARNING, "glk::phy: Rcomp master grc_done timeout\n");
	}

	if(!power_up_phy(gpu, ddi))
		return;

	if(!is_master) {
		// Copy the master's GRC (fast) code into this slave PHY and override.
		uint32_t grc = (REG(ref_dw6(master)) >> 24) & 0xFF;
		REG(ref_dw6(ddi)) = (grc << 24) | (grc << 16) | grc;
		REG(ref_dw8(ddi)) |= GRCDIS | GRC_RDY_OVRD;
	}

	// Release this PHY's common reset (enables the lanes).
	REG(phy_ctl_family(ddi)) |= COMMON_RESET_DIS;

	if(is_master) {
		// The master runs GRC calibration once its common reset is released.
		if(!wait_for_bit_set(REG_PTR(ref_dw3(ddi)), GRC_DONE, 100, 10))
			lil_log(WARNING, "glk::phy: Rcomp master grc_done timeout\n");
	}
}

void hdmi_vswing(LilGpu *lil_gpu, enum LilDdiId ddi) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	// BXT HDMI full-swing default level (400/1000 mV, no de-emphasis). Programmed
	// into the group registers so every lane of the DDI updates together.
	constexpr uint32_t margin = 154;   // omargin000
	constexpr uint32_t scale  = 0x9A;  // ouniqtranscale
	constexpr uint32_t deemph = 128;   // ow2tapdeemph9p5

	// Clear calc init.
	REG(pcs_dw10_grp(ddi)) &= ~(TX1_SWING_CALC_INIT | TX2_SWING_CALC_INIT);

	// Program swing / scale / de-emphasis.
	REG(tx_dw2_grp(ddi)) = (REG(tx_dw2_grp(ddi)) & ~(TX_MARGIN_000_MASK | TX_UNIQ_SCALE_MASK))
	                     | TX_MARGIN_000(margin) | TX_UNIQ_SCALE(scale);
	REG(tx_dw3_grp(ddi)) |= TX_UNIQ_SCALE_EN;
	REG(tx_dw4_grp(ddi)) = (REG(tx_dw4_grp(ddi)) & ~TX_DEEMPH_MASK) | TX_DEEMPH(deemph);

	// Trigger the swing calculation update.
	REG(pcs_dw10_grp(ddi)) |= TX1_SWING_CALC_INIT | TX2_SWING_CALC_INIT;
}

void dp_vswing(LilGpu *lil_gpu, enum LilDdiId ddi, uint8_t vswing, uint8_t preemph) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	struct Trans { uint8_t margin, deemph; };
	static const Trans table[10] = {
		{ 52, 128 }, { 78, 85 }, { 104, 64 }, { 154, 43 }, // 400 mV, 0/3.5/6/9.5 dB
		{ 77, 128 }, { 116, 85 }, { 154, 64 },             // 600 mV, 0/3.5/6 dB
		{ 102, 128 }, { 154, 85 },                         // 800 mV, 0/3.5 dB
		{ 154, 128 },                                      // 1200 mV, 0 dB
	};
	static const uint8_t idx_of[4][4] = {
		{ 0, 1, 2, 3 },
		{ 4, 5, 6, 0xFF },
		{ 7, 8, 0xFF, 0xFF },
		{ 9, 0xFF, 0xFF, 0xFF },
	};

	uint8_t idx = idx_of[vswing & 3][preemph & 3];
	if(idx == 0xFF)
		idx = idx_of[vswing & 3][0]; // clamp to the highest legal pre-emph
	Trans t = table[idx];

	constexpr uint32_t scale = 0x9A;

	// Clear calc init, program swing / scale / de-emphasis into the DDI's group
	// registers (every lane at once), then re-trigger the swing calculation.
	REG(pcs_dw10_grp(ddi)) &= ~(TX1_SWING_CALC_INIT | TX2_SWING_CALC_INIT);
	REG(tx_dw2_grp(ddi)) = (REG(tx_dw2_grp(ddi)) & ~(TX_MARGIN_000_MASK | TX_UNIQ_SCALE_MASK))
	                     | TX_MARGIN_000(t.margin) | TX_UNIQ_SCALE(scale);
	REG(tx_dw3_grp(ddi)) |= TX_UNIQ_SCALE_EN;
	REG(tx_dw4_grp(ddi)) = (REG(tx_dw4_grp(ddi)) & ~TX_DEEMPH_MASK) | TX_DEEMPH(t.deemph);
	REG(pcs_dw10_grp(ddi)) |= TX1_SWING_CALC_INIT | TX2_SWING_CALC_INIT;
}

} // namespace glk::phy
