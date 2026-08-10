#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/dpcd.hpp"
#include "src/edid.hpp"
#include "src/gmbus.hpp"
#include "src/gemini_lake/dp.hpp"
#include "src/gemini_lake/phy.hpp"
#include "src/gemini_lake/glk_regs.hpp"
#include "src/gemini_lake/pll.hpp"
#include "src/kaby_lake/dp-aux.hpp"
#include "src/kaby_lake/link-training.hpp"
#include "src/kaby_lake/pipe.hpp"
#include "src/kaby_lake/plane.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/regs.hpp"

namespace glk::dp {

namespace {

// DPCD MAX_LINK_RATE encoding -> link bit rate in kHz.
uint32_t link_rate_khz(uint8_t dpcd_rate) {
	switch(dpcd_rate) {
		case 6:  return 1620000; // RBR
		case 10: return 2700000; // HBR
		case 20: return 5400000; // HBR2
		default: return 0;
	}
}

} // namespace

SinkKind pre_enable(LilGpu *lil_gpu, LilConnector *con) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	LilEncoder *enc = con->encoder;

	// Wake a would-be DP sink into D0 and read its capabilities over native AUX.
	kbl::dp::aux::native_write(gpu, con, SET_POWER, 1);

	uint8_t rev = kbl::dp::aux::native_read(gpu, con, DPCD_REV);
	enc->dp.dp_max_link_rate = kbl::dp::aux::native_read(gpu, con, MAX_LINK_RATE);
	uint8_t raw_lane_count = kbl::dp::aux::native_read(gpu, con, MAX_LANE_COUNT);
	enc->dp.dp_lane_count = raw_lane_count & MAX_LANE_COUNT_MASK;
	enc->dp.support_tps3_pattern = raw_lane_count & MAX_LANE_COUNT_TPS3_SUPPORTED;
	enc->dp.support_enhanced_frame_caps = raw_lane_count & MAX_LANE_COUNT_ENHANCED_FRAME_CAP;

	if(enc->dp.dp_max_link_rate != 0 && enc->dp.dp_lane_count != 0) {
		lil_log(INFO, "glk::dp: DP sink DPCD rev=0x%x max_link_rate=0x%x lanes=%u\n",
		        rev, enc->dp.dp_max_link_rate, enc->dp.dp_lane_count);
		return SinkKind::DisplayPort;
	}

	// No DPCD: could be a passive DP++ -> HDMI/DVI adapter. Its downstream sink
	// answers DDC. On DDI A the AUX channel is dead, but the DDC pins are wired to
	// GMBUS, so read EDID over GMBUS (the HDMI DDC path) and check the header.
	auto header_ok = [](const DisplayData &e) {
		const uint8_t *h = reinterpret_cast<const uint8_t *>(&e);
		return h[0] == 0x00 && h[1] == 0xFF && h[2] == 0xFF && h[3] == 0xFF &&
		       h[4] == 0xFF && h[5] == 0xFF && h[6] == 0xFF && h[7] == 0x00;
	};

	DisplayData aux_edid = {};
	kbl::dp::aux::read_edid(gpu, con, &aux_edid);

	DisplayData gm_edid = {};
	gmbus_read(gpu, con, 0x50, 0, 128, reinterpret_cast<uint8_t *>(&gm_edid));

	const uint8_t *ah = reinterpret_cast<const uint8_t *>(&aux_edid);
	const uint8_t *gh = reinterpret_cast<const uint8_t *>(&gm_edid);
	lil_log(INFO, "glk::dp: no DPCD; ddc_pin=%u AUX-EDID[0..3]=%02x %02x %02x %02x GMBUS-EDID[0..3]=%02x %02x %02x %02x\n",
	        con->encoder->dp.ddc_pin, ah[0], ah[1], ah[2], ah[3], gh[0], gh[1], gh[2], gh[3]);

	bool dual_mode = header_ok(aux_edid) || header_ok(gm_edid);
	lil_log(INFO, "glk::dp: dual-mode HDMI adapter %s\n", dual_mode ? "detected" : "not found");

	return dual_mode ? SinkKind::DualModeHDMI : SinkKind::None;
}

