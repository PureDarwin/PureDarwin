#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/gemini_lake/pll.hpp"
#include "src/gemini_lake/glk_regs.hpp"
#include "src/regs.hpp"

namespace glk::pll {

namespace {

using namespace glk::regs;

// BXT/GLK Port PLL is driven from the 100 MHz Non-SSC reference. N is fixed at 1
// and M1 at 2; the VCO must land in [4.8, 6.7] GHz. For HDMI the PLL output
// (dot) equals the pixel clock, so vco = pixel * p1 * p2.
constexpr uint32_t REF_KHZ = 100000;
constexpr uint32_t M1 = 2;
constexpr uint64_t VCO_MIN = 4800000; // kHz
constexpr uint64_t VCO_MAX = 6700000; // kHz

struct Dividers {
	uint32_t p1, p2;
	uint32_t m2_int, m2_frac;
	bool m2_frac_en;
	uint64_t vco; // kHz
};

// Search for the divider set whose output is closest to the target pixel clock
// while keeping the VCO in range. Mirrors the chv/bxt divider math.
bool compute_dividers(uint32_t target_khz, Dividers *out) {
	bool found = false;
	uint32_t best_err = 0xFFFFFFFF;

	for(uint32_t p1 = 1; p1 <= 7; p1++) {
		for(uint32_t p2 = 1; p2 <= 31; p2++) {
			uint64_t p = (uint64_t)p1 * p2;
			uint64_t vco = (uint64_t)target_khz * p;
			if(vco < VCO_MIN || vco > VCO_MAX)
				continue;

			// m2 (22-bit fixed point) = vco * N * 2^22 / (ref * m1)
			uint64_t m2_x22 = (vco << 22) / ((uint64_t)REF_KHZ * M1);
			uint32_t m2_int = (uint32_t)(m2_x22 >> 22);
			if(m2_int < 2 || m2_int > 255)
				continue;

			// Realised VCO with the rounded m2, to score the candidate.
			uint64_t vco_real = ((uint64_t)REF_KHZ * M1 * m2_x22) >> 22;
			uint64_t dot = vco_real / p;
			uint32_t err = (dot > target_khz) ? (uint32_t)(dot - target_khz)
			                                  : (uint32_t)(target_khz - dot);
			if(err < best_err) {
				best_err = err;
				out->p1 = p1;
				out->p2 = p2;
				out->m2_int = m2_int;
				out->m2_frac = (uint32_t)(m2_x22 & PORT_PLL_M2_FRAC_MASK);
				out->m2_frac_en = out->m2_frac != 0;
				out->vco = vco_real;
				found = true;
			}
		}
	}

	return found;
}

// PORT_PLL_6/8/10 coefficient values by VCO band (BXT PRM).
struct BandCoeff { uint32_t prop, gain, integ, tdc, dcoamp; };

BandCoeff band_coefficients(uint64_t vco_khz) {
	if(vco_khz >= 6200000)          return { 4, 3, 9, 8, 15 };
	if(vco_khz > 5400000)           return { 5, 3, 11, 9, 15 };
	if(vco_khz == 5400000)          return { 3, 1, 8, 9, 15 };
	return { 5, 3, 11, 9, 15 }; // 4.8 <= vco < 5.4
}

// PORT_PCS_DW12 lane stagger by port symbol rate (MHz).
uint32_t lane_stagger(uint32_t pixel_khz) {
	uint32_t sr = pixel_khz / 1000;
	if(sr > 270) return 0x18;
	if(sr > 135) return 0x0D;
	if(sr > 67)  return 0x07;
	if(sr > 33)  return 0x04;
	return 0x02;
}

constexpr uint32_t LANESTAGGER_STRAP_OVRD = (1u << 6);

} // namespace

bool enable(LilGpu *lil_gpu, LilCrtc *crtc) {
	return enable_at(lil_gpu, crtc, crtc->current_mode.clock);
}

bool enable_at(LilGpu *lil_gpu, LilCrtc *crtc, uint32_t clock_khz) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	enum LilDdiId ddi = crtc->connector->ddi_id;
	uint32_t pixel_khz = clock_khz;

	// If firmware already has this Port PLL enabled and locked, inherit it rather
	// than reprogramming: a reprogram can leave the PLL reporting lock while its
	// output clock to the DDI is dead (transcoder access then wedges).
	uint32_t pll_state = REG(pll_enable(ddi));
	if((pll_state & PORT_PLL_ENABLE_BIT) && (pll_state & PORT_PLL_LOCK)) {
		lil_log(VERBOSE, "glk::pll: DDI %c inheriting live firmware PLL\n", '0' + ddi);
		return true;
	}

	Dividers d = {};
	if(!compute_dividers(pixel_khz, &d)) {
		lil_log(ERROR, "glk::pll: no valid dividers for %u kHz\n", pixel_khz);
		return false;
	}

