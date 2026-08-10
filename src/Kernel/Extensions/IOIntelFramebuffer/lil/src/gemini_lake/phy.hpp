#pragma once

#include <lil/intel.h>

namespace glk::phy {

void init(LilGpu *gpu, enum LilDdiId ddi);

void hdmi_vswing(LilGpu *gpu, enum LilDdiId ddi);

void dp_vswing(LilGpu *gpu, enum LilDdiId ddi, uint8_t vswing, uint8_t preemph);

} // namespace glk::phy
