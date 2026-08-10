#include <lil/imports.h>

#include "src/gtt.hpp"
#include "src/helpers.hpp"
#include "src/kaby_lake/gtt.hpp"

// TODO: Servers can support up to 46-bits, but I don't see a way to determine when it's the case.
#define GTT_HAW 39

namespace kbl::gtt {

void vmem_clear(LilGpu* gpu) {
    for (size_t i = 0; i < (gpu->gtt_size >> 12); i++) {
        GTT64_ENTRY(gpu, i << 12) = 0;
    }
}

void vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr) {
    if((gpu_addr >> 12) >= (gpu->gtt_size / 8))
        lil_panic("GPU address beyond range of global GTT");

    uint64_t mask = ((1UL << GTT_HAW) - 1) & ~0xFFF;

    if ((host & ~mask) != 0)
        lil_panic("Kaby Lake GPUs only support " STRINGIFY(GTT_HAW) "-bit host addresses");

    GTT64_ENTRY(gpu, gpu_addr) = (host & mask) | GTT_PAGE_PRESENT;
}

} // namespace kbl::gtt
