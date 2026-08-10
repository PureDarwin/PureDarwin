#pragma once

#include <lil/intel.h>

namespace kbl::gtt {

void vmem_clear(LilGpu* gpu);
void vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr);

}
