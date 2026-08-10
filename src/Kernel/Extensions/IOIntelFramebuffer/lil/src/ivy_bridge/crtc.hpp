#pragma once

#include <lil/intel.h>

void lil_ivb_shutdown (struct LilGpu* gpu, struct LilCrtc* crtc);
void lil_ivb_commit_modeset (struct LilGpu* gpu, struct LilCrtc* crtc);
