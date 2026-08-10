#include "src/lvds.hpp"
#include "src/gmbus.hpp"

#include <lil/imports.h>

#define LVDS_CTL 0xE1180

bool lil_lvds_is_connected (struct LilGpu* gpu, struct LilConnector* connector) {
    (void)connector;
    volatile uint32_t* lvds_ctl = (uint32_t*)(gpu->mmio_start + LVDS_CTL);
    return *lvds_ctl & (1 << 1);
}

LilConnectorInfo lil_lvds_get_connector_info (struct LilGpu* gpu, struct LilConnector* connector) {
    (void)connector;
    LilConnectorInfo ret = {};
    auto info = reinterpret_cast<LilModeInfo *>(lil_malloc(sizeof(LilModeInfo) * 4));
    ret.num_modes = lil_gmbus_get_mode_info(gpu, connector, info);
    ret.modes = info;
    return ret;
}

void lil_lvds_set_state (struct LilGpu* gpu, struct LilConnector* connector, uint32_t state) {
    (void)connector;
    volatile uint32_t* lvds_ctl = (uint32_t*)(gpu->mmio_start + LVDS_CTL);
    *lvds_ctl = state;
    (void)lvds_ctl;
}

uint32_t lil_lvds_get_state (struct LilGpu* gpu, struct LilConnector* connector) {
    (void)connector;
    volatile uint32_t* lvds_ctl = (uint32_t*)(gpu->mmio_start + LVDS_CTL);
    return *lvds_ctl;
}
