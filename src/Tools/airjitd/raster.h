#ifndef AIRJITD_RASTER_H
#define AIRJITD_RASTER_H

#include <stdint.h>

#include "bridge.h"

// runs a draw's vertex stage per index and its fragment stage per covered pixel,
// blended into the target
int pd_run_draw(struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial);

#endif
