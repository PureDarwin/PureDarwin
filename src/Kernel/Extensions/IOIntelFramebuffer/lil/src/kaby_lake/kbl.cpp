#include "src/pd_kext_prims.hpp"
#include <lil/imports.h>
#include <lil/intel.h>


#include "src/base.hpp"
#include "src/debug.hpp"
#include "src/kaby_lake/cdclk.hpp"
#include "src/kaby_lake/dp.hpp"
#include "src/kaby_lake/edp.hpp"
#include "src/kaby_lake/hdmi.hpp"
#include "src/kaby_lake/kbl.hpp"
#include "src/kaby_lake/gtt.hpp"
#include "src/kaby_lake/setup.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/kaby_lake/pci.hpp"
#include "src/kaby_lake/pcode.hpp"
#include "src/regs.hpp"
#include "src/vbt/vbt.hpp"

namespace {

void wm_latency_setup(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	uint32_t data0 = 0;
	uint32_t data1 = 0;
	uint32_t timeout = 100;
	if(kbl::pcode::rw(gpu, &data0, &data1, kbl::pcode::Mailbox::GEN9_READ_MEM_LATENCY, &timeout)) {
		gpu->mem_latency_first_set = data0;

		data0 = 1;
		data1 = 0;
		timeout = 100;

		if(kbl::pcode::rw(gpu, &data0, &data1, kbl::pcode::Mailbox::GEN9_READ_MEM_LATENCY, &timeout)) {
			gpu->mem_latency_second_set = data0;
		}
	}
}

void kbl_crtc_init(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	enum LilPllId pll_id = INVALID_PLL;

	if(crtc->connector->type != EDP) {
		uint32_t pll_choice = (REG(DPLL_CTRL2) & DPLL_CTRL2_DDI_CLOCK_SELECT_MASK(crtc->connector->ddi_id)) >> DPLL_CTRL2_DDI_CLOCK_SELECT_SHIFT(crtc->connector->ddi_id);

		switch(pll_choice) {
			case 0: {
				if(REG(LCPLL1_CTL) & (1 << 31))
					pll_id = LCPLL1;
				break;
			}
			case 1: {
				if(REG(LCPLL2_CTL) & (1 << 31))
					pll_id = LCPLL2;
				break;
			}
			case 2: {
				if(REG(WRPLL_CTL1) & (1 << 31))
					pll_id = WRPLL1;
				break;
			}
			case 3: {
				if(REG(WRPLL_CTL2) & (1 << 31))
					pll_id = WRPLL2;
				break;
			}
			default:
				lil_panic("unsound PLL choice");
		}
	}

	crtc->pll_id = pll_id;

	auto f = vbt_get_bdb_block<bdb_fixed_mode_set>(gpu->vbt_header);
	if(f) {
		/* TODO: handle the VBT bdb block for fixed mode set */
		lil_assert(!f->feature_enable);
	}
}

} // namespace

namespace kbl {

void init_gpu(LilGpu *lil_gpu) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	gpu->vmem_clear = kbl::gtt::vmem_clear;
	gpu->vmem_map = kbl::gtt::vmem_map;

	gpu->max_connectors = 4;
	gpu->connectors = reinterpret_cast<LilConnector *>(lil_malloc(sizeof(LilConnector) * gpu->max_connectors));

	kbl::pci::detect(gpu);

	// TODO: we should probably use an array or something for this
	switch(gpu->gen) {
		case GEN_SKL: {
			lil_log(VERBOSE, "\tGPU gen: Skylake\n");
			break;
		}
		default: {
			lil_panic("unknown GPU generation in this codepath");
		}
	}

	switch(gpu->subgen) {
		case SUBGEN_GEMINI_LAKE: {
			lil_log(VERBOSE, "\tGPU subgen: Gemini Lake\n");
			break;
		}
		case SUBGEN_KABY_LAKE: {
			lil_log(VERBOSE, "\tGPU subgen: Kaby Lake\n");
			break;
		}
		case SUBGEN_COFFEE_LAKE: {
			lil_log(VERBOSE, "\tGPU subgen: Coffee Lake\n");
			break;
		}
		case SUBGEN_NONE:
		default: {
			break;
		}
	}

