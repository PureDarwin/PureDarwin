#pragma once

#include <lil/intel.h>

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t lil_gmbus_get_mode_info(LilGpu* gpu, LilConnector *con, LilModeInfo* out);

bool gmbus_read(LilGpu* gpu, LilConnector *con, uint8_t offset, uint8_t index, size_t len, uint8_t* buf);
bool gmbus_write(LilGpu *gpu, LilConnector *con, uint8_t offset, uint8_t index, size_t len, uint8_t *buf);

#ifdef __cplusplus
}
#endif
