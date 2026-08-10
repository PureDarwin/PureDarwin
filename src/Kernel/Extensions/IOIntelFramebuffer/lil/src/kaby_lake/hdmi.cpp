#include <lil/imports.h>
#include <lil/intel.h>

#include "src/avi.hpp"
#include "src/base.hpp"
#include "src/edid.hpp"
#include "src/gmbus.hpp"
#include "src/helpers.hpp"
#include "src/kaby_lake/cdclk.hpp"
#include "src/kaby_lake/crtc.hpp"
#include "src/kaby_lake/ddi.hpp"
#include "src/kaby_lake/dp-aux.hpp"
#include "src/kaby_lake/hdmi.hpp"
#include "src/kaby_lake/pipe.hpp"
#include "src/kaby_lake/plane.hpp"
#include "src/kaby_lake/pll.hpp"
#include "src/kaby_lake/transcoder.hpp"
#include "src/regs.hpp"

namespace kbl::hdmi {

namespace {

bool ddi_in_use_by_hdport(LilGpu *gpu, enum LilDdiId ddi_id) {
	uint32_t val = REG(HDPORT_STATE);

	if((val & HDPORT_STATE_ENABLED) == 0) {
		return false;
	}

	uint32_t mask = 0;

	switch(ddi_id) {
		case DDI_A:
		case DDI_E:
			return false;
		case DDI_B:
			mask = (1 << 3);
			break;
		case DDI_C:
			mask = (1 << 5);
			break;
		case DDI_D:
			mask = (1 << 7);
			break;
	}

	return (val & mask);
}

bool is_lspcon_adaptor(LilGpu *gpu, LilCrtc *crtc) {
	if(!crtc->connector->encoder->dp.aux_ch)
		return false;

	LilConnector *con = crtc->connector;

	uint8_t val = 0;
	if(!kbl::dp::aux::dual_mode_read(gpu, con, 0x10, &val, 1))
		return false;

	lil_log(VERBOSE, "HDMI ID read = %#x\n", val);

	return (val & 0xF8) == 0xA8;
}

uint8_t HDMI_DDI_TRANS_TABLE[] = {
	0x18, 0x00, 0x00, 0x00, 0xAC, 0x00, 0x00, 0x00, 0x12, 0x50,
	0x00, 0x00, 0x9D, 0x00, 0x00, 0x00, 0x11, 0x70, 0x00, 0x00,
	0x88, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0xA1, 0x00,
	0x00, 0x00, 0x18, 0x00, 0x00, 0x00, 0x98, 0x00, 0x00, 0x00,
	0x13, 0x40, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00, 0x12, 0x60,
	0x00, 0x80, 0xCD, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00,
	0xDF, 0x00, 0x00, 0x00, 0x15, 0x30, 0x00, 0x80, 0xCD, 0x00,
	0x00, 0x00, 0x15, 0x30, 0x00, 0x80, 0xC0, 0x00, 0x00, 0x00,
	0x18, 0x00, 0x00, 0x80, 0xC0, 0x00, 0x00, 0x00
};

} // namespace

bool pre_enable(LilGpu *lil_gpu, LilConnector *con) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(con->ddi_id == DDI_E || ddi_in_use_by_hdport(gpu, con->ddi_id))
		return false;

	if((gpu->variant == ULT || gpu->variant == ULX) && con->ddi_id == DDI_D)
		return false;

	if(!kbl::ddi::buf_enabled(gpu, con->crtc)) {
		lil_log(INFO, "DDI %c not enabled, enabling\n", '0' + con->ddi_id);

		// Gemini Lake and Broxton do not have SFUSE_STRAP.
		if(gpu->subgen != SUBGEN_GEMINI_LAKE) {
			uint32_t sfuse_strap_mask = 0;

			switch(con->ddi_id) {
				case DDI_B: {
					sfuse_strap_mask = 4;
					break;
				}
				case DDI_C: {
					sfuse_strap_mask = 2;
					break;
				}
				case DDI_D: {
					sfuse_strap_mask = 1;
					break;
				}
				default:
					break;
			}

			if((REG(SFUSE_STRAP) & sfuse_strap_mask) == 0)
				return false;
		}

		kbl::ddi::buffer_setup_translations(gpu, con, DDI_BUF_CTL(con->crtc->pipe_id));

		/* TODO: HPD */
	}

	LilModeInfo out = {};
	uint8_t signature[8] = {};
	uint8_t null = 0;
	if(!gmbus_read(gpu, con, 0x50, 0, 8, signature))
		return false;
	bool init = !is_lspcon_adaptor(gpu, con->crtc); // TODO: do a signature check
	return init;
}

