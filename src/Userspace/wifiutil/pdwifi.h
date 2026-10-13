/*
 * pdwifi.h - Wi-Fi for the command line and the desktop: the radio, scans, joining and the
 * networks this computer remembers, over the interface's SIOCGA80211 and SIOCSA80211 requests.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef PDWIFI_H
#define PDWIFI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PDWIFI_MAX_NETWORKS 64

// where the remembered networks live, one "ssid<TAB>pmk-hex" line each, root only
#define PDWIFI_KNOWN_PATH "/Library/Preferences/SystemConfiguration/org.puredarwin.wifi.known"

typedef struct {
	char ssid[33];
	uint8_t bssid[6];
	int rssi;
	unsigned channel;
	int secure;
	const char *security;
} pdwifi_network;

typedef enum {
	PDWIFI_OK = 0,
	PDWIFI_NO_HARDWARE,
	PDWIFI_OFF,
	PDWIFI_BAD_PASSWORD,
	PDWIFI_FAILED,
	PDWIFI_TIMEOUT,
	PDWIFI_NOT_FOUND,
} pdwifi_result;

// the Wi-Fi interface, the first enN that answers the 802.11 state request, NULL if there is none
const char *pdwifi_interface(void);
void pdwifi_set_interface(const char *name);

int pdwifi_power(void);
pdwifi_result pdwifi_set_power(int on);

// a fresh scan, one entry per network name with its strongest access point, strongest first
int pdwifi_scan(pdwifi_network *out, int max);

// the joined network, 1 when associated and filled in, 0 when not
int pdwifi_current(pdwifi_network *out);
int pdwifi_state(void);
int pdwifi_hwaddr(uint8_t mac[6]);

// joins and waits for the link, a password of NULL is an open network
pdwifi_result pdwifi_join(const char *ssid, const char *password, int remember);
pdwifi_result pdwifi_join_known(const char *ssid);
pdwifi_result pdwifi_leave(void);
pdwifi_result pdwifi_autojoin(void);

int pdwifi_known(char names[][33], int max);
int pdwifi_is_known(const char *ssid);
void pdwifi_forget(const char *ssid);

// signal level for the glyph, 1 to 3
int pdwifi_bars(int rssi);
const char *pdwifi_message(pdwifi_result r);

#ifdef __cplusplus
}
#endif

#endif
