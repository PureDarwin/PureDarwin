#pragma once

#include <stdint.h>

#define PD_RADEON_RENDER_CLIENT 0x52414445u
#define PD_RADEON_HEAP_START 0x4000000u
#define PD_RADEON_MAX_SUBMIT_DW 65536u

enum {
    PD_RADEON_INFO,
    PD_RADEON_ALLOC,
    PD_RADEON_FREE,
    PD_RADEON_SUBMIT,
    PD_RADEON_SCANOUT,
    PD_RADEON_FLIP,
    PD_RADEON_COPY_OUT,
};

struct pd_radeon_info {
    uint32_t version;
    uint32_t pci_id;
    uint64_t heap_size;
    uint32_t tiling_config;
    uint32_t backend_map;
    uint32_t active_simds;
    uint32_t shader_config;
    uint32_t ram_config;
    uint32_t max_clock_mhz;
    uint32_t clock_crystal_khz;
};

struct pd_radeon_scanout {
    uint64_t handle, offset, size;
    uint32_t width, height, stride, reserved;
};
