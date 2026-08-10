#pragma once

#include <lil/intel.h>

namespace kbl::hdmi {

bool pre_enable(LilGpu *gpu, LilConnector *con);
bool is_connected(LilGpu *gpu, LilConnector *con);
LilConnectorInfo get_connector_info(LilGpu *gpu, LilConnector *con);
void shutdown(LilGpu *gpu, LilCrtc *crtc);
void commit_modeset(LilGpu *gpu, LilCrtc *crtc);
void pll_enable_sequence(LilGpu *gpu, LilCrtc *crtc);

}
