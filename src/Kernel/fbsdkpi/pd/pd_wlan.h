// SPDX-License-Identifier: MIT
// what the xnu side calls to drive a net80211 station vap
// the ifp is the one pd_fbsd_attach_pci returned, fixed-width types only
#ifndef PD_WLAN_H
#define PD_WLAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PD_WLAN_MAX_SSID	32

// one network from the scan cache
struct pd_wlan_bss {
	uint8_t		bssid[6];
	uint8_t		ssid_len;
	uint8_t		ssid[PD_WLAN_MAX_SSID];
	uint16_t	freq;
	uint16_t	channel;
	uint16_t	chan_flags;
	int8_t		rssi;
	int8_t		noise;
	uint16_t	capinfo;
	uint16_t	intval;
	uint8_t		has_rsn;
	uint8_t		has_wpa;
};

// net80211 states, the same numbers as enum ieee80211_state
enum {
	PD_WLAN_S_INIT = 0,
	PD_WLAN_S_SCAN = 1,
	PD_WLAN_S_AUTH = 2,
	PD_WLAN_S_ASSOC = 3,
	PD_WLAN_S_CAC = 4,
	PD_WLAN_S_RUN = 5,
	PD_WLAN_S_CSA = 6,
	PD_WLAN_S_SLEEP = 7,
};

struct pd_wlan_status {
	uint32_t	state;
	uint8_t		ssid_len;
	uint8_t		ssid[PD_WLAN_MAX_SSID];
	uint8_t		bssid[6];
	uint16_t	channel;
	uint16_t	chan_flags;
	int32_t		rssi;
	int32_t		noise;
	// the port is open: open networks once associated, wpa2 once keys are in
	int32_t		authorized;
};

int pd_wlan_scan(void *ifp, const uint8_t *ssid, uint32_t ssid_len);
int pd_wlan_scan_results(void *ifp, struct pd_wlan_bss *out, int max);
// pmk is NULL for an open network, else the 32-byte wpa2-psk key
int pd_wlan_join(void *ifp, const uint8_t *ssid, uint32_t ssid_len,
	const uint8_t *bssid, const uint8_t *pmk);
int pd_wlan_leave(void *ifp);
int pd_wlan_get_status(void *ifp, struct pd_wlan_status *status);

#ifdef __cplusplus
}
#endif

#endif
