#ifndef AIRJITD_TILE_H
#define AIRJITD_TILE_H

#include <stdint.h>

#include "bridge.h"

// runs a tile kernel over every tile of the pass, quad by quad, reading and writing the pass's
// colour attachments through its implicit imageblock
int pd_run_tile(struct PDTileRequest *r, uint8_t *base, uint32_t bsize);

#endif
