#pragma once

#include <lil/intel.h>

#include <stdint.h>

namespace kbl::brightness {

uint16_t get(LilGpu *gpu, LilConnector *con);
void set(LilGpu *gpu, LilConnector *con, uint16_t level);

}
