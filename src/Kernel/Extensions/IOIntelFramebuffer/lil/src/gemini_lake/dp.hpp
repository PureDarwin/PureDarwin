#pragma once

#include <lil/intel.h>

namespace glk::dp {

enum class SinkKind {
	None,          // nothing responded on AUX or DDC
	DisplayPort,   // a real DP sink (DPCD readable) - use the DP path
	DualModeHDMI,  // passive DP++ -> HDMI/DVI adapter (EDID over DDC, no DPCD)
};

SinkKind pre_enable(LilGpu *gpu, LilConnector *con);

void commit_modeset(LilGpu *gpu, LilCrtc *crtc);

void shutdown(LilGpu *gpu, LilCrtc *crtc);

} // namespace glk::dp