bool is_connected(LilGpu *gpu, LilConnector *con) {
	DisplayData edid = {};
	bool read = gmbus_read(gpu, con, 0x50, 0, 128, reinterpret_cast<uint8_t *>(&edid));

	if(!read) return false;

	uint8_t expected[8] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};

	return !memcmp(edid.magic, expected, 0);
}

LilConnectorInfo get_connector_info(LilGpu *gpu, LilConnector *con) {
    LilConnectorInfo ret = {};
    LilModeInfo* info = (LilModeInfo *) lil_malloc(sizeof(LilModeInfo) * 4);

    DisplayData edid = {};
    bool read = gmbus_read(gpu, con, 0x50, 0, 128, reinterpret_cast<uint8_t *>(&edid));

    int j = 0;
    for(int i = 0; i < 4; i++) { // Maybe 4 Detailed Timings
        if(edid.detailTimings[i].pixelClock == 0)
            continue; // Not a timing descriptor

		// TODO: with DVI this the max pixel clock is 165'000 afaict
		if(con->type == HDMI && edid.detailTimings[i].pixelClock * 10 > 300000)
			continue;

        edid_timing_to_mode(&edid, edid.detailTimings[i], &info[j++]);
    }

    ret.modes = info;
    ret.num_modes = j;
    return ret;
}

void shutdown(LilGpu *gpu, LilCrtc *crtc) {
	LilConnector *con = crtc->connector;

	lil_assert(con->type == HDMI);

	REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_CTL) &= ~VIDEO_DIP_CTL_ENABLE_AVI;
	kbl::plane::disable(gpu, crtc);
	kbl::transcoder::disable(gpu, crtc->transcoder);
	kbl::transcoder::ddi_disable(gpu, crtc->transcoder);
	kbl::pipe::scaler_disable(gpu, crtc);
	kbl::transcoder::clock_disable(gpu, crtc);
	if(DDI_BUF_CTL(con->ddi_id) & DDI_BUF_CTL_PORT_REVERSAL)
		REG(DDI_BUF_CTL(con->ddi_id)) &= ~DDI_BUF_CTL_PORT_REVERSAL;
	REG(DDI_BUF_CTL(con->ddi_id)) &= ~DDI_BUF_CTL_ENABLE;
	kbl::ddi::balance_leg_set(gpu, con->ddi_id, 0);
	lil_usleep(10);
	kbl::ddi::power_disable(gpu, con);
	kbl::ddi::clock_disable(gpu, crtc);
	kbl::pll::disable(gpu, con);
}

namespace {

void enable_vblank(LilGpu *gpu, LilCrtc *crtc) {
	REG(IMR(crtc->pipe_id)) &= ~1;
}

uint32_t kbl_chicken_trans_reg_for_port(LilCrtc *crtc) {
	switch(crtc->transcoder) {
		case TRANSCODER_A: return CHICKEN_TRANS_A;
		case TRANSCODER_B: return CHICKEN_TRANS_B;
		case TRANSCODER_C: return CHICKEN_TRANS_C;
		// case TRANSCODER_D: return CHICKEN_TRANS_D;
		case TRANSCODER_EDP: return CHICKEN_TRANS_EDP;
		default: lil_panic("unhandled transcoder");
	}
}

} // namespace

