#pragma once

#include <lil/intel.h>

#include "src/gtt.hpp"

#define GTT_IVB_CACHE_MLC_LLC 0b110

void lil_ivb_vmem_clear(LilGpu* gpu);
void lil_ivb_vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr);