	switch(gpu->variant) {
		case H: {
			lil_log(VERBOSE, "\tGPU variant: H\n");
			break;
		}
		case ULT: {
			lil_log(VERBOSE, "\tGPU variant: ULT\n");
			break;
		}
		case ULX: {
			lil_log(VERBOSE, "\tGPU variant: ULX\n");
			break;
		}
	}

	kbl::setup::setup(gpu);
	kbl::setup::initialize_display(gpu);
	kbl::setup::hotplug_enable(gpu);
	kbl::setup::psr_disable(gpu);

	/* TODO: on cold boot, perform the display init sequence */
	lil::array transcoders = {TRANSCODER_A, TRANSCODER_B, TRANSCODER_C};

	// Disable every transcoder
	for(auto transcoder : transcoders) {
		kbl::transcoder::disable(gpu, transcoder);
		kbl::transcoder::ddi_disable(gpu, transcoder);
		kbl::transcoder::clock_disable_by_id(gpu, transcoder);
	}

	REG(DPLL_CTRL2) |= (1 << 16);

	if(gpu->subgen == SUBGEN_KABY_LAKE || gpu->subgen == SUBGEN_COFFEE_LAKE) {
		/* Display WA #1142:kbl,cfl,cml */
		REG(CHICKEN_MISC2) = (REG(CHICKEN_MISC2) & ~CHICKEN_MISC2_KBL_ARB_FILL_SPARE_13) | CHICKEN_MISC2_KBL_ARB_FILL_SPARE_14;
		REG(CHICKEN_PAR1_1) |= CHICKEN_PAR1_1_KBL_ARB_FILL_SPARE_22;

		/* Display WA #0828:skl,bxt,kbl,cfl */
		REG(CHICKEN_PAR1_1) |= CHICKEN_PAR1_1_SKL_EDP_PSR_FIX_RDWRAP;

		REG(GEN8_CHICKEN_DCPR_1) |= GEN8_CHICKEN_DCPR_1_MASK_WAKEMEM;

		REG(DISP_ARB_CTL) |= DISP_ARB_CTL_FBC_MEMORY_WAKE;
	}

	uint8_t dpll0_link_rate = (REG(DPLL_CTRL1) & DPLL_CTRL1_LINK_RATE_MASK(0)) >> 1;
	gpu->vco_8640 = dpll0_link_rate == 4 || dpll0_link_rate == 5;
	gpu->boot_cdclk_freq = kbl::cdclk::dec_to_int(REG(SWF_6) & CDCLK_CTL_DECIMAL_MASK);
    gpu->cdclk_freq = kbl::cdclk::dec_to_int(REG(CDCLK_CTL) & CDCLK_CTL_DECIMAL_MASK);

	if(!gpu->boot_cdclk_freq)
		gpu->boot_cdclk_freq = gpu->cdclk_freq;

	wm_latency_setup(gpu);

	vbt_setup_children(gpu);

	for(size_t i = 0; i < gpu->num_connectors; i++) {
		lil_log(INFO, "pre-enabling connector %lu\n", i);

		switch(gpu->connectors[i].type) {
			case EDP: {
				if(!kbl::edp::pre_enable(gpu, &gpu->connectors[i]))
					lil_panic("eDP pre-enable failed");
				else if(restrictMultipleOutputs) {
					gpu->num_connectors = 1;
					gpu->connectors = &gpu->connectors[i];
				}
				break;
			}
			case DISPLAYPORT: {
				if(!kbl::dp::pre_enable(gpu, &gpu->connectors[i]))
					lil_log(INFO, "DP pre-enable for connector %lu failed\n", i);
				else if(restrictMultipleOutputs) {
					gpu->num_connectors = 1;
					gpu->connectors = &gpu->connectors[i];
				}
				break;
			}
			case HDMI: {
				if(!kbl::hdmi::pre_enable(gpu, &gpu->connectors[i]))
					lil_log(INFO, "HDMI pre-enable for connector %lu failed\n", i);
				else if(restrictMultipleOutputs) {
					gpu->num_connectors = 1;
					gpu->connectors = &gpu->connectors[i];
				}
				break;
			}
			default: {
				lil_log(WARNING, "unhandled pre-enable for type %u\n", gpu->connectors[i].type);
				break;
			}
		}
	}

	for(size_t i = 0; i < gpu->num_connectors; i++) {
		kbl_crtc_init(gpu, gpu->connectors[i].crtc);
	}
}

} // namespace kbl
