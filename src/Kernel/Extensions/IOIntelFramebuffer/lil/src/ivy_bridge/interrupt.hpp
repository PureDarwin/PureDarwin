#pragma once

#include <lil/intel.h>

void lil_ivb_enable_display_interrupt (LilGpu* gpu, uint32_t enable_mask);
void lil_ivb_process_display_interrupt (LilGpu* gpu);
