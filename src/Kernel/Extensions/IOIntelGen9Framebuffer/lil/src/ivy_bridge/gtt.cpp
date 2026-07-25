#include <lil/imports.h>

#include "src/gtt.hpp"
#include "src/ivy_bridge/gtt.hpp"

void lil_ivb_vmem_clear(LilGpu* gpu) {
    for (size_t i = 0; i < (gpu->gtt_size >> 12); i++) {
        GTT32_ENTRY(gpu, i << 12) = 0;
    }
}

void lil_ivb_vmem_map(LilGpu* gpu, uint64_t host, GpuAddr gpu_addr) {
    if ((host & ~0xFFFFFFFFFF) != 0)
        lil_panic("Ivy Bridge GPU only supports 40-bit host addresses");

    GTT32_ENTRY(gpu, gpu_addr) = host | (((host >> 32) & 0xFF) << 4) | GTT_IVB_CACHE_MLC_LLC | GTT_PAGE_PRESENT;
}
