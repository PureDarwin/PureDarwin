#ifndef AIRJITD_COMPUTE_H
#define AIRJITD_COMPUTE_H

#include <stdint.h>

#include "bridge.h"

#define PD_TG_MEM_MAX 0x10000
#define PD_TG_MEM_SLOTS 4
#define PD_MAX_THREADS (1ull << 24)

// runs a kernel over its grid, one thread after another
int pd_run_compute(struct PDComputeRequest *r, uint8_t *base, uint32_t bsize);

#endif
