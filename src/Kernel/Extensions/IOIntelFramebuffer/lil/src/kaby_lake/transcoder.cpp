#include <lil/intel.h>

#include "src/base.hpp"
#include "src/debug.hpp"
#include "src/dpcd.hpp"
#include "src/helpers.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/regs.hpp"

namespace kbl::transcoder {

uint32_t base(LilTranscoder id) {
	switch(id) {
		case TRANSCODER_A: return TRANSCODER_A_BASE;
		case TRANSCODER_B: return TRANSCODER_B_BASE;
		case TRANSCODER_C: return TRANSCODER_C_BASE;
		case TRANSCODER_EDP: return TRANSCODER_EDP_BASE;
		default: lil_panic("invalid transcoder");
	}
}

void enable(LilGpu *gpu, LilCrtc *crtc) {
	REG(base(crtc->transcoder) + TRANS_CONF) |= TRANS_CONF_ENABLE;
}

void disable(LilGpu *gpu, LilTranscoder transcoder) {
	REG(base(transcoder) + TRANS_CONF) &= ~TRANS_CONF_ENABLE;
	wait_for_bit_unset(REG_PTR(base(transcoder) + TRANS_CONF), TRANS_CONF_STATE, 21000, 1000);
}

void ddi_disable(LilGpu *lil_gpu, LilTranscoder transcoder) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	REG(base(transcoder) + TRANS_DDI_FUNC_CTL) &= ~(TRANS_DDI_FUNC_CTL_ENABLE | TRANS_DDI_FUNC_CTL_SELECT_DDI_MASK);

	if(gpu->subgen == SUBGEN_GEMINI_LAKE) {
		// Quirk: delay for 100ms
		lil_sleep(100);
	}
}

void clock_disable(LilGpu *gpu, LilCrtc *crtc) {
	if(crtc->transcoder != TRANSCODER_EDP && crtc->connector->type != EDP) {
		REG(TRANS_CLK_SEL(crtc->transcoder)) &= ~TRANS_CLK_SEL_CLOCK_MASK;
	}
}

void clock_disable_by_id(LilGpu *gpu, LilTranscoder transcoder) {
	REG(TRANS_CLK_SEL(transcoder)) &= ~TRANS_CLK_SEL_CLOCK_MASK;
}

void configure_clock(LilGpu *gpu, LilCrtc *crtc) {
	lil_assert((REG(base(crtc->transcoder) + TRANS_CONF) & TRANS_CONF_ENABLE) == 0);

	uint32_t val = 0;

	// transcoder EDP always uses DDI A clock
	if(crtc->transcoder == TRANSCODER_EDP)
		return;

	switch(crtc->connector->ddi_id) {
		case DDI_A: val = TRANS_CLK_SEL_CLOCK_NONE; break;
		case DDI_B: val = TRANS_CLK_SEL_CLOCK_DDI_B; break;
		case DDI_C: val = TRANS_CLK_SEL_CLOCK_DDI_C; break;
		case DDI_D: val = TRANS_CLK_SEL_CLOCK_DDI_D; break;
		case DDI_E: val = TRANS_CLK_SEL_CLOCK_DDI_E; break;
	}

	REG(TRANS_CLK_SEL(crtc->transcoder)) = (REG(TRANS_CLK_SEL(crtc->transcoder)) & TRANS_CLK_SEL_CLOCK_MASK) | val;
}

void timings_configure(LilGpu *gpu, LilCrtc *crtc) {
	LilModeInfo *mode = &crtc->current_mode;

	REG(base(crtc->transcoder) + TRANS_HTOTAL) = ((mode->htotal - 1) << 16) | (mode->hactive - 1);
	REG(base(crtc->transcoder) + TRANS_HBLANK) = ((mode->htotal - 1) << 16) | (mode->hactive - 1);
	REG(base(crtc->transcoder) + TRANS_HSYNC) = ((mode->hsyncEnd - 1) << 16) | (mode->hsyncStart - 1);
	REG(base(crtc->transcoder) + TRANS_VTOTAL) = ((mode->vtotal - 1) << 16) | (mode->vactive - 1);
	REG(base(crtc->transcoder) + TRANS_VBLANK) = ((mode->vtotal - 1) << 16) | (mode->vactive - 1);
	REG(base(crtc->transcoder) + TRANS_VSYNC) = ((mode->vsyncEnd - 1) << 16) | (mode->vsyncStart - 1);

	REG(base(crtc->transcoder) + TRANS_CONF) &= ~TRANS_CONF_INTERLACED_MODE_MASK;

	switch(crtc->pipe_id) {
		case 0:
			lil_assert(restrictMultipleOutputs);
			REG(PLANE_BUF_CFG_1_A) = 0x037b0000;
			break;
		default:
			lil_panic("timings_configure for pipe_id != A is unimplemented");
	}

	/* TODO: handle interlaced modes here? */
}

void bpp_set(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	uint32_t val = 0;

	switch(bpp) {
		case 18:
			val = TRANS_DDI_FUNC_CTL_MODE_6_BPC;
			break;
		case 24:
			val = TRANS_DDI_FUNC_CTL_MODE_8_BPC;
			break;
		case 30:
			val = TRANS_DDI_FUNC_CTL_MODE_10_BPC;
			break;
		case 36:
			val = TRANS_DDI_FUNC_CTL_MODE_12_BPC;
			break;
		default:
			lil_panic("unsupported bpp");
	}

	REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & ~TRANS_DDI_FUNC_CTL_MODE_BPC_MASK) | val;
}

