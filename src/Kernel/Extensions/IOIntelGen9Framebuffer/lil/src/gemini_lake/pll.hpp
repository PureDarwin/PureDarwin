#pragma once

#include <lil/intel.h>

namespace glk::pll {

bool enable(LilGpu *gpu, LilCrtc *crtc);

bool enable_at(LilGpu *gpu, LilCrtc *crtc, uint32_t clock_khz);

void disable(LilGpu *gpu, LilConnector *con);

} // namespace glk::pll