	BandCoeff c = band_coefficients(d.vco);

	lil_log(VERBOSE, "glk::pll: DDI %c pixel=%u p1=%u p2=%u m2=%u.%u vco=%llu\n",
	        '0' + ddi, pixel_khz, d.p1, d.p2, d.m2_int, d.m2_frac,
	        (unsigned long long)d.vco);

	// Reference select: HDMI uses the Non-SSC reference. Keep PLL disabled.
	REG(pll_enable(ddi)) = REG(pll_enable(ddi)) & ~(PORT_PLL_ENABLE_BIT | PORT_PLL_REF_SEL);

	// Disable the 10-bit clock to the display engine while reprogramming.
	REG(pll_ebb4(ddi)) &= ~PORT_PLL_10BIT_CLK_ENABLE;

	// P1 / P2 dividers.
	REG(pll_ebb0(ddi)) = (REG(pll_ebb0(ddi)) & ~(PORT_PLL_P1_MASK | PORT_PLL_P2_MASK))
	                   | PORT_PLL_P1(d.p1) | PORT_PLL_P2(d.p2);

	// M2 integer, N, M2 fraction, fraction enable.
	REG(pll_dw(ddi, 0)) = (REG(pll_dw(ddi, 0)) & ~PORT_PLL_M2_INT_MASK) | (d.m2_int & PORT_PLL_M2_INT_MASK);
	REG(pll_dw(ddi, 1)) = (REG(pll_dw(ddi, 1)) & ~(0xFu << 8)) | (1u << 8); // N = 1
	REG(pll_dw(ddi, 2)) = (REG(pll_dw(ddi, 2)) & ~PORT_PLL_M2_FRAC_MASK) | (d.m2_frac & PORT_PLL_M2_FRAC_MASK);
	if(d.m2_frac_en)
		REG(pll_dw(ddi, 3)) |= PORT_PLL_M2_FRAC_ENABLE;
	else
		REG(pll_dw(ddi, 3)) &= ~PORT_PLL_M2_FRAC_ENABLE;

	// Proportional / integral coefficients and gain control.
	REG(pll_dw(ddi, 6)) = (REG(pll_dw(ddi, 6)) & ~(PORT_PLL_PROP_COEFF_MASK | PORT_PLL_INT_COEFF_MASK | PORT_PLL_GAIN_CTL_MASK))
	                 | PORT_PLL_PROP_COEFF(c.prop) | PORT_PLL_INT_COEFF(c.integ) | PORT_PLL_GAIN_CTL(c.gain);

	// TDC target count.
	REG(pll_dw(ddi, 8)) = (REG(pll_dw(ddi, 8)) & ~PORT_PLL_TARGET_CNT_MASK) | PORT_PLL_TARGET_CNT(c.tdc);

	// Lock threshold = 5.
	REG(pll_dw(ddi, 9)) = (REG(pll_dw(ddi, 9)) & ~PORT_PLL_LOCK_THRESHOLD_MASK) | PORT_PLL_LOCK_THRESHOLD(5);

	// DCO amplitude override.
	REG(pll_dw(ddi, 10)) = (REG(pll_dw(ddi, 10)) & ~PORT_PLL_DCO_AMP_MASK)
	                  | PORT_PLL_DCO_AMP(c.dcoamp) | PORT_PLL_DCO_AMP_OVR_EN;

	// Recalibrate and re-enable the 10-bit clock.
	REG(pll_ebb4(ddi)) |= PORT_PLL_RECALIBRATE;
	REG(pll_ebb4(ddi)) |= PORT_PLL_10BIT_CLK_ENABLE;

	// Enable the PLL and wait for lock.
	REG(pll_enable(ddi)) |= PORT_PLL_ENABLE_BIT;
	if(!wait_for_bit_set(REG_PTR(pll_enable(ddi)), PORT_PLL_LOCK, 200, 10)) {
		lil_log(ERROR, "glk::pll: DDI %c failed to lock\n", '0' + ddi);
		return false;
	}

	// Lane stagger (group instance).
	REG(pcs_dw12_grp(ddi)) = (REG(pcs_dw12_grp(ddi)) & ~0x1Fu)
	                       | (lane_stagger(pixel_khz) & 0x1F) | LANESTAGGER_STRAP_OVRD;

	return true;
}

void disable(LilGpu *lil_gpu, LilConnector *con) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	enum LilDdiId ddi = con->ddi_id;

	REG(pll_enable(ddi)) &= ~PORT_PLL_ENABLE_BIT;
	wait_for_bit_unset(REG_PTR(pll_enable(ddi)), PORT_PLL_LOCK, 200, 10);
}

} // namespace glk::pll
