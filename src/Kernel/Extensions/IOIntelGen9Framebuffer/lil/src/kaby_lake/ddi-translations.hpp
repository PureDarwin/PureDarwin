#pragma once

#include "src/pd_kext_prims.hpp"
#include <stdint.h>

struct lil_ddi_buf_trans_entry {
	uint32_t trans1;
	uint32_t trans2;
	uint8_t iboost;
};

struct lil_ddi_buf_trans {
	const lil::span<const struct lil_ddi_buf_trans_entry> &entries;
	uint8_t hdmi_default_entry;
};

extern const struct lil_ddi_buf_trans kbl_trans_dp;
extern const struct lil_ddi_buf_trans kbl_u_trans_dp;
extern const struct lil_ddi_buf_trans kbl_y_trans_dp;
extern const struct lil_ddi_buf_trans skl_trans_edp;
extern const struct lil_ddi_buf_trans skl_u_trans_edp;
extern const struct lil_ddi_buf_trans skl_y_trans_edp;
extern const struct lil_ddi_buf_trans skl_trans_hdmi;
extern const struct lil_ddi_buf_trans skl_y_trans_hdmi;
