#include <lil/imports.h>
#include <lil/intel.h>

#include "src/base.hpp"
#include "src/kaby_lake/ddi.hpp"
#include "src/kaby_lake/ddi-translations.hpp"
#include "src/regs.hpp"

namespace kbl::ddi {

bool buf_enabled(LilGpu *gpu, LilCrtc *crtc) {
	return REG(DDI_BUF_CTL(crtc->connector->ddi_id) & DDI_BUF_CTL_ENABLE);
}

bool hotplug_detected(LilGpu *gpu, enum LilDdiId ddi_id) {
	uint32_t mask = 0;

	if(ddi_id == DDI_A) {
		mask = 0x1000000;
	} else if(ddi_id == DDI_D) {
		mask = 0x800000;
	} else {
		lil_panic("unimplemented DDI");
	}

	return (REG(SDEISR) & mask);
}

static const struct lil_ddi_buf_trans *ddi_translation_table(LilGpu *lil_gpu, LilConnector *con, bool hdmi) {
	auto gpu = static_cast<Gpu *>(lil_gpu);

	if(hdmi) {
		if(gpu->gen == GEN_SKL && (gpu->subgen == SUBGEN_NONE || gpu->subgen == SUBGEN_KABY_LAKE)) {
			if(gpu->variant == ULX)
				return &skl_y_trans_hdmi;
			else
				return &skl_trans_hdmi;
		}

		lil_panic("unhandled GPU gen for HDMI translation tables");
	}

	if(gpu->gen == GEN_SKL && gpu->subgen == SUBGEN_NONE) {
		if(gpu->variant == ULT) {
			return &skl_u_trans_edp;
		} else if(gpu->variant == ULX) {
			return &skl_y_trans_edp;
		} else {
			return &skl_trans_edp;
		}
	} else if(gpu->gen == GEN_SKL && gpu->subgen == SUBGEN_KABY_LAKE) {
		if(gpu->variant == ULT) {
				return &kbl_u_trans_dp;
		} else if(gpu->variant == ULX) {
			return &kbl_y_trans_dp;
		} else {
			return &kbl_trans_dp;
		}
	} else {
		lil_panic("buffer_setup_translations unsupported for this GPU gen");
	}
}

static bool has_iboost(LilConnector *con) {
	switch(con->type) {
		case HDMI:
			return con->encoder->hdmi.iboost && con->encoder->hdmi.iboost_level < 3;
		case EDP:
			return con->encoder->edp.edp_iboost && con->encoder->edp.edp_balance_leg_val;
		default:
			lil_panic("unimplemented iboost detection for connector type");
	}
}

#define DDI_BUF_TRANS_HDMI_ENTRY 9

void buffer_setup_translations(LilGpu *gpu, LilConnector *con, uint32_t reg) {
	const struct lil_ddi_buf_trans *table = ddi_translation_table(gpu, con, false);

	if(!table)
		lil_panic("no DDI translations table found");

	uint32_t iboost_flag = has_iboost(con) ? (1 << 31) : 0;

	if(con->type == DISPLAYPORT || con->type == EDP) {
		for(size_t i = 0; i < table->entries.size(); i++) {
			const struct lil_ddi_buf_trans_entry *t = &table->entries[i];

			REG(reg + (8 * i) + 0) = t->trans1 | iboost_flag;
			REG(reg + (8 * i) + 4) = t->trans2;
		}
	} else if(con->type == HDMI) {
		const struct lil_ddi_buf_trans *hdmi_table = ddi_translation_table(gpu, con, true);
		uint8_t hdmi_level = con->encoder->hdmi.hdmi_level_shift;

		REG(reg + (8 * DDI_BUF_TRANS_HDMI_ENTRY) + 0) = hdmi_table->entries[hdmi_level].trans1 | iboost_flag;
		REG(reg + (8 * DDI_BUF_TRANS_HDMI_ENTRY) + 4) = hdmi_table->entries[hdmi_level].trans2;
	}
}

void power_enable(LilGpu *gpu, LilCrtc *crtc) {
	uint32_t val = 0;
	uint32_t wait_mask = 0;

	switch(crtc->connector->ddi_id) {
		case DDI_A:
		case DDI_E: {
			val = 8;
			wait_mask = 4;
			break;
		}
		case DDI_B: {
			val = 0x20;
			wait_mask = 0x10;
			break;
		}
		case DDI_C: {
			val = 0x80;
			wait_mask = 0x40;
			break;
		}
		case DDI_D: {
			val = 0x200;
			wait_mask = 0x100;
			break;
		}
	}

	REG(HSW_PWR_WELL_CTL1) |= val;

	if(!wait_for_bit_set(REG_PTR(HSW_PWR_WELL_CTL1), wait_mask, 20, 1))
		lil_panic("timeout on DDI powerup?");
}

void power_disable(LilGpu *gpu, LilConnector *con) {
	switch(con->ddi_id) {
		case DDI_B:
			REG(HSW_PWR_WELL_CTL1) &= 0xFFFFFFDF;
			break;
		case DDI_C:
			REG(HSW_PWR_WELL_CTL1) &= 0xFFFFFF7F;
			break;
		case DDI_D:
			REG(HSW_PWR_WELL_CTL1) &= 0xFFFFFDFF;
			break;
		default:
			/* TODO: only shut this down if there are two enabled CRTC, at least one of which that isn't LFP */
			lil_log(WARNING, "shutdown of DDI A or E is unhandled!\n");
			break;
	}
}

void balance_leg_set(LilGpu *gpu, enum LilDdiId ddi_id, uint8_t balance_leg) {
	REG(DISPIO_CR_TX_BMU_CR0) = DISPIO_CR_TX_BMU_CR0_DDI_BALANCE_LEG(ddi_id, balance_leg) | (REG(DISPIO_CR_TX_BMU_CR0) & ~DISPIO_CR_TX_BMU_CR0_DDI_BALANCE_LEG_MASK(ddi_id));
}

void clock_disable(LilGpu *gpu, LilCrtc *crtc) {
	REG(DPLL_CTRL2) |= (1 << (15 + crtc->connector->ddi_id));
}

} // namespace kbl::ddi
