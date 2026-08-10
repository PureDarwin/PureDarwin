#pragma once

#include <lil/intel.h>

namespace kbl::setup {

void setup(LilGpu *gpu);
void initialize_display(LilGpu* gpu);
void psr_disable(LilGpu *gpu);
void hotplug_enable(LilGpu *gpu);

}
