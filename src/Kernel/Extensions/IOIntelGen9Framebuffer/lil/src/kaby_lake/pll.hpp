#pragma once

#include <lil/intel.h>

namespace kbl::pll {

LilError disable(LilGpu *gpu, LilConnector *con);
void dpll_clock_set(LilGpu *gpu, LilCrtc *crtc);
void dpll_ctrl_enable(LilGpu *gpu, LilCrtc *crtc, uint32_t link_rate);

}
