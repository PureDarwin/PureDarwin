#pragma once

#include <lil/intel.h>

namespace kbl::transcoder {

/**
 * Return the base address for the given transcoder.
 *
 * This corresponds to the `TRANS_HTOTAL` register for the transcoder.
 */
uint32_t base(LilTranscoder id);

void enable(LilGpu *gpu, LilCrtc *crtc);
void disable(LilGpu *gpu, LilTranscoder transcoder);
void ddi_disable(LilGpu *gpu, LilTranscoder transcoder);
void clock_disable(LilGpu *gpu, LilCrtc *crtc);
void clock_disable_by_id(LilGpu *gpu, LilTranscoder transcoder);
void configure_clock(LilGpu *gpu, LilCrtc *crtc);
void timings_configure(LilGpu *gpu, LilCrtc *crtc);
void bpp_set(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void set_dp_msa_misc(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void ddi_polarity_setup(LilGpu *gpu, LilCrtc *crtc);
void ddi_setup(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes);
void configure_m_n(LilGpu *gpu, LilCrtc *crtc, uint32_t pixel_clock, uint32_t link_rate, uint32_t max_lanes, uint32_t bits_per_pixel);

} // namespace kbL::transcoder
