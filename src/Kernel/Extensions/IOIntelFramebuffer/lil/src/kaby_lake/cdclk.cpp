#include <lil/intel.h>
#include <lil/imports.h>

#include "src/base.hpp"
#include "src/kaby_lake/cdclk.hpp"
#include "src/kaby_lake/pcode.hpp"
#include "src/regs.hpp"

namespace {

struct vco_lookup {
	unsigned int decimal;
	unsigned int integer;
};

static struct vco_lookup vco8100_lookup[] = {
	{ 0x2A1, 0x152 },
	{ 0x382, 0x1C2 },
	{ 0x436, 0x21C },
	{ 0x544, 0x2A3 },
};

static struct vco_lookup vco8640_lookup[] = {
	{ 0x267, 0x135 },
	{ 0x35E, 0x1B0 },
	{ 0x436, 0x21C },
	{ 0x4D0, 0x26A },
};

} // namespace

namespace kbl::cdclk {

uint32_t dec_to_int(uint32_t cdclk_freq_decimal) {
	for(size_t i = 0; i < 4; i++) {
		if(vco8100_lookup[i].decimal == cdclk_freq_decimal)
			return vco8100_lookup[i].integer;
		if(vco8640_lookup[i].decimal == cdclk_freq_decimal)
			return vco8640_lookup[i].integer;
	}

	return 0;
}

void set_freq(LilGpu *lil_gpu, uint32_t cdclk_freq_int) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	uint32_t cdfreq_decimal = 0;
	uint32_t cdfreq_select = 0;

	switch(cdclk_freq_int) {
		case 309: {
			cdfreq_decimal = 615;
			cdfreq_select = 0x8000000;
			break;
		}
		case 338: {
			cdfreq_decimal = 673;
			cdfreq_select = 0x8000000;
			break;
		}
		case 432: {
			cdfreq_decimal = 862;
			cdfreq_select = 0;
			break;
		}
		case 450: {
			cdfreq_decimal = 898;
			cdfreq_select = 0;
			break;
		}
		case 540: {
			cdfreq_decimal = 1078;
			cdfreq_select = 0x4000000;
			break;
		}
		case 618: {
			cdfreq_decimal = 1232;
			cdfreq_select = 0xC000000;
			break;
		}
		case 675: {
			cdfreq_decimal = 1348;
			cdfreq_select = 0xC000000;
			break;
		}
		default: {
			lil_log(ERROR, "cdclk_int=%u\n", cdclk_freq_int);
			lil_panic("unhandled cdclk");
		}
	}

	uint32_t big_timeout = 3000;
	uint32_t data0 = 0;
	uint32_t data1 = 0;

	while(1) {
		uint32_t pcode_max_timeout = (big_timeout < 150) ? big_timeout : 150;
		uint32_t pcode_real_timeout = pcode_max_timeout;

		data0 = 3;

		if(!kbl::pcode::rw(gpu, &data0, &data1, pcode::Mailbox::SKL_CDCLK_CONTROL, &pcode_real_timeout))
			lil_panic("cdclk pcode_rw failed");

		if(data0 & 1) {
			break;
		}

		lil_usleep(10);

		big_timeout = (big_timeout + pcode_real_timeout - pcode_max_timeout) - 10;

		if(!big_timeout)
			lil_panic("timeout in cdclk_set_freq");
	}

	REG(CDCLK_CTL) = cdfreq_decimal | cdfreq_select | (REG(CDCLK_CTL) & 0xF3FFF800);

	lil_usleep(10);

	switch(cdfreq_select) {
		case 0: {
			data0 = 1;
			break;
		}
		case 0x4000000: {
			data0 = 2;
			break;
		}
		case 0x8000000: {
			data0 = 0;
			break;
		}
		case 0xC000000: {
			data0 = 3;
			break;
		}
		default:
			lil_panic("invalid cdfreq_select");
	}

	data1 = 0;

	uint32_t timeout = 100;

	if(!kbl::pcode::rw(gpu, &data0, &data1, pcode::Mailbox::SKL_CDCLK_CONTROL, &timeout))
		lil_panic("timeout in cdclk set");

	gpu->cdclk_freq = cdclk_freq_int;
}

void set_for_pixel_clock(LilGpu *lil_gpu, uint32_t *pixel_clock) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	struct vco_lookup *table = (gpu->vco_8640) ? vco8640_lookup : vco8100_lookup;
	size_t offset = 0;
	uint32_t clock = *pixel_clock;

	while(clock >= 990 * table[offset].integer) {
		if(++offset >= 4) {
			lil_panic("VCO table lookup failed");
		}
	}

	uint32_t cdclk_val = table[offset].integer;

	if(cdclk_val != gpu->cdclk_freq) {
		set_freq(gpu, cdclk_val);
	}
}

} // namespace kbl::cdclk