// TODO(CLEAN;BIT): this function needs to be cleaned up
// 					specifically, we should be using enums or defines for this bit setting/clearing
void commit_modeset(LilGpu *lil_gpu, LilCrtc *crtc) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	LilConnector *con = crtc->connector;

	// TODO: we can't rely on this
	if(gpu->cdclk_freq != gpu->boot_cdclk_freq) {
		kbl::cdclk::set_freq(gpu, gpu->boot_cdclk_freq);
	}

	kbl::crtc::pll_find(gpu, crtc);

	uint32_t stride = ((crtc->current_mode.hactive * 4) + 63) >> 6;

	REG(PRI_STRIDE(crtc->pipe_id)) = stride;
	REG(DSP_ADDR(crtc->pipe_id)) = 0;

	kbl::plane::page_flip(gpu, crtc);

	uint32_t htotal = crtc->current_mode.htotal;
	uint32_t pixel_clock = crtc->current_mode.clock;
	uint32_t pitch = (crtc->current_mode.hactive * 4 + 63) & 0xFFFFFFC0;
	uint32_t linetime = div_u32_ceil(8000 * htotal, pixel_clock);

	REG(PLANE_CTL(crtc->pipe_id)) = (REG(PLANE_CTL(crtc->pipe_id)) & 0xF0FFFFFF) | 0x4000000;

	kbl::pll::dpll_ctrl_enable(gpu, crtc, crtc->current_mode.clock);
	kbl::pll::dpll_clock_set(gpu, crtc);
	kbl::ddi::power_enable(gpu, crtc);

	kbl::transcoder::configure_clock(gpu, crtc);
	kbl::pipe::src_size_set(gpu, crtc);
	kbl::plane::size_set(gpu, crtc);
	enable_vblank(gpu, crtc);
	kbl::transcoder::timings_configure(gpu, crtc);
	kbl::transcoder::bpp_set(gpu, crtc, crtc->current_mode.bpp);
	kbl::pipe::dithering_enable(gpu, crtc, crtc->current_mode.bpp);

	/* Display WA #1143: skl,kbl,cfl */
	if(gpu->gen == GEN_SKL) {
		uint32_t reg = kbl_chicken_trans_reg_for_port(crtc);
		uint32_t val = REG(reg);

		val |= CHICKEN_TRANS_DDI_TRAINING_OVERRIDE_VALUE | CHICKEN_TRANS_DDI_TRAINING_OVERRIDE_ENABLE;

		REG(reg) = val;
		REG(reg);

		lil_usleep(1);

		val &= CHICKEN_TRANS_DDI_TRAINING_OVERRIDE_VALUE | CHICKEN_TRANS_DDI_TRAINING_OVERRIDE_ENABLE;

		REG(reg) = val;
	}

	kbl::transcoder::ddi_setup(gpu, crtc, 0);
	kbl::transcoder::ddi_polarity_setup(gpu, crtc);
	kbl::transcoder::enable(gpu, crtc);

	uint8_t balance_leg = 0;

	if(con->encoder->hdmi.iboost) {
		balance_leg = con->encoder->hdmi.iboost_level;
	} else {
		balance_leg = 3;

		if(gpu->variant != ULX) {
			balance_leg = 1;
		}
	}

	kbl::ddi::balance_leg_set(gpu, con->ddi_id, balance_leg);
	REG(DDI_BUF_CTL(con->ddi_id)) |= DDI_BUF_CTL_ENABLE;
	kbl::plane::enable(gpu, crtc, true);

	uint32_t dip_data[8] = {};
	::hdmi::avi::infoframe_populate(crtc, &dip_data);

	for(size_t i = 0; i < 8; i++) {
		REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_AVI_DATA(i)) = dip_data[i];
	}

	REG(kbl::transcoder::base(crtc->transcoder) + VIDEO_DIP_CTL) |= VIDEO_DIP_CTL_ENABLE_AVI;
}

namespace {

uint8_t even_candidate_div[36] = {
	4, 6, 8, 0xA,
	0xC, 0xE, 0x10, 0x12,
	0x14, 0x18, 0x1C, 0x1E,
	0x20, 0x24, 0x28, 0x2A,
	0x2C, 0x30, 0x34, 0x36,
	0x38, 0x3C, 0x40, 0x42,
	0x44, 0x46, 0x48, 0x4C,
	0x4E, 0x50, 0x54, 0x58,
	0x5A, 0x5C, 0x60, 0x62,
};

uint8_t odd_candidate_div[7] = {
	3, 5, 7, 9, 15, 21, 35,
};

uint64_t dco_central_freq_list[3] = {
	8400000000,
	9000000000,
	9600000000,
};

// TODO(CLEAN;UNCLEAR_ACTIONS)	what the fuck
bool dpll_find_dco_divider(uint8_t *candidate, uint8_t table_len, uint64_t afe_clock, uint8_t *dco_central_freq_index, uint64_t *dco_freq_dev, uint8_t *out_div) {
	bool valid_dco_found = false;

	for(uint8_t dco_central_freq_i = 0; dco_central_freq_i < 3; dco_central_freq_i++) {
		if(table_len) {
			size_t remaining_candidates = table_len;
			uint8_t *candidate_div = candidate;
			bool dco_freq_within_bounds = false;

			do {
				uint64_t dco_freq = afe_clock * *candidate_div;
				uint64_t dco_freq_deviation = 0;

				if(dco_freq <= dco_central_freq_list[dco_central_freq_i]) {
					dco_freq_deviation = 10000 * (dco_central_freq_list[dco_central_freq_i] - dco_freq) / dco_central_freq_list[dco_central_freq_i];
					dco_freq_within_bounds = dco_freq_deviation < 600;
				} else {
					dco_freq_deviation = 10000 * (dco_freq - dco_central_freq_list[dco_central_freq_i]) / dco_central_freq_list[dco_central_freq_i];
					dco_freq_within_bounds = dco_freq_deviation < 100;
				}

				if(dco_freq_within_bounds && dco_freq_deviation <= *dco_freq_dev) {
					valid_dco_found = true;
					*out_div = *candidate_div;
					*dco_central_freq_index = dco_central_freq_i;
					*dco_freq_dev = dco_freq_deviation;
				}

				++candidate_div;
				--remaining_candidates;
			} while(remaining_candidates);
		}
	}

	return valid_dco_found;
}

struct p_div {
	uint32_t p0;
	uint32_t p1;
	uint32_t p2;
};

void get_multiplier(struct p_div *mul, uint8_t num) {
	mul->p0 = 0;
	mul->p1 = 0;
	mul->p2 = 0;

	if ((num & 1) != 0) {
		if (num == 3 || num == 9) {
			mul->p1 = 1;
			mul->p2 = num / 3;
		} else {
			if (num == 5 || num == 7) {
				mul->p0 = num;
				mul->p1 = 1;
				mul->p2 = 1;
				return;
			}
			if (num != 15) {
				if (num == 21) {
					mul->p0 = 7;
					mul->p1 = 1;
					mul->p2 = 3;
				} else if (num == 35) {
					mul->p0 = 7;
					mul->p1 = 1;
					mul->p2 = 5;
				}
				return;
			}
			mul->p1 = 1;
			mul->p2 = 5;
		}
		mul->p0 = 3;
		return;
	}

	uint8_t num_one_shift = num >> 1;

	if (num_one_shift == 1 || num_one_shift == 2 || num_one_shift == 3 || num_one_shift == 5) {
			mul->p0 = 2;
			mul->p1 = 1;
			mul->p2 = num_one_shift;
			return;
	}

	if ((num & 2) != 0) {
		if (num_one_shift == 3 * (num_one_shift / 3)) {
				mul->p0 = 3;
				mul->p1 = num_one_shift / 3;
		} else {
			if (num_one_shift != 7 * (num_one_shift / 7)) {
				return;
			}
			mul->p0 = 7;
			mul->p1 = num_one_shift / 7;
		}
		mul->p2 = 2;
	} else {
		mul->p0 = 2;
		mul->p2 = 2;
		mul->p1 = num >> 2;
	}
}

} // namespace

