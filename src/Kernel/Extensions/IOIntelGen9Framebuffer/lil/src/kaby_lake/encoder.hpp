#pragma once

#include <lil/intel.h>

#include "src/vbt/vbt-types.hpp"

namespace kbl::encoder {

void edp_init(LilGpu *gpu, LilEncoder *enc);
void dp_init(LilGpu *gpu, LilEncoder *enc, struct child_device *dev);
void hdmi_init(LilGpu *gpu, LilEncoder *enc, struct child_device *dev);

}
