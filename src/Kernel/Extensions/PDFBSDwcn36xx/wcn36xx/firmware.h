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

// firmware.h - the hal feature capability bits, from prima's riva/inc/wlan_hal_msg.h placeHolderInCapBitmap
// SPDX-License-Identifier: ISC

#ifndef _FIRMWARE_H_
#define _FIRMWARE_H_

#include <linux/types.h>

enum wcn36xx_firmware_feat_caps {
	MCC = 0,
	P2P = 1,
	DOT11AC = 2,
	SLM_SESSIONIZATION = 3,
	DOT11AC_OPMODE = 4,
	SAP32STA = 5,
	TDLS = 6,
	P2P_GO_NOA_DECOUPLE_INIT_SCAN = 7,
	WLANACTIVE_OFFLOAD = 8,
	BEACON_OFFLOAD = 9,
	SCAN_OFFLOAD = 10,
	ROAM_OFFLOAD = 11,
	BCN_MISS_OFFLOAD = 12,
	STA_POWERSAVE = 13,
	STA_ADVANCED_PWRSAVE = 14,
	AP_UAPSD = 15,
	AP_DFS = 16,
	BLOCKACK = 17,
	PHY_ERR = 18,
	BCN_FILTER = 19,
	RTT = 20,
	RATECTRL = 21,
	WOW = 22,
	WLAN_ROAM_SCAN_OFFLOAD = 23,
	SPECULATIVE_PS_POLL = 24,
	SCAN_SCH = 25,
	IBSS_HEARTBEAT_OFFLOAD = 26,
	WLAN_SCAN_OFFLOAD = 27,
	WLAN_PERIODIC_TX_PTRN = 28,
	ADVANCE_TDLS = 29,
	BATCH_SCAN = 30,
	FW_IN_TX_PATH = 31,
	EXTENDED_NSOFFLOAD_SLOT = 32,
	CH_SWITCH_V1 = 33,
	HT40_OBSS_SCAN = 34,
	UPDATE_CHANNEL_LIST = 35,
	WLAN_MCADDR_FLT = 36,
	WLAN_CH144 = 37,
	WLAN_NAN = 38,
	TDLS_SCAN_COEXISTENCE = 39,
	LINK_LAYER_STATS_MEAS = 40,
	MU_MIMO = 41,
	EXTENDED_SCAN = 42,
	DYNAMIC_WMM_PS = 43,
	MAC_SPOOFED_SCAN = 44,
	BMU_ERROR_GENERIC_RECOVERY = 45,
	DISA = 46,
	FW_STATS = 47,
	WPS_PRBRSP_TMPL = 48,
	BCN_IE_FLT_DELTA = 49,
	TDLS_OFF_CHANNEL = 51,
	RTT3 = 52,
	MGMT_FRAME_LOGGING = 53,
	ENHANCED_TXBD_COMPLETION = 54,
	LOGGING_ENHANCEMENT = 55,
	EXT_SCAN_ENHANCED = 56,
	MEMORY_DUMP_SUPPORTED = 57,
	PER_PKT_STATS_SUPPORTED = 58,
	EXT_LL_STAT = 60,
	WIFI_CONFIG = 61,
	ANTENNA_DIVERSITY_SELECTION = 62,
	MAX_FEATURE_SUPPORTED = 128,
};

void wcn36xx_firmware_set_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap);
int wcn36xx_firmware_get_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap);
void wcn36xx_firmware_clear_feat_caps(u32 *bitmap, enum wcn36xx_firmware_feat_caps cap);
const char *wcn36xx_firmware_get_cap_name(enum wcn36xx_firmware_feat_caps x);

#endif
