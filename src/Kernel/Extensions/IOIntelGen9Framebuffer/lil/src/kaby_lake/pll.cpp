#include <lil/imports.h>
#include <lil/intel.h>
#include <stdint.h>

#include "src/kaby_lake/hdmi.hpp"
#include "src/kaby_lake/pcode.hpp"
#include "src/kaby_lake/pll.hpp"
#include "src/regs.hpp"

namespace kbl::pll {

// TODO(CLEAN;BIT): this function needs to be cleaned up
// 					specifically, we should be using enums or defines for this bit setting/clearing
LilError disable(LilGpu *gpu, LilConnector *con) {
	uint8_t shift = 0;
	uint32_t mask = 0;

	switch(con->crtc->pll_id) {
		case LCPLL1: {
			shift = 0x01;
			mask = 0x0E;
			break;
		}
		case LCPLL2: {
			shift = 7;
			mask = 0x380;
			REG(LCPLL2_CTL) &= ~0x80000000;
			break;
		}
		case WRPLL1: {
			shift = 13;
			mask = 0xE000;
			lil_panic("WRPLL1 disable unimplemented");
		}
		case WRPLL2: {
			shift = 19;
			mask = 0x380000;
			lil_panic("WRPLL2 disable unimplemented");
		}
		default: {
			return LilError::LIL_INVALID_PLL;
		}
	}

	uint32_t link_rate = (REG(DPLL_CTRL1) & mask) >> shift;

	if(con->type == EDP && (link_rate == 4 || link_rate == 5)) {
		lil_panic("eDP PLL disable logic unimplemented");
	}

	return LilError::LIL_SUCCESS;
}

void dpll_clock_set(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t clock_select = 0;
	uint32_t clock_ungate = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 2;
					break;
				case WRPLL1:
					clock_select = 4;
					break;
				case WRPLL2:
					clock_select = 6;
					break;
				default:
					lil_panic("invalid PLL");
			}
			REG(DPLL_CTRL2) &= 0xFFFF7FF8;
			REG(DPLL_CTRL2) |= clock_select | 0x8001;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFF7FFF;
			break;
		}
		case DDI_B: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x10;
					break;
				case WRPLL1:
					clock_select = 0x20;
					break;
				case WRPLL2:
					clock_select = 0x30;
					break;
				default:
					lil_panic("invalid PLL");
			}
			REG(DPLL_CTRL2) &= 0xFFFEFFC7;
			REG(DPLL_CTRL2) |= clock_select | 0x10008;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFEFFFF;
			break;
		}
		case DDI_C: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x80;
					break;
				case WRPLL1:
					clock_select = 0x100;
					break;
				case WRPLL2:
					clock_select = 0x180;
					break;
				default:
					lil_panic("invalid PLL");
			}
			REG(DPLL_CTRL2) &= 0xFFFDFE3F;
			REG(DPLL_CTRL2) |= clock_select | 0x20040;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFDFFFF;
			break;
		}
		case DDI_D: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x400;
					break;
				case WRPLL1:
					clock_select = 0x800;
					break;
				case WRPLL2:
					clock_select = 0xC00;
					break;
				default:
					lil_panic("invalid PLL");
			}
			REG(DPLL_CTRL2) &= 0xFFFBF1FF;
			REG(DPLL_CTRL2) |= clock_select | 0x40200;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFFBFFFF;
			break;
		}
		case DDI_E: {
			switch(crtc->pll_id) {
				case LCPLL1:
					clock_select = 0;
					break;
				case LCPLL2:
					clock_select = 0x2000;
					break;
				case WRPLL1:
					clock_select = 0x4000;
					break;
				case WRPLL2:
					clock_select = 0x6000;
					break;
				default:
					lil_panic("invalid PLL");
			}
			REG(DPLL_CTRL2) &= 0xFFF78FFF;
			REG(DPLL_CTRL2) |= clock_select | 0x81000;
			clock_ungate = REG(DPLL_CTRL2) & 0xFFF7FFFF;
			break;
		}
	}

	REG(DPLL_CTRL2) = clock_ungate;
}

namespace {

bool dpll_is_locked(LilGpu *gpu, enum LilPllId pll_id) {
	uint32_t mask = 0;

	switch(pll_id) {
		case LCPLL1:
			mask = 1;
			break;
		case LCPLL2:
			mask = 0x100;
			break;
		case WRPLL1:
			mask = 0x10000;
			break;
		case WRPLL2:
			mask = 0x1000000;
			break;
		default:
			lil_panic("invalid PLL");
	}

	return (REG(DPLL_STATUS) & mask) != 0;
}

} // namespace

