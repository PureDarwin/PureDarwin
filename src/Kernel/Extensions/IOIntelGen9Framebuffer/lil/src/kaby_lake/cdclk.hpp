#pragma once

#include <lil/intel.h>

namespace kbl::cdclk {

uint32_t dec_to_int(uint32_t cdclk_freq_decimal);
void set_freq(LilGpu *gpu, uint32_t cdclk_freq_int);
void set_for_pixel_clock(LilGpu *gpu, uint32_t *clock);

}
