// raster.h - runs a draw's vertex and fragment stages, blended into the target
// Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause

#ifndef AIRJITD_RASTER_H
#define AIRJITD_RASTER_H

#include <stdint.h>

#include "bridge.h"

// runs a draw's vertex stage per index and its fragment stage per covered pixel,
// blended into the target
int pd_run_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial);

#endif
