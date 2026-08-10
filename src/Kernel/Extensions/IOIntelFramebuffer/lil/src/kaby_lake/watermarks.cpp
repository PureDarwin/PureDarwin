#include <lil/imports.h>

#include "src/kaby_lake/watermarks.hpp"
#include "src/helpers.hpp"

namespace {

uint32_t linetime_us(uint32_t pixel_clock, uint32_t htotal) {
	return div_u32_ceil(htotal * 1000, pixel_clock);
}

uint_fixed_16_16_t method1(uint32_t pixel_rate, uint32_t latency) {
	if(!latency)
		return FP_16_16_MAX;

	uint32_t tmp = latency * pixel_rate * 4;
	return div_fixed16(tmp, 1000 * 512);
}

uint_fixed_16_16_t method2(uint32_t latency, uint32_t pixel_rate, uint32_t htotal, uint_fixed_16_16_t plane_blocks_per_line) {
	if(!latency)
		return FP_16_16_MAX;

	uint32_t tmp = latency * pixel_rate;
	tmp = DIV_ROUND_UP(tmp, htotal * 1000);

	return mul_u32_fixed16(tmp, plane_blocks_per_line);
}

} // namespace

namespace kbl::watermarks {

lil::optional<lil::pair<size_t, size_t>> calculate(size_t pixel_clock_khz, size_t width, size_t htotal, uint8_t wm, size_t level) {
	auto m1 = method1(pixel_clock_khz, wm);

	size_t plane_bytes_per_line = width * 4;
	auto plane_blocks_per_line = u32_to_fixed16(DIV_ROUND_UP(plane_bytes_per_line, 512) + 1);

	auto m2 = method2(wm, pixel_clock_khz, width, plane_blocks_per_line);

	uint_fixed_16_16_t res;

	if((4 * htotal / 512 < 1) && (plane_bytes_per_line / 512 < 1)) {
		res = m2;
	} else if(wm >= linetime_us(pixel_clock_khz, htotal)) {
		res = min_fixed16(m1, m2);
	} else {
		res = m1;
	}

	uint32_t blocks = fixed16_to_u32_round_up(res) + 1;

	if(level > 0)
		blocks = max(blocks, fixed16_to_u32_round_up(plane_blocks_per_line));

	uint32_t lines = div_round_up_fixed16(res, plane_blocks_per_line);

	if(level >= 1 && level <= 7) {
		blocks++;
	} else {
		lines = 0;
	}

	if(wm) {
		return {{blocks, lines}};
	}

	return lil::nullopt;
}

} // namespace kbl::watermarks