void pll_enable_sequence(LilGpu *gpu, LilCrtc *crtc) {
	uint64_t afe_clock = 5000 * crtc->current_mode.clock;
	uint8_t dvo_freq_index = 3;
	uint8_t out_div = 0;
	uint64_t dco_freq_dev = 600;
	struct p_div multiplier{};
	struct p_div multipliers{};

	if(dpll_find_dco_divider(even_candidate_div, 36, afe_clock, &dvo_freq_index, &dco_freq_dev, &out_div) ||
		dpll_find_dco_divider(odd_candidate_div, 7, afe_clock, &dvo_freq_index, &dco_freq_dev, &out_div)) {
		get_multiplier(&multiplier, out_div);

		memcpy(&multipliers, &multiplier, sizeof(multiplier));
	}

	uint64_t dco_frequency = afe_clock * out_div;
	uint64_t dco_int = dco_frequency / 24000000;
	uint64_t dco_fraction = ((dco_frequency % 24000000) << 15) / 24000000;
	uint64_t central_freq = dco_central_freq_list[dvo_freq_index] / 1000000;

	REG(DPLL_CFGCR1(crtc->pll_id)) = dco_int | ((dco_fraction | 0xFFC00000) << 9) | (REG(DPLL_CFGCR1(crtc->pll_id) & 0xFF000000));

	uint32_t dpll_cfgcr2_val = 0;

	if(multipliers.p1 > 1) {
		dpll_cfgcr2_val = (multipliers.p1 << 8) | 0x80;
	}

	uint32_t k_div = 0;

	switch(multipliers.p2) {
		case 1:
			k_div = 3;
			break;
		case 2:
			k_div = 1;
			break;
		case 3:
			k_div = 2;
			break;
	}

	dpll_cfgcr2_val |= 0x20 * k_div;

	uint32_t p_div = 0;

	switch(multipliers.p0) {
		case 1:
			break;
		case 2:
			p_div = 1;
			break;
		case 3:
			p_div = 2;
			break;
		case 7:
			p_div = 4;
			break;
	}

	dpll_cfgcr2_val |= 4 * p_div;

	uint32_t central_freq_val = 0;

	if(central_freq == 8400) {
		central_freq_val = 3;
	} else if(central_freq == 9000) {
		central_freq_val = 1;
	}

	REG(DPLL_CFGCR2(crtc->pll_id)) = central_freq_val | dpll_cfgcr2_val | (REG(DPLL_CFGCR2(crtc->pll_id)) & 0xFFFF0000);
}

} // namespace kbl::hdmi
