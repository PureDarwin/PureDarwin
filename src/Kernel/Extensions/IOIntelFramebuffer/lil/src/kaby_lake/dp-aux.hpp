#pragma once

#include <lil/intel.h>

#include "src/edid.hpp"

namespace kbl::dp::aux {

uint8_t native_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr);
void native_readn(struct LilGpu* gpu, LilConnector *con, uint16_t addr, size_t n, void *buf);
void native_write(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t v);
void native_writen(struct LilGpu* gpu, LilConnector *con, uint16_t addr, size_t n, void *buf);

LilError i2c_read(struct LilGpu* gpu, LilConnector *con, uint16_t addr, uint8_t len, uint8_t* buf);
LilError i2c_write(struct LilGpu* gpu, LilConnector *con,  uint16_t addr, uint8_t len, uint8_t* buf);

void read_edid(struct LilGpu* gpu, LilConnector *con, DisplayData* buf);

bool dual_mode_read(LilGpu *gpu, LilConnector *con, uint8_t offset, void *buffer, size_t size);

}
