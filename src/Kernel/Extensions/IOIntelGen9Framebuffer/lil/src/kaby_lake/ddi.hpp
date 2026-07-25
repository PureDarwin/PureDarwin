#pragma once

#include <lil/intel.h>

namespace kbl::ddi {

bool buf_enabled(LilGpu *gpu, LilCrtc *crtc);
bool hotplug_detected(LilGpu *gpu, enum LilDdiId ddi_id);
void buffer_setup_translations(LilGpu *gpu, LilConnector *con, uint32_t reg);
void power_enable(LilGpu *gpu, LilCrtc *crtc);
void power_disable(LilGpu *gpu, LilConnector *con);
void balance_leg_set(LilGpu *gpu, enum LilDdiId ddi_id, uint8_t balance_leg);
void clock_disable(LilGpu *gpu, LilCrtc *crtc);

}