void set_dp_msa_misc(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	if(crtc->connector->type != EDP && crtc->connector->type != DISPLAYPORT)
		return;

	uint32_t val = 0;

	switch(bpp) {
		case 18:
			val = DP_MSA_MISC_6_BPC;
			break;
		case 24:
			val = DP_MSA_MISC_8_BPC;
			break;
		case 30:
			val = DP_MSA_MISC_10_BPC;
			break;
		case 36:
			val = DP_MSA_MISC_12_BPC;
			break;
		default:
			lil_panic("unsupported bpp");
	}

	REG(base(crtc->transcoder) + TRANS_MSA_MISC) = val | DP_MSA_MISC_SYNC_CLOCK;
}

void ddi_polarity_setup(LilGpu *gpu, LilCrtc *crtc) {
	if(crtc->current_mode.hsyncPolarity && crtc->current_mode.vsyncPolarity) {
		uint32_t val = REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL);

		if(crtc->current_mode.hsyncPolarity == 2)
			val |= TRANS_DDI_FUNC_CTL_HSYNC;
		else
			val &= ~TRANS_DDI_FUNC_CTL_HSYNC;

		REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = val;
		val = REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL);

		if(crtc->current_mode.vsyncPolarity == 2)
			val |= TRANS_DDI_FUNC_CTL_VSYNC;
		else
			val &= ~TRANS_DDI_FUNC_CTL_VSYNC;

		REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = val;
	}
}

void ddi_setup(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes) {
	uint32_t val = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A:
			val = TRANS_DDI_FUNC_CTL_SELECT_DDI_NONE;
			break;
		case DDI_B:
			val = TRANS_DDI_FUNC_CTL_SELECT_DDI_B;
			break;
		case DDI_C:
			val = TRANS_DDI_FUNC_CTL_SELECT_DDI_C;
			break;
		case DDI_D:
			val = TRANS_DDI_FUNC_CTL_SELECT_DDI_D;
			break;
		case DDI_E:
			val = TRANS_DDI_FUNC_CTL_SELECT_DDI_E;
			break;
	}

	REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & 0x8FFFFFFF) | val;

	if(crtc->transcoder == TRANSCODER_EDP) {
		switch(crtc->pipe_id) {
			case 0:
				val = TRANS_DDI_FUNC_CTL_EDP_INPUT_PIPE_A;
				break;
			case 1:
				val = TRANS_DDI_FUNC_CTL_EDP_INPUT_PIPE_B;
				break;
			case 2:
				val = TRANS_DDI_FUNC_CTL_EDP_INPUT_PIPE_C;
				break;
			default:
				lil_panic("invalid pipe for transcoder EDP");
		}

		REG(TRANSCODER_EDP_BASE + TRANS_DDI_FUNC_CTL) = val;
	}

	switch(crtc->connector->type) {
		case HDMI:
			REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & ~TRANS_DDI_FUNC_CTL_MODE_SELECT_MASK) | TRANS_DDI_FUNC_CTL_MODE_SELECT_HDMI;
			break;
		case DISPLAYPORT:
		case EDP:
			REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & ~TRANS_DDI_FUNC_CTL_MODE_SELECT_MASK) | TRANS_DDI_FUNC_CTL_MODE_SELECT_DP_SST;
			break;
		default:
			lil_panic("unimplemented connector type");
	}

	switch(lanes) {
		case 0:
			goto trans_ddi_enable;
		case 1:
			val = TRANS_DDI_FUNC_CTL_MODE_X1;
			break;
		case 2:
			val = TRANS_DDI_FUNC_CTL_MODE_X2;
			break;
		case 4:
			val = TRANS_DDI_FUNC_CTL_MODE_X4;
			break;
		default:
			lil_panic("invalid lane count");
	}

	REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) = (REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) & ~TRANS_DDI_FUNC_CTL_MODE_MASK) | val;
trans_ddi_enable:
	REG(base(crtc->transcoder) + TRANS_DDI_FUNC_CTL) |= TRANS_DDI_FUNC_CTL_ENABLE;
}

namespace {

void compute_m_n(uint32_t *ret_m, uint32_t *ret_n, uint32_t m, uint32_t n, uint32_t constant_n) {
	*ret_n = constant_n;

	// Darwin: uint64_t is unsigned long long, so use the 'll' overflow builtin
	// (upstream's umull matches uint64_t only on LP64 Linux).
	uint64_t mul;
	bool overflow = __builtin_umulll_overflow(m, *ret_n, &mul);
	lil_assert(!overflow);

	*ret_m = div_u64(mul, n);

	while (*ret_m > 0xFFFFFF || *ret_n > 0xFFFFFF) {
		*ret_m >>= 1;
		*ret_n >>= 1;
	}
}

} // namespace

void configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t lanes, uint32_t bits_per_pixel) {
	uint32_t data_m, data_n;
	uint32_t link_m, link_n;
	uint64_t strm_clk = 10 * pixel_clock;
	uint32_t ls_clk = 10 * link_rate;

	compute_m_n(&data_m, &data_n, (bits_per_pixel * strm_clk), ls_clk * lanes * 8, 0x8000000);
	compute_m_n(&link_m, &link_n, strm_clk, ls_clk, 0x80000);

	REG(base(crtc->transcoder) + TRANS_DATAM) = data_m | TRANS_DATAM_TU_SIZE(64);
	REG(base(crtc->transcoder) + TRANS_DATAN) = data_n;
	REG(base(crtc->transcoder) + TRANS_LINKM) = link_m;
	REG(base(crtc->transcoder) + TRANS_LINKN) = link_n;
}

} // namespace kbl::transcoder
