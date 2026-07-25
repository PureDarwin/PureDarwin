#pragma once

#include <lil/intel.h>

namespace kbl::link_training {

bool edp(LilGpu *gpu, LilCrtc *crtc, uint32_t max_link_rate, uint8_t lane_count);

}
