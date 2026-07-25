#pragma once

#include <lil/intel.h>

namespace kbl::edp {

bool pre_enable(LilGpu *gpu, LilConnector *con);
bool aux_readable(LilGpu *gpu, LilConnector *con);
void validate_clocks_for_bpp(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t link_rate, uint32_t *bpp);

}
