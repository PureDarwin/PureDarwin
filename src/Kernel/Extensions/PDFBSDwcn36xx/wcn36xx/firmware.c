/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

// firmware.c - setting and testing hal feature capability bits, word cap / 32 and bit cap % 32
// as prima's setFeatCaps and getFeatCaps in riva/inc/wlan_hal_msg.h do
// SPDX-License-Identifier: ISC

#include "wcn36xx.h"
#include "firmware.h"

static const char * const cap_names[] = {
	[0] = "MCC",
	[1] = "P2P",
	[2] = "DOT11AC",
	[3] = "SLM_SESSIONIZATION",
	[4] = "DOT11AC_OPMODE",
	[5] = "SAP32STA",
	[6] = "TDLS",
	[7] = "P2P_GO_NOA_DECOUPLE_INIT_SCAN",
	[8] = "WLANACTIVE_OFFLOAD",
	[9] = "BEACON_OFFLOAD",
	[10] = "SCAN_OFFLOAD",
	[11] = "ROAM_OFFLOAD",
	[12] = "BCN_MISS_OFFLOAD",
	[13] = "STA_POWERSAVE",
	[14] = "STA_ADVANCED_PWRSAVE",
	[15] = "AP_UAPSD",
	[16] = "AP_DFS",
	[17] = "BLOCKACK",
	[18] = "PHY_ERR",
	[19] = "BCN_FILTER",
	[20] = "RTT",
	[21] = "RATECTRL",
	[22] = "WOW",
	[23] = "WLAN_ROAM_SCAN_OFFLOAD",
	[24] = "SPECULATIVE_PS_POLL",
	[25] = "SCAN_SCH",
	[26] = "IBSS_HEARTBEAT_OFFLOAD",
	[27] = "WLAN_SCAN_OFFLOAD",
	[28] = "WLAN_PERIODIC_TX_PTRN",
	[29] = "ADVANCE_TDLS",
	[30] = "BATCH_SCAN",
	[31] = "FW_IN_TX_PATH",
	[32] = "EXTENDED_NSOFFLOAD_SLOT",
	[33] = "CH_SWITCH_V1",
	[34] = "HT40_OBSS_SCAN",
	[35] = "UPDATE_CHANNEL_LIST",
	[36] = "WLAN_MCADDR_FLT",
	[37] = "WLAN_CH144",
	[38] = "WLAN_NAN",
	[39] = "TDLS_SCAN_COEXISTENCE",
	[40] = "LINK_LAYER_STATS_MEAS",
	[41] = "MU_MIMO",
	[42] = "EXTENDED_SCAN",
	[43] = "DYNAMIC_WMM_PS",
	[44] = "MAC_SPOOFED_SCAN",
	[45] = "BMU_ERROR_GENERIC_RECOVERY",
	[46] = "DISA",
	[47] = "FW_STATS",
	[48] = "WPS_PRBRSP_TMPL",
	[49] = "BCN_IE_FLT_DELTA",
	[51] = "TDLS_OFF_CHANNEL",
	[52] = "RTT3",
	[53] = "MGMT_FRAME_LOGGING",
	[54] = "ENHANCED_TXBD_COMPLETION",
	[55] = "LOGGING_ENHANCEMENT",
	[56] = "EXT_SCAN_ENHANCED",
	[57] = "MEMORY_DUMP_SUPPORTED",
	[58] = "PER_PKT_STATS_SUPPORTED",
	[60] = "EXT_LL_STAT",
	[61] = "WIFI_CONFIG",
	[62] = "ANTENNA_DIVERSITY_SELECTION",
	[128] = "MAX_FEATURE_SUPPORTED",
};

void
wcn36xx_firmware_set_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap)
{
	if (cap < 128)
		bitmap[cap / 32] |= 1u << (cap % 32);
}

int
wcn36xx_firmware_get_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap)
{
	return cap < 128 ? (bitmap[cap / 32] >> (cap % 32)) & 1 : 0;
}

void
wcn36xx_firmware_clear_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap)
{
	if (cap < 128)
		bitmap[cap / 32] &= ~(1u << (cap % 32));
}

const char *
wcn36xx_firmware_get_cap_name(enum wcn36xx_firmware_feat_caps x)
{
	if (x >= ARRAY_SIZE(cap_names) || cap_names[x] == NULL)
		return "UNKNOWN";
	return cap_names[x];
}
