#include "src/kaby_lake/brightness.hpp"
#include "src/regs.hpp"

namespace kbl::brightness {

uint16_t get(LilGpu *gpu, LilConnector *con) {
	return REG(BLC_PWM_DATA) & 0xFFFF;
}

void set(LilGpu *gpu, LilConnector *con, uint16_t level) {
	uint32_t reg = REG(BLC_PWM_DATA);

	REG(BLC_PWM_DATA) = (reg & 0xFFFF0000) | level;
}

} // namespace kbl::brightness
