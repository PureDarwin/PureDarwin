#ifndef PD_DUMP_H
#define PD_DUMP_H

#include <stdint.h>
#include "bridge.h"

void pd_dump_draw(const struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial);
void pd_dump_compute(const struct PDComputeRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial);
void pd_dump_tile(const struct PDTileRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial);

// airjitdcap: the job's inputs (after 0, before it runs) and what it wrote (after 1) to a replay file
void pd_cap_draw(const struct PDDrawRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after);
void pd_cap_compute(const struct PDComputeRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after);
void pd_cap_tile(const struct PDTileRequest *r, uint8_t *base, uint32_t bsize, uint32_t serial, int after);

#endif
