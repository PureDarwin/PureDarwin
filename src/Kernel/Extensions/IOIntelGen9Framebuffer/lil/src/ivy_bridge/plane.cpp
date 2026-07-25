#include "src/ivy_bridge/plane.hpp"

#define  PRI_LINOFF_BASE 0x70184
#define  PRI_STRIDE_BASE 0x70188
#define  PRI_SURF_BASE 0x7019C

bool lil_ivb_update_primary_surface(struct LilGpu* gpu, struct LilPlane* plane, GpuAddr surface_address, GpuAddr line_stride) {
    if(line_stride > (32 * 1024) || (line_stride & 0x3F) != 0) // Stride must be smaller than 32K and 64 byte aligned
        return false;

    volatile uint32_t* pri_surf = (uint32_t*)(gpu->mmio_start + PRI_SURF_BASE + 0x1000 * plane->pipe_id);
    volatile uint32_t* pri_stride = (uint32_t*)(gpu->mmio_start + PRI_STRIDE_BASE + 0x1000 * plane->pipe_id);
    volatile uint32_t* pri_linoff = (uint32_t*)(gpu->mmio_start + PRI_LINOFF_BASE + 0x1000 * plane->pipe_id);

    gpu->connectors[plane->pipe_id].crtc->planes[0].surface_address = surface_address;

    *pri_linoff = surface_address &  0xfff;
    //FIXME: this seems to behave weirdly depending on the machine, even with the same monitor resolutin
    //*pri_stride = line_stride;
    *pri_surf   = surface_address  & ~0xfff;

    return true;
}
