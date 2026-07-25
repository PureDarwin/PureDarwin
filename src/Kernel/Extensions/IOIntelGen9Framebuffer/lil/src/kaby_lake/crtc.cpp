#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/regs.hpp"

static bool pll_available(LilGpu *gpu, uint32_t reg) {
	uint32_t hdport_state = REG(HDPORT_STATE);
	bool hdport_in_use = false;

	lil_log(VERBOSE, "pll_available: hdport_state: %u\n", hdport_state);
	lil_log(VERBOSE, "pll_available: reg=%x, REG(reg)=%u\n", reg, REG(reg));

	/* check if HDPORT pre-emption is enabled to begin with */
	if(!(hdport_state & HDPORT_STATE_ENABLED)) {
		/* if not, we can just look for the enable flag */
		lil_log(VERBOSE, "pll_available: hdport pre-emption disabled, returning enabled bit\n");
		return (REG(reg) & (1 << 31));
	}

	/* check if our DPLL has been preempted for HDPORT */
	switch(reg) {
		case LCPLL1_CTL: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL0_USED;
			break;
		}
		case LCPLL2_CTL: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL1_USED;
			break;
		}
		case WRPLL_CTL1: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL3_USED;
			break;
		}
		case WRPLL_CTL2: {
			hdport_in_use = hdport_state & HDPORT_STATE_DPLL2_USED;
			break;
		}
		default: {
			return (REG(reg) & (1 << 31));
		}
	}

	/* used for HDPORT */
	if(hdport_in_use)
		return false;

	return (REG(reg) & (1 << 31));
}

namespace kbl::crtc {

void pll_find(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(gpu->subgen == SUBGEN_GEMINI_LAKE && crtc->connector->type != EDP) {
		// Gemini Lake has a fixed mapping from pipe to PLL
		switch(crtc->transcoder) {
			case TRANSCODER_A: {
				crtc->pll_id = LCPLL1;
				return;
			}
			case TRANSCODER_B: {
				crtc->pll_id = LCPLL2;
				return;
			}
			case TRANSCODER_C: {
				crtc->pll_id = WRPLL1;
				return;
			}
			default: {
				lil_panic("TODO: add more cases here");
			}
		}
		lil_panic("should not be reached");
	}

	if(crtc->connector->type == EDP/* && !crtc->connector->encoder->edp.edp_downspread*/) {
		crtc->pll_id = LCPLL1;
		return;
	}

	if(!pll_available(gpu, LCPLL2_CTL)) {
		if(!pll_available(gpu, WRPLL_CTL2)) {
			if(!pll_available(gpu, WRPLL_CTL1)) {
				lil_panic("no PLL available");
			} else {
				crtc->pll_id = WRPLL1;
				return;
			}
		} else {
			crtc->pll_id = WRPLL2;
			return;
		}
	} else {
		crtc->pll_id = LCPLL2;
		return;
	}
}

} // namespace kbl::crtc
