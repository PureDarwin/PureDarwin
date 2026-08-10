#include <lil/imports.h>
#include <lil/intel.h>

#include "src/kaby_lake/hpd.hpp"
#include "src/regs.hpp"

namespace {

void program_pulse_count(LilGpu* gpu, LilConnector* connector) {
	uint32_t pulse_count;
	switch(connector->type) {
		case DISPLAYPORT: {
			pulse_count = 0x007CE; // 2000 microseconds
			break;
		}
		case HDMI: {
			pulse_count = 0x1869E; // 100000 microseconds
			break;
		}
		default: {
			lil_panic("kbl_program_pulse_count: unhandled connector");
		}
	}
	switch(connector->ddi_id) {
		case DDI_A: {
			REG(SHPD_PULSE_CNT_A) = pulse_count;
			break;
		}
		case DDI_B: {
			REG(SHPD_PULSE_CNT_B) = pulse_count;
			break;
		}
		case DDI_C: {
			REG(SHPD_PULSE_CNT_C) = pulse_count;
			break;
		}
		case DDI_D: {
			REG(SHPD_PULSE_CNT_D) = pulse_count;
			break;
		}
		case DDI_E: {
			REG(SHPD_PULSE_CNT_E) = pulse_count;
			break;
		}
	}
}

} // namespace

namespace kbl::hpd {

void enable(LilGpu *gpu, LilCrtc *crtc) {
	switch(crtc->connector->ddi_id) {
		case DDI_A:
			// AFAIK, hotplug for DDIA is always enabled
			break;
		case DDI_B: {
			REG(SHOTPLUG_CTL) |= 0x10;
			break;
		}
		case DDI_C: {
			REG(SHOTPLUG_CTL) |= 0x1000;
			break;
		}
		case DDI_D: {
			REG(SHOTPLUG_CTL) |= 0x100000;
			break;
		}
		case DDI_E: {
			REG(SHOTPLUG_CTL2) |= 0x10;
			break;
		}
	}

	if(!crtc->connector->encoder->dp.vbios_hotplug_support) {
		return;
	}

	program_pulse_count(gpu, crtc->connector);

	uint32_t sdeier_val = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_B: {
			REG(SDEIMR) &= ~0x200000;
			sdeier_val = REG(SDEIER) | 0x200000;
			break;
		}
		default: {
			lil_panic("unhandled DDI in kbl_hpd_enable");
		}
	}

	REG(SDEIER) = sdeier_val;
}

} // namespace kbl::hpd
