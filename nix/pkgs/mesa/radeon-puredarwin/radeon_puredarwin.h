#pragma once
#include "winsys/radeon_winsys.h"
#include "radeon_surface.h"
#include <IOKit/IOKitLib.h>
#include "PDRadeonRenderShared.h"
struct pd_winsys {
    struct radeon_winsys base;
    io_connect_t conn;
    struct pd_radeon_info hw;
    struct radeon_info info;
    struct radeon_surface_manager *surf_man;
    uint64_t allocated, submissions;
    bool failed;
    struct pd_radeon_scanout scanout;
    struct pipe_resource *scanout_resource;
    struct pipe_resource *present_buffers[2];
    unsigned present_index;
    struct pipe_resource *readback_buffer;
    struct sw_winsys *sw;
};
struct pipe_screen *radeon_puredarwin_screen_create(struct sw_winsys *sw, const struct pipe_screen_config *config);
int pd_surface_query(int conn, unsigned request, uint32_t *value);
int pd_surface_init(struct radeon_winsys *, const struct radeon_info *, const struct pipe_resource *, uint64_t, unsigned, enum radeon_surf_mode, struct radeon_surf *);

bool pd_radeon_present_resource(struct pipe_context *, struct pipe_resource *, unsigned, unsigned, bool);
bool pd_radeon_readback_resource(struct pipe_context *, struct pipe_resource *, unsigned, unsigned, bool, void *, unsigned);
