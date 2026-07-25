#pragma once

#include <lil/intel.h>

// Broxton / Gemini Lake (gen9 LP) display PHY and Port PLL registers.
//
// The DDI signal path on gen9 LP is completely different silicon from SKL/KBL:
// there is no DDI_BUF_TRANS translation table or LCPLL/WRPLL. Each DDI has its
// own Port PLL inside a DPIO PHY, and voltage swing is programmed per-lane into
// the PHY PORT_TX/PORT_PCS registers.
//
// Address map (confirmed against BXT PRM Vol 2b):
//   DDI A  -> PHY1, base 0x162000  (eDP family, PHY_CTL_FAMILY_EDP)
//   DDI B  -> PHY0, base 0x6C000, channel 0  (DDI family, PHY_CTL_FAMILY_DDI)
//   DDI C  -> PHY0, base 0x6C000, channel 1
// Per-register channel addresses are not a uniform stride, so the accessors below
// select the exact PRM address per DDI rather than computing an offset.

namespace glk::regs {

// Per-PHY common-lane block base (CL1CM / REF registers live here).
static inline uint32_t phy_base(enum LilDdiId ddi) {
	return (ddi == DDI_A) ? 0x162000u : 0x6C000u;
}

// PHY power request bit in P_CR_GT_DISP_PWRON. PHY0 (DDI B/C) = bit 0,
// PHY1 (DDI A/eDP) = bit 1.
static inline uint32_t phy_pwron_bit(enum LilDdiId ddi) {
	return (ddi == DDI_A) ? (1u << 1) : (1u << 0);
}

// PHY common-reset family register (COMMON_RESET_DIS in bit 31).
static inline uint32_t phy_ctl_family(enum LilDdiId ddi) {
	return (ddi == DDI_A) ? 0x64C80u : 0x64C90u;
}

// Common-lane DW registers (per PHY).
static inline uint32_t cl1cm_dw0(enum LilDdiId ddi)  { return phy_base(ddi) + 0x000; }
static inline uint32_t cl1cm_dw9(enum LilDdiId ddi)  { return phy_base(ddi) + 0x024; }
static inline uint32_t cl1cm_dw10(enum LilDdiId ddi) { return phy_base(ddi) + 0x028; }
static inline uint32_t cl1cm_dw28(enum LilDdiId ddi) { return phy_base(ddi) + 0x070; }

// Reference DW registers (per PHY).
static inline uint32_t ref_dw3(enum LilDdiId ddi) { return phy_base(ddi) + 0x18C; }
static inline uint32_t ref_dw6(enum LilDdiId ddi) { return phy_base(ddi) + 0x198; }
static inline uint32_t ref_dw8(enum LilDdiId ddi) { return phy_base(ddi) + 0x1A0; }

// Port PLL registers (per channel). Base holds PORT_PLL_0; index steps by 4.
static inline uint32_t pll_ch_base(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162100u;
		case DDI_B: return 0x6C100u;
		case DDI_C: return 0x6C380u;
		default:    return 0x6C100u;
	}
}
static inline uint32_t pll_dw(enum LilDdiId ddi, unsigned idx) { return pll_ch_base(ddi) + idx * 4; }

static inline uint32_t pll_ebb0(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162034u;
		case DDI_B: return 0x6C034u;
		case DDI_C: return 0x6C340u;
		default:    return 0x6C034u;
	}
}
static inline uint32_t pll_ebb4(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162038u;
		case DDI_B: return 0x6C038u;
		case DDI_C: return 0x6C344u;
		default:    return 0x6C038u;
	}
}

// PLL enable/lock register (outside the PHY, in the always-on space).
static inline uint32_t pll_enable(enum LilDdiId ddi) { return 0x46074u + (unsigned)ddi * 4; }

// Voltage-swing group registers (write all lanes of a DDI at once).
static inline uint32_t pcs_dw10_grp(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162C28u;
		case DDI_B: return 0x6CC28u;
		case DDI_C: return 0x6CE28u;
		default:    return 0x6CC28u;
	}
}
static inline uint32_t pcs_dw12_grp(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162C30u;
		case DDI_B: return 0x6CC30u;
		case DDI_C: return 0x6CE30u;
		default:    return 0x6CC30u;
	}
}
static inline uint32_t tx_dw2_grp(enum LilDdiId ddi) {
	switch(ddi) {
		case DDI_A: return 0x162D08u;
		case DDI_B: return 0x6CD08u;
		case DDI_C: return 0x6CF08u;
		default:    return 0x6CD08u;
	}
}
static inline uint32_t tx_dw3_grp(enum LilDdiId ddi) { return tx_dw2_grp(ddi) + 0x04; }
static inline uint32_t tx_dw4_grp(enum LilDdiId ddi) { return tx_dw2_grp(ddi) + 0x08; }

