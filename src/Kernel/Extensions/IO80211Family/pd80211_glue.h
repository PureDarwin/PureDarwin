/*
 * pd80211_glue.h - what IO80211Controller asks of net80211, in plain C so the
 * C++ side never sees the OpenBSD headers. Every call takes the stack lock.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef PD80211_GLUE_H
#define PD80211_GLUE_H

#include <stdint.h>
#include <sys/kpi_mbuf.h>
#include "apple80211_ioctl.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ieee80211com;

void	pd80211_init(void);

// frames up the stack and link changes come back through these
typedef void (*pd80211_input_fn)(void *owner, mbuf_t m);
typedef void (*pd80211_link_fn)(void *owner, int up);
void	pd80211_hook(struct ieee80211com *ic, void *owner, pd80211_input_fn input, pd80211_link_fn link);

const uint8_t *pd80211_macaddr(struct ieee80211com *ic);
int	pd80211_output(struct ieee80211com *ic, mbuf_t m);
int	pd80211_set_up(struct ieee80211com *ic, int up);
int	pd80211_is_up(struct ieee80211com *ic);

// the apple80211 requests
int	pd80211_state(struct ieee80211com *ic);
int	pd80211_ssid(struct ieee80211com *ic, struct apple80211_ssid_data *out);
int	pd80211_bssid(struct ieee80211com *ic, struct apple80211_bssid_data *out);
int	pd80211_channel(struct ieee80211com *ic, struct apple80211_channel *out);
int	pd80211_rssi(struct ieee80211com *ic, struct apple80211_rssi_data *out);
int	pd80211_phymode(struct ieee80211com *ic, struct apple80211_phymode_data *out);
int	pd80211_set_band(struct ieee80211com *ic, uint32_t phy_mode);
int	pd80211_capabilities(struct ieee80211com *ic, struct apple80211_capability_data *out);
int	pd80211_scan(struct ieee80211com *ic, const struct apple80211_scan_data *req);
int	pd80211_scan_result(struct ieee80211com *ic, int index, struct apple80211_scan_result *out);
int	pd80211_associate(struct ieee80211com *ic, const struct apple80211_assoc_data *req);
int	pd80211_disassociate(struct ieee80211com *ic);

#ifdef __cplusplus
}
#endif

#endif
