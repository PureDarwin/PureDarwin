#include <lil/imports.h>
#include <lil/intel.h>

#include "src/avi.hpp"
#include "src/base.hpp"
#include "src/gemini_lake/hdmi.hpp"
#include "src/gemini_lake/pll.hpp"
#include "src/gemini_lake/phy.hpp"
#include "src/kaby_lake/pipe.hpp"
#include "src/kaby_lake/plane.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/regs.hpp"

namespace glk::hdmi {

void commit_modeset(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	LilConnector *con = crtc->connector;

	// Primary plane stride + surface base, then flip in the framebuffer.
	uint32_t stride = ((crtc->current_mode.hactive * 4) + 63) >> 6;
	REG(PRI_STRIDE(crtc->pipe_id)) = stride;
	REG(DSP_ADDR(crtc->pipe_id)) = 0;
	kbl::plane::page_flip(gpu, crtc);

	// 32bpp BGRX, linear tiling. GOP may leave the surface X/Y-tiled (bits 12:10)
	// and a leftover color order; our scanout is a linear buffer, so clear both
	// fields explicitly or the plane fetches the linear framebuffer as tiled and
	// scans out garbage/black.
	REG(PLANE_CTL(crtc->pipe_id)) =
	    (REG(PLANE_CTL(crtc->pipe_id)) & ~((0x7u << 10) | (0xFu << 24) | PLANE_CTL_COLOR_ORDER_RGBX))
	    | PLANE_CTL_SOURCE_PIXEL_FORMAT_RGB_8_8_8_8 | PLANE_CTL_COLOR_ORDER_BGRX;

	// Enable the Port PLL and latch the DDI voltage swing.
	if(!glk::pll::enable(gpu, crtc))
		lil_panic("glk::hdmi: Port PLL enable failed");
	glk::phy::hdmi_vswing(gpu, con->ddi_id);

	// Route the DDI's Port PLL clock to the transcoder. Without this the
	// transcoder has no clock and the pipe never reaches running state
	// (PIPECONF bit30 stays 0 -> black). Program it after the Port PLL is locked.
	uint32_t clk_sel;
	switch(con->ddi_id) {
		case DDI_B: clk_sel = TRANS_CLK_SEL_CLOCK_DDI_B; break;
		case DDI_C: clk_sel = TRANS_CLK_SEL_CLOCK_DDI_C; break;
		default:    clk_sel = TRANS_CLK_SEL_CLOCK_NONE; break;
	}
	REG(TRANS_CLK_SEL(crtc->transcoder)) =
	    (REG(TRANS_CLK_SEL(crtc->transcoder)) & ~TRANS_CLK_SEL_CLOCK_MASK) | clk_sel;

	kbl::pipe::src_size_set(gpu, crtc);
	kbl::plane::size_set(gpu, crtc);
	kbl::transcoder::timings_configure(gpu, crtc);
	kbl::transcoder::bpp_set(gpu, crtc, crtc->current_mode.bpp);
	kbl::pipe::dithering_enable(gpu, crtc, crtc->current_mode.bpp);

	kbl::transcoder::ddi_setup(gpu, crtc, 0);
	kbl::transcoder::ddi_polarity_setup(gpu, crtc);
	kbl::transcoder::enable(gpu, crtc);

	// Enable the DDI buffer and the primary plane.
	REG(DDI_BUF_CTL(con->ddi_id)) |= DDI_BUF_CTL_ENABLE;
	kbl::plane::enable(gpu, crtc, true);

	// AVI infoframe.
	uint32_t dip_data[8] = {};
	::hdmi::avi::infoframe_populate(crtc, &dip_data);
	for(size_t i = 0; i < 8; i++)
		REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_AVI_DATA(i)) = dip_data[i];
	REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_CTL) |= VIDEO_DIP_CTL_ENABLE_AVI;
}

void shutdown(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	LilConnector *con = crtc->connector;

	REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_CTL) &= ~VIDEO_DIP_CTL_ENABLE_AVI;
	kbl::plane::disable(gpu, crtc);
	kbl::transcoder::disable(gpu, crtc->transcoder);
	kbl::transcoder::ddi_disable(gpu, crtc->transcoder);
	kbl::pipe::scaler_disable(gpu, crtc);
	kbl::transcoder::clock_disable(gpu, crtc);

	REG(DDI_BUF_CTL(con->ddi_id)) &= ~DDI_BUF_CTL_ENABLE;
	glk::pll::disable(gpu, con);
}

} // namespace glk::hdmi
