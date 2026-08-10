#pragma once

#include <lil/intel.h>

namespace kbl::dp {

bool is_connected (struct LilGpu* gpu, struct LilConnector* connector);
LilConnectorInfo get_connector_info(struct LilGpu* gpu, struct LilConnector* connector);
bool pre_enable(LilGpu *gpu, LilConnector *con);
void commit_modeset(struct LilGpu* gpu, struct LilCrtc* crtc);
void crtc_shutdown(LilGpu *gpu, LilCrtc *crtc);

}
