#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/dpcd.hpp"
#include "src/kaby_lake/ddi.hpp"
#include "src/kaby_lake/dp-aux.hpp"
#include "src/kaby_lake/edp.hpp"
#include "src/regs.hpp"

namespace kbl::edp {

bool aux_readable(LilGpu *gpu, LilConnector *con) {
	LilEncoder *enc = con->encoder;

	if(REG(PP_CONTROL) & 8) {
		lil_usleep(100);

		if(kbl::ddi::hotplug_detected(gpu, con->ddi_id)) {
			return kbl::dp::aux::native_read(gpu, con, DPCD_REV) != 0;
		}
	} else {
		REG(PP_CONTROL) |= 8;
		lil_sleep(10);

		if(enc->edp.t3_optimization) {
			size_t tries = enc->edp.t1_t3 / 100;

			while(tries --> 0) {
				if(kbl::ddi::hotplug_detected(gpu, con->ddi_id)) {
					REG(SHOTPLUG_CTL) = REG(SHOTPLUG_CTL);

					if(kbl::dp::aux::native_read(gpu, con, DPCD_REV) != 0) {
						return true;
					}
				}

				lil_sleep(10);
			}

			REG(PP_CONTROL) &= ~8;
			return false;
		} else {
			lil_usleep(100 * enc->edp.t1_t3);
		}
	}

	return true;
}

bool pre_enable(LilGpu *lil_gpu, LilConnector *con) {
	LilEncoder *enc = con->encoder;
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if((gpu->variant == ULT || gpu->variant == ULX) && con->ddi_id == DDI_D) {
		lil_panic("unsupported eDP configuration");
	}

	lil_log(VERBOSE, "pre-enabling eDP on DDI %c\n", con->ddi_id + 'A');

	if(!kbl::ddi::buf_enabled(gpu, con->crtc)) {
		if(con->ddi_id == DDI_A) {
			REG(SHOTPLUG_CTL) |= 0x10000000;
		} else if(con->ddi_id == DDI_D) {
			REG(SHOTPLUG_CTL) |= 0x100000;

			if(enc->edp.edp_vbios_hotplug_support) {
				REG(SDEIMR) &= ~0x800000;
				REG(SDEIER) |= 0x800000;
			}
		}

		aux_readable(gpu, con);

		if(!kbl::ddi::hotplug_detected(gpu, con->ddi_id)) {
			REG(PP_CONTROL) &= ~8;
			return false;
		}

		REG(SHOTPLUG_CTL) = REG(SHOTPLUG_CTL);
		uint32_t ref_div = 100 * gpu->ref_clock_freq.MHz();
		REG(PP_OFF_DELAYS) = (enc->edp.t10 << 16) | (REG(PP_OFF_DELAYS) & 0xE000E000);
		REG(PP_DIVISOR) = (enc->edp.t11_12 & 0xFF) | (((ref_div >> 1) - 1) << 8);
		REG(PP_CONTROL) |= PP_CONTROL_RESET;

		if(enc->edp.backlight_control_method_type == 2 && enc->edp.backlight_inverter_type == 2) {
			uint32_t backlight_level = (gpu->ref_clock_freq.Hz() / enc->edp.pwm_inv_freq) >> 4;

			if(backlight_level < 100) {
				backlight_level = 100;
			}

			uint32_t backlight_val = (backlight_level << 16) | (backlight_level * enc->edp.initial_brightness / 0xFF);
			REG(BLC_PWM_DATA) = backlight_val;
			REG(SBLC_PWM_CTL2) = backlight_val;
			uint32_t sblc_pwm_ctl1 = REG(SBLC_PWM_CTL1);

			if(enc->edp.backlight_inverter_polarity) {
				sblc_pwm_ctl1 |= 0x20000000;
			} else {
				sblc_pwm_ctl1 &= ~0x20000000;
			}

			REG(SBLC_PWM_CTL1) = sblc_pwm_ctl1;
		}

		uint32_t ddi_buf_trans = 0;

		if(con->ddi_id == DDI_A) {
			ddi_buf_trans = DDI_BUF_TRANS(DDI_A);
		} else if(con->ddi_id == DDI_D) {
			ddi_buf_trans = DDI_BUF_TRANS(DDI_D);
		}

		if(ddi_buf_trans)
			kbl::ddi::buffer_setup_translations(gpu, con, ddi_buf_trans);
		else
			lil_panic("no valid DDI_BUF_TRANS");

		REG(SOUTH_CHICKEN1) &= 1;
	}

	enc->edp.edp_dpcd_rev = kbl::dp::aux::native_read(gpu, con, EDP_DPCD_REV);
	enc->edp.edp_dpcd_rev = kbl::dp::aux::native_read(gpu, con, EDP_DPCD_REV);

	if(enc->edp.ssc_bits) {
		enc->edp.edp_downspread = kbl::dp::aux::native_read(gpu, con, MAX_DOWNSPREAD) & 1;
	}

	enc->edp.edp_fast_link_training_supported = (kbl::dp::aux::native_read(gpu, con, MAX_DOWNSPREAD) & 0x40) && enc->edp.edp_fast_link_training;

	if(enc->edp.edp_fast_link_training_supported) {
		enc->edp.edp_max_link_rate = enc->edp.edp_fast_link_rate;
	} else if(enc->edp.edp_dpcd_rev >= 3) {
		enc->edp.edp_max_link_rate = 0;

		for(size_t i = 0; i < 8; ++i) {
			uint16_t val = 0;
			kbl::dp::aux::native_readn(gpu, con, 2 * (i + 8), 2, &val);

			enc->edp.supported_link_rates[i] = val;

			if(val == 0) {
				break;
			}

			enc->edp.supported_link_rates_len++;
		}
	} else {
		enc->edp.edp_max_link_rate = kbl::dp::aux::native_read(gpu, con, MAX_LINK_RATE);
	}

	enc->edp.edp_lane_count = kbl::dp::aux::native_read(gpu, con, MAX_LANE_COUNT);

	if((enc->edp.edp_dpcd_rev < 3 && !enc->edp.edp_lane_count) || !enc->edp.edp_lane_count) {
		REG(PP_CONTROL) &= ~8;
	}

	if(enc->edp.edp_dpcd_rev >= 3 && !enc->edp.supported_link_rates_len) {
		lil_panic("unsupported");
	}

	if(enc->edp.edp_max_link_rate == 20 && gpu->gen == GEN_SKL) {
		lil_panic("TODO");

		/* if PCI rev < 2, then set gpu->edp_max_link_rate = 10 */
	}

	lil_log(VERBOSE, "DPCD Info:\n");
	lil_log(VERBOSE, "\tmax_link_rate: %i\n", enc->edp.edp_max_link_rate);
	lil_log(VERBOSE, "\tlane_count: %i\n", enc->edp.edp_lane_count & 0x1F);
	lil_log(VERBOSE, "\tsupport_post_lt_adjust: %s\n", enc->edp.edp_lane_count & (1 << 5) ? "yes" : "no");
	lil_log(VERBOSE, "\tsupport_tps3_pattern: %s\n", enc->edp.edp_lane_count & (1 << 6) ? "yes" : "no");
	lil_log(VERBOSE, "\tsupport_enhanced_frame_caps: %s\n", enc->edp.edp_lane_count & (1 << 7) ? "yes" : "no");

	return true;
}

void validate_clocks_for_bpp(LilGpu *gpu, LilCrtc *crtc, uint32_t lanes, uint32_t link_rate, uint32_t *bpp) {
	uint32_t orig = *bpp;
	switch(*bpp) {
		case 36:
			*bpp = 36;
			if(crtc->current_mode.clock <= 8 * link_rate * lanes / 36)
				return;
			[[fallthrough]];
		case 30:
			*bpp = 30;
			if(crtc->current_mode.clock <= 8 * link_rate * lanes / 30)
				return;
			[[fallthrough]];
		case 24:
			*bpp = 24;
			if(crtc->current_mode.clock <= 8 * link_rate * lanes / 24)
				return;
			[[fallthrough]];
		case 18:
			*bpp = 18;
			if(8 * link_rate * lanes / 18 >= crtc->current_mode.clock)
				return;
			lil_log(VERBOSE, "original bpp=%u clock=%u linkrate=%u lanes=%u\n", orig, crtc->current_mode.clock, link_rate, lanes);
			lil_panic("no valid clock for bpp");
	}
}

} // namespace kbl::edp
