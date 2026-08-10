
#include "src/pd_kext_prims.hpp"
#include <lil/imports.h>
#include <lil/intel.h>
#include <lil/vbt-types.h>

#include "src/base.hpp"
#include "src/kaby_lake/encoder.hpp"
#include "src/vbt/vbt.hpp"

namespace kbl::encoder {

void edp_init(LilGpu *lil_gpu, LilEncoder *enc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	auto lvds_options = vbt_get_bdb_block<bdb_lvds_options>(gpu->vbt_header);
	auto edp = vbt_get_bdb_block<bdb_edp>(gpu->vbt_header);
	if(!edp)
		lil_panic("BDB eDP not found");

	uint8_t panel_type = lvds_options->panel_type;
	auto lfp_blc = vbt_get_bdb_block<bdb_lfp_blc>(gpu->vbt_header);

	enc->edp.backlight_control_method_type = lfp_blc->backlight_control[panel_type].type & 0xF;
	enc->edp.backlight_inverter_type = lfp_blc->panel[panel_type].type & 3;
	enc->edp.backlight_inverter_polarity = lfp_blc->panel[panel_type].active_low_pwm;
	enc->edp.pwm_inv_freq = lfp_blc->panel[panel_type].pwm_freq_hz;
	enc->edp.initial_brightness = lfp_blc->level[panel_type];
	enc->edp.min_brightness = lfp_blc->panel[panel_type].min_brightness;

	enc->edp.ssc_bits = (lvds_options->ssc_bits >> panel_type) & 1;

	auto general_defs = vbt_get_bdb_block<bdb_general_definitions>(gpu->vbt_header);

	if(gpu->connectors[0].ddi_id) {
		enc->edp.edp_max_lanes = 4;
	} else {
		uint8_t max_lanes = 4;

		if (lil::ranges::any_of(*general_defs, [](auto &dev) {
			return dev.device_type && (dev.dvo_port == 12 || dev.dvo_port == 11);
		}))
			max_lanes = 2;

		enc->edp.edp_max_lanes = max_lanes;
	}

	enc->edp.edp_vswing_preemph = (edp->edp_vswing_preemph >> (4 * panel_type)) & 0xF;
	enc->edp.t3_optimization = edp->edp_t3_optimization & 1;
	enc->edp.t1_t3 = edp->power_seqs[panel_type].t3;
	enc->edp.t8 = edp->power_seqs[panel_type].t8;
	enc->edp.t9 = edp->power_seqs[panel_type].t9;
	enc->edp.t10 = edp->power_seqs[panel_type].t10;
	enc->edp.t11_12 = (edp->power_seqs[panel_type].t12 / 1000 + 1);
	enc->edp.edp_fast_link_training = (edp->fast_link_training >> panel_type) & 1;
	enc->edp.edp_fast_link_lanes = edp->fast_link_params[panel_type].lanes;
	enc->edp.edp_iboost = general_defs->child_dev[0].iboost;
	enc->edp.edp_port_reversal = general_defs->child_dev[0].lane_reversal;
	enc->edp.pwm_on_to_backlight_enable = edp->pwm_delays[panel_type].pwm_on_to_backlight_enable;
	enc->edp.edp_full_link_params_provided = ((edp->full_link_params_provided >> panel_type) & 1) != 0;

	uint32_t fast_link_rate = edp->fast_link_params[panel_type].rate;

	switch(fast_link_rate) {
		case 0:
			enc->edp.edp_fast_link_rate = 6;
			break;
		case 1:
			enc->edp.edp_fast_link_rate = 10;
			break;
		case 2:
			enc->edp.edp_fast_link_rate = 20;
			break;
		default:
			lil_panic("unhandled fast_link_rate");
	}

	uint32_t color_depth = (edp->color_depth >> (2 * panel_type)) & 3;

	switch(color_depth) {
		case 0:
			enc->edp.edp_color_depth = 18;
			break;
		case 1:
			enc->edp.edp_color_depth = 24;
			break;
		case 2:
			enc->edp.edp_color_depth = 30;
			break;
		case 3:
			enc->edp.edp_color_depth = 36;
			break;
	}

	switch(general_defs->child_dev[0].dp_iboost_level) {
		case 0:
			enc->edp.edp_balance_leg_val = 1;
			break;
		case 1:
			enc->edp.edp_balance_leg_val = 3;
			break;
		case 2:
			enc->edp.edp_balance_leg_val = 7;
			break;
		default:
			enc->edp.edp_balance_leg_val = 0;
			break;
	}

	auto general_features = vbt_get_bdb_block<bdb_general_features>(gpu->vbt_header);
	if(!general_features)
		lil_panic("BDB general features not found");

	enc->edp.edp_vbios_hotplug_support = general_features->vbios_hotplug_support;
	enc->edp.dynamic_cdclk_supported = general_features->display_clock_mode;
}

void dp_init(LilGpu *lil_gpu, LilEncoder *enc, struct child_device *dev) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	auto general_features = vbt_get_bdb_block<bdb_general_features>(gpu->vbt_header);
	if(!general_features)
		lil_panic("BDB general features not found");

	enc->dp.vbios_hotplug_support = general_features->vbios_hotplug_support;

	auto general_defs = vbt_get_bdb_block<bdb_general_definitions>(gpu->vbt_header);
	enc->dp.ddc_pin = dev->ddc_pin;
	enc->dp.aux_ch = dev->aux_channel;
	enc->dp.onboard_redriver_emph_vswing = dev->onboard_redriver & 0x3f;
	enc->dp.dp_max_link_rate = 0;
	enc->dp.dp_lane_count = 0;
}

void hdmi_init(LilGpu *lil_gpu, LilEncoder *enc, struct child_device *dev) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	auto general_defs = vbt_get_bdb_block<bdb_general_definitions>(gpu->vbt_header);

	uint8_t iboost_level = 0;
	switch(dev->hdmi_iboost_level) {
		case 0:
			iboost_level = 1;
			break;
		case 1:
			iboost_level = 3;
			break;
		case 2:
			iboost_level = 7;
			break;
		default:
			lil_panic("unhandled HDMI iboost level");
	}
	enc->hdmi.ddc_pin = dev->ddc_pin;
	enc->hdmi.aux_ch = dev->aux_channel;
	enc->hdmi.iboost_level = iboost_level;
	enc->hdmi.iboost = dev->iboost;
	enc->hdmi.hdmi_level_shift = dev->hdmi_level_shifter_value;
}

} // namespace kbl::encoder
