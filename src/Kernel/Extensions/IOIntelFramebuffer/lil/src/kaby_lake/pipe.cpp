#include <lil/intel.h>

#include "src/kaby_lake/pipe.hpp"
#include "src/regs.hpp"

namespace kbl::pipe {

void src_size_set(LilGpu *gpu, LilCrtc *crtc) {
	REG(PIPE_SRCSZ(crtc->pipe_id)) = (crtc->current_mode.vactive - 1) | ((crtc->current_mode.hactive - 1) << 16);
}

void dithering_enable(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp) {
	uint32_t dithering_bpc = 0;

	switch(bpp) {
		case 18:
			dithering_bpc = PIPE_MISC_DITHERING_6_BPC;
			break;
		case 24:
			dithering_bpc = PIPE_MISC_DITHERING_8_BPC;
			break;
		case 30:
			dithering_bpc = PIPE_MISC_DITHERING_10_BPC;
			break;
		case 36:
			// TODO: this is only supported on Alder Lake P and later it seems?
			dithering_bpc = PIPE_MISC_DITHERING_12_BPC;
			break;
		default:
			lil_panic("invalid bpp");
	}

	REG(PIPE_MISC(crtc->pipe_id)) = dithering_bpc | (REG(PIPE_MISC(crtc->pipe_id)) & ~PIPE_MISC_DITHERING_BPC_MASK);
	REG(PIPE_MISC(crtc->pipe_id)) = (REG(PIPE_MISC(crtc->pipe_id)) & ~PIPE_MISC_DITHERING_TYPE_MASK) | PIPE_MISC_DITHERING_TYPE_SPATIAL;

	if(bpp != 24)
		REG(PIPE_MISC(crtc->pipe_id)) |= PIPE_MISC_DITHERING_ENABLE;
}

void scaler_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(PS_WIN_POS_1(crtc->pipe_id)) = 0;
	REG(PS_CTRL_1(crtc->pipe_id)) = 0;
	REG(PS_WIN_SZ_1(crtc->pipe_id)) = 0;
}

} // namespace kbl::pipe
