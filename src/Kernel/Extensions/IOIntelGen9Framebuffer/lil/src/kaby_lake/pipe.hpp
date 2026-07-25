#pragma once

#include <lil/intel.h>

namespace kbl::pipe {

void src_size_set(LilGpu *gpu, LilCrtc *crtc);
void dithering_enable(LilGpu *gpu, LilCrtc *crtc, uint8_t bpp);
void scaler_disable(LilGpu *gpu, LilCrtc *crtc);

} // namespace kbl::pipe