// Per-lane DW14 (latency optimization); lane N is +0x80 * N.
static inline uint32_t tx_dw14_ln(enum LilDdiId ddi, unsigned lane) {
	uint32_t base;
	switch(ddi) {
		case DDI_A: base = 0x162538u; break;
		case DDI_B: base = 0x6C538u; break;
		case DDI_C: base = 0x6C938u; break;
		default:    base = 0x6C538u; break;
	}
	return base + lane * 0x80;
}

// P_CR_GT_DISP_PWRON: display PHY IO power request.
static constexpr uint32_t P_CR_GT_DISP_PWRON = 0x138090;

// Common-lane / reference / family bit fields.
static constexpr uint32_t PHY_POWER_GOOD          = (1u << 16); // PORT_CL1CM_DW0
static constexpr uint32_t PHY_RESERVED_BIT7       = (1u << 7);  // must read 0 with powergood
static constexpr uint32_t COMMON_RESET_DIS        = (1u << 31); // PHY_CTL_FAMILY
static constexpr uint32_t GRC_DONE                = (1u << 22); // PORT_REF_DW3
static constexpr uint32_t FCOMPREFSEL_100OHM      = (1u << 0);  // PORT_REF_DW8 fcomprefsel
static constexpr uint32_t GRCDIS                  = (1u << 15); // PORT_REF_DW8 grcdis
static constexpr uint32_t GRC_RDY_OVRD            = (1u << 23); // PORT_REF_DW8 grc_rdy_ovrd

// PORT_CL1CM_DW28: sus_clk_config (bits 1:0) + power-down enables.
static constexpr uint32_t SUS_CLK_CONFIG          = 0x3;        // 11b
static constexpr uint32_t OCL1_POWER_DOWN_EN      = (1u << 23);
static constexpr uint32_t DW28_OLDO_DYNPWRDOWNEN  = (1u << 22);

// PORT_PLL_ENABLE bit fields.
static constexpr uint32_t PORT_PLL_ENABLE_BIT     = (1u << 31);
static constexpr uint32_t PORT_PLL_LOCK           = (1u << 30);
static constexpr uint32_t PORT_PLL_REF_SEL        = (1u << 27); // 0 = Non-SSC (100 MHz NSSC)

// PORT_PLL_EBB_0: divider selects.
static inline uint32_t PORT_PLL_P1(uint32_t p1) { return (p1 & 0x7) << 13; }
static inline uint32_t PORT_PLL_P2(uint32_t p2) { return (p2 & 0x1F) << 8; }
static constexpr uint32_t PORT_PLL_P1_MASK        = (0x7u << 13);
static constexpr uint32_t PORT_PLL_P2_MASK        = (0x1Fu << 8);

// PORT_PLL_EBB_4.
static constexpr uint32_t PORT_PLL_10BIT_CLK_ENABLE = (1u << 13); // o_dtdclkpen_h
static constexpr uint32_t PORT_PLL_RECALIBRATE      = (1u << 14); // o_dtafcrecal

// PORT_PLL_0..10 fields.
static constexpr uint32_t PORT_PLL_M2_INT_MASK    = 0xFF;        // PLL_0 [7:0]
static constexpr uint32_t PORT_PLL_M2_FRAC_MASK   = 0x3FFFFF;    // PLL_2 [21:0]
static constexpr uint32_t PORT_PLL_M2_FRAC_ENABLE = (1u << 16);  // PLL_3
static inline uint32_t PORT_PLL_PROP_COEFF(uint32_t v) { return (v & 0xF); }        // PLL_6 [3:0]
static inline uint32_t PORT_PLL_INT_COEFF(uint32_t v)  { return (v & 0x1F) << 8; }  // PLL_6 [12:8]
static inline uint32_t PORT_PLL_GAIN_CTL(uint32_t v)   { return (v & 0x7) << 16; }  // PLL_6 [18:16]
static constexpr uint32_t PORT_PLL_PROP_COEFF_MASK = 0xF;
static constexpr uint32_t PORT_PLL_INT_COEFF_MASK  = (0x1Fu << 8);
static constexpr uint32_t PORT_PLL_GAIN_CTL_MASK   = (0x7u << 16);
static inline uint32_t PORT_PLL_TARGET_CNT(uint32_t v) { return (v & 0x3FF); }      // PLL_8 [9:0]
static constexpr uint32_t PORT_PLL_TARGET_CNT_MASK = 0x3FF;
static inline uint32_t PORT_PLL_LOCK_THRESHOLD(uint32_t v) { return (v & 0x7) << 1; } // PLL_9 [3:1]
static constexpr uint32_t PORT_PLL_LOCK_THRESHOLD_MASK = (0x7u << 1);
static inline uint32_t PORT_PLL_DCO_AMP(uint32_t v) { return (v & 0xF) << 10; }     // PLL_10 [13:10]
static constexpr uint32_t PORT_PLL_DCO_AMP_MASK    = (0xFu << 10);
static constexpr uint32_t PORT_PLL_DCO_AMP_OVR_EN  = (1u << 27);                    // PLL_10 [27]

} // namespace glk::regs