void dpll_ctrl_enable(LilGpu *gpu, LilCrtc *crtc, uint32_t link_rate) {
	uint32_t dpll_link_rate = 0;

	switch(link_rate) {
		case 162000u:
			dpll_link_rate = 2;
			break;
		case 216000u:
			dpll_link_rate = 4;
			break;
		case 270000u:
			dpll_link_rate = 1;
			break;
		case 324000u:
			dpll_link_rate = 3;
			break;
		case 432000u:
			dpll_link_rate = 5;
			break;
	}

	uint32_t set_dpll_ctrl = 0;
	uint32_t dpll_ctrl_val = 0;

	LilEncoder *enc = crtc->connector->encoder;

	switch(crtc->pll_id) {
		case LCPLL1: {
			lil_assert(crtc->connector->type == EDP || crtc->connector->type == DISPLAYPORT);
			lil_log(VERBOSE, "configuring LCPLL1\n");
			set_dpll_ctrl = (2 * dpll_link_rate) | 1;
			REG(LCPLL1_CTL) &= ~(1 << 31);
			dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFFFFF0;
			break;
		}
		case LCPLL2: {
			uint32_t dpll1_flags = 0x40;
			lil_log(VERBOSE, "configuring LCPLL2\n");
			if(crtc->connector->type == HDMI) {
				lil_log(VERBOSE, "\tfor HDMI\n");
				set_dpll_ctrl = dpll1_flags | 0x800;
				// mask out DPLL1 Override, SSC and HDMI mode
				dpll_ctrl_val = (REG(DPLL_CTRL1) & 0xFFFFF3BF);
				break;
			}

			if(crtc->connector->type == EDP && enc->edp.edp_downspread) {
				dpll1_flags = 0x440;
			}

			set_dpll_ctrl = (dpll_link_rate << 7) | dpll1_flags;
			dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFFF03F;
			break;
		}
		case WRPLL1: {
			uint32_t dpll1_flags = 0x1000;
			if(crtc->connector->type != HDMI && crtc->connector->type != EDP) {
				lil_panic("non-eDP/HDMI is unimplemented");
			} else if(enc->edp.edp_downspread)
				dpll1_flags = 0x11000;
			set_dpll_ctrl = (dpll_link_rate << 13) | dpll1_flags;
        	dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFC0FFF;
			break;
		}
		case WRPLL2: {
			uint32_t dpll1_flags = 0x40000;
			if(crtc->connector->type != EDP) {
				lil_panic("non-eDP is unimplemented");
			} else if(enc->edp.edp_downspread) {
				dpll1_flags = 0x440000;
			}

			if(crtc->connector->type != HDMI) {
				set_dpll_ctrl = (dpll_link_rate << 19) | dpll1_flags;
				dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFF03FFFF;
			} else {
				set_dpll_ctrl = 0x20000 | dpll1_flags;
				dpll_ctrl_val = REG(DPLL_CTRL1) & 0xFFFCEFFF;
			}
			break;
		}
		default:
			lil_panic("invalid PLL");
	}

	lil_log(VERBOSE, "setting DPLL_CTRL1 = %#010x\n", set_dpll_ctrl | dpll_ctrl_val);
	REG(DPLL_CTRL1) = set_dpll_ctrl | dpll_ctrl_val;

	if(crtc->connector->type == HDMI) {
		kbl::hdmi::pll_enable_sequence(gpu, crtc);
	}

	if(crtc->connector->type == EDP) {
		uint32_t data0 = 0;
		uint32_t data1 = 0;
		bool pcode_write = true;

		if(link_rate == 216000 || link_rate == 432000) {
			if(enc->edp.edp_downspread) {
				switch(crtc->pll_id) {
					case LCPLL1:
						data0 = 0;
						break;
					case LCPLL2:
						data0 = 1;
						break;
					case WRPLL1:
						data0 = 2;
						break;
					case WRPLL2:
						data0 = 3;
						break;
					default:
						lil_panic("invalid PLL");
				}

			} else {
				pcode_write = false;
			}
		} else {
			data0 = 5;
		}

		if(pcode_write) {
			uint32_t timeout = 150;
			kbl::pcode::rw(gpu, &data0, &data1, pcode::Mailbox(0x23), &timeout);
		}
	}

	uint32_t pll_ctl = 0;

	switch(crtc->pll_id) {
		case LCPLL1:
			pll_ctl = LCPLL1_CTL;
			break;
		case LCPLL2:
			pll_ctl = LCPLL2_CTL;
			break;
		case WRPLL1:
			pll_ctl = WRPLL_CTL1;
			break;
		case WRPLL2:
			pll_ctl = WRPLL_CTL2;
			break;
		default:
			lil_panic("invalid PLL");
	}

	REG(pll_ctl) |= (1 << 31);
	lil_usleep(3000);

	for(size_t pll_enable_timeout = 3000; pll_enable_timeout; pll_enable_timeout -= 100) {
		lil_usleep(100);

		if(dpll_is_locked(gpu, crtc->pll_id)) {
			break;
		}
	}
}

} // namespace kbl::pll