void commit_modeset(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	LilConnector *con = crtc->connector;
	LilEncoder *enc = con->encoder;

	uint32_t bpp = (crtc->current_mode.bpp == (uint32_t)-1) ? 24 : crtc->current_mode.bpp;

	uint32_t link_khz = link_rate_khz(enc->dp.dp_max_link_rate);
	if(!link_khz)
		lil_panic("glk::dp: invalid dp_max_link_rate");

	uint32_t lanes = enc->dp.dp_lane_count;
	if(!lanes)
		lil_panic("glk::dp: dp_lane_count == 0");

	// The Port PLL runs at the link symbol clock (link bit rate / 10).
	uint32_t link_symbol_khz = link_khz / 10;

	lil_log(INFO, "glk::dp: link=%u kHz lanes=%u bpp=%u pipe=%u\n",
	        link_khz, lanes, bpp, crtc->pipe_id);

	// Primary plane: stride, surface, 32bpp BGRX linear (see glk::hdmi).
	uint32_t stride = ((crtc->current_mode.hactive * 4) + 63) >> 6;
	REG(PRI_STRIDE(crtc->pipe_id)) = stride;
	REG(DSP_ADDR(crtc->pipe_id)) = 0;
	kbl::plane::page_flip(gpu, crtc);
	REG(PLANE_CTL(crtc->pipe_id)) =
	    (REG(PLANE_CTL(crtc->pipe_id)) & ~((0x7u << 10) | (0xFu << 24) | PLANE_CTL_COLOR_ORDER_RGBX))
	    | PLANE_CTL_SOURCE_PIXEL_FORMAT_RGB_8_8_8_8 | PLANE_CTL_COLOR_ORDER_BGRX;

	// If the DP transport is already enabled (GOP left it running), quiesce it
	// before retraining so DDI_BUF_CTL / DP_TP_CTL start from a known state.
	if(REG(DP_TP_CTL(con->ddi_id)) & DP_TP_CTL_ENABLE) {
		uint32_t v = (REG(DP_TP_CTL(con->ddi_id)) & ~DP_TP_CTL_TRAIN_MASK) | DP_TP_CTL_TRAIN_PATTERN_IDLE;
		REG(DP_TP_CTL(con->ddi_id)) = v;
		lil_usleep(17000);
		REG(DP_TP_CTL(con->ddi_id)) = v & ~DP_TP_CTL_ENABLE;
		REG(DDI_BUF_CTL(con->ddi_id)) &= ~DDI_BUF_CTL_ENABLE;
		lil_usleep(1000);
	}

	// Port PLL at the link symbol clock (no DPLL mux / DDI clock select on GLK).
	if(!glk::pll::enable_at(gpu, crtc, link_symbol_khz))
		lil_panic("glk::dp: Port PLL enable failed");

	// Initial voltage swing (level 0); link training adjusts from here.
	glk::phy::dp_vswing(gpu, con->ddi_id, 0, 0);

	// DDI buffer: program the DP port width, enable, and enable the DP transport
	// in training-pattern-1 for link training.
	REG(DDI_BUF_CTL(con->ddi_id)) =
	    (REG(DDI_BUF_CTL(con->ddi_id)) & ~DDI_BUF_CTL_DP_PORT_WIDTH_MASK) | DDI_BUF_CTL_DP_PORT_WIDTH(lanes);
	if(lanes == 4)
		REG(DDI_BUF_CTL(con->ddi_id)) |= DDI_BUF_CTL_DDI_A_4_LANES;

	REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & ~DP_TP_CTL_TRAIN_MASK) | DP_TP_CTL_TRAIN_PATTERN1;
	REG(DP_TP_CTL(con->ddi_id)) |= DP_TP_CTL_ENABLE;

	REG(DDI_BUF_CTL(con->ddi_id)) |= DDI_BUF_CTL_ENABLE;
	lil_usleep(600);

	// Link training over AUX (generic loop; uses the Broxton PHY vswing on GLK).
	if(!kbl::link_training::edp(gpu, crtc, enc->dp.dp_max_link_rate, enc->dp.dp_lane_count))
		lil_panic("glk::dp: link training failed");

	// Send normal pixel data.
	REG(DP_TP_CTL(con->ddi_id)) = (REG(DP_TP_CTL(con->ddi_id)) & ~DP_TP_CTL_TRAIN_MASK) | (3u << 8);

	// Pipe / transcoder / plane. No configure_clock on GLK (port PLL feeds the
	// DDI directly).
	kbl::pipe::src_size_set(gpu, crtc);
	kbl::plane::size_set(gpu, crtc);
	kbl::transcoder::timings_configure(gpu, crtc);
	kbl::transcoder::bpp_set(gpu, crtc, bpp);
	kbl::pipe::dithering_enable(gpu, crtc, bpp);
	kbl::transcoder::configure_m_n(gpu, crtc, crtc->current_mode.clock, link_symbol_khz, lanes, bpp);
	kbl::transcoder::ddi_polarity_setup(gpu, crtc);
	kbl::transcoder::set_dp_msa_misc(gpu, crtc, bpp);
	kbl::transcoder::ddi_setup(gpu, crtc, lanes);
	kbl::transcoder::enable(gpu, crtc);

	if(crtc->planes[0].enabled)
		kbl::plane::enable(gpu, crtc, true);
}

void shutdown(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);
	LilConnector *con = crtc->connector;

	kbl::plane::disable(gpu, crtc);
	kbl::transcoder::disable(gpu, crtc->transcoder);
	kbl::transcoder::ddi_disable(gpu, crtc->transcoder);

	REG(DP_TP_CTL(con->ddi_id)) &= ~DP_TP_CTL_ENABLE;
	REG(DDI_BUF_CTL(con->ddi_id)) &= ~DDI_BUF_CTL_ENABLE;
	glk::pll::disable(gpu, con);
}

} // namespace glk::dp
