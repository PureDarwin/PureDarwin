#pragma once

#include <lil/intel.h>

namespace glk::hdmi {

// Gemini Lake HDMI mode set. Reuses the gen9-generic pipe/transcoder/plane
// programming from the kaby_lake path and swaps in the Broxton Port PLL, PHY
// voltage swing, and DDI buffer enable.
void commit_modeset(LilGpu *gpu, LilCrtc *crtc);
void shutdown(LilGpu *gpu, LilCrtc *crtc);

} // namespace glk::hdmi
