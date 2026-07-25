#pragma once

#include <lil/intel.h>

namespace kbl::pcode {

enum class Mailbox : uint32_t {
	GEN9_READ_MEM_LATENCY = 0x6,
	SKL_CDCLK_CONTROL = 0x7,
#define SKL_CDCLK_PREPARE_FOR_CHANGE 0x3
#define SKL_CDCLK_READY_FOR_CHANGE 0x1
};

bool rw(LilGpu *gpu, uint32_t *data0, uint32_t *data1, Mailbox mbox, uint32_t *timeout);

}
