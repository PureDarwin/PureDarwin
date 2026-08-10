#pragma once

#include <lil/intel.h>

namespace kbl::plane {

bool update_primary_surface(LilGpu* gpu, LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride);
uint32_t *get_formats(LilGpu *gpu, size_t *num);

void page_flip(LilGpu *gpu, LilCrtc *crtc);
void enable(LilGpu *gpu, LilCrtc *crtc, bool vblank_wait);
void disable(LilGpu *gpu, LilCrtc *crtc);
void size_set(LilGpu *gpu, LilCrtc *crtc);

} // namespace kbl::plane
