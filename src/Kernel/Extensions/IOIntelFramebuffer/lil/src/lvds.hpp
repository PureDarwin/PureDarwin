#pragma once

#include <lil/intel.h>

#ifdef __cplusplus
extern "C" {
#endif

bool lil_lvds_is_connected (struct LilGpu* gpu, struct LilConnector* connector);
LilConnectorInfo lil_lvds_get_connector_info (struct LilGpu* gpu, struct LilConnector* connector);
void lil_lvds_set_state (struct LilGpu* gpu, struct LilConnector* connector, uint32_t state);
uint32_t lil_lvds_get_state (struct LilGpu* gpu, struct LilConnector* connector);

#ifdef __cplusplus
}
#endif
