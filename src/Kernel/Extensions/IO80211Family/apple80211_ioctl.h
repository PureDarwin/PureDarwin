/*
 * apple80211_ioctl.h - the requests an 802.11 interface answers, shaped like
 * Apple's: an apple80211req naming the interface, a request type, a value
 * and a buffer, sent with SIOCSA80211 to set and SIOCGA80211 to get.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#ifndef _APPLE80211_IOCTL_H_
#define _APPLE80211_IOCTL_H_

#include <stdint.h>
#include <sys/ioccom.h>

#define APPLE80211_IFNAMSIZ	16

struct apple80211req {
	char		req_if_name[APPLE80211_IFNAMSIZ];
	int		req_type;
	int		req_val;
	uint32_t	req_len;
	void		*req_data;
};

#define SIOCSA80211	_IOW('i', 200, struct apple80211req)
#define SIOCGA80211	_IOWR('i', 201, struct apple80211req)

// request types
#define APPLE80211_IOC_SSID		1
#define APPLE80211_IOC_AUTH_TYPE	2
#define APPLE80211_IOC_CHANNEL		4
#define APPLE80211_IOC_POWERSAVE	5
#define APPLE80211_IOC_TXPOWER		7
#define APPLE80211_IOC_RATE		8
#define APPLE80211_IOC_BSSID		9
#define APPLE80211_IOC_SCAN_REQ		10
#define APPLE80211_IOC_SCAN_RESULT	11
#define APPLE80211_IOC_CARD_CAPABILITIES 12
#define APPLE80211_IOC_STATE		13
#define APPLE80211_IOC_PHY_MODE		14
#define APPLE80211_IOC_OP_MODE		15
#define APPLE80211_IOC_RSSI		16
#define APPLE80211_IOC_NOISE		17
#define APPLE80211_IOC_POWER		19
#define APPLE80211_IOC_ASSOCIATE	20
#define APPLE80211_IOC_ASSOCIATE_RESULT	21
#define APPLE80211_IOC_DISASSOCIATE	22
#define APPLE80211_IOC_HW_ADDR		23
#define APPLE80211_IOC_COUNTRY_CODE	51

#define APPLE80211_VERSION		1
#define APPLE80211_MAX_SSID_LEN		32
#define APPLE80211_ADDR_LEN		6
#define APPLE80211_KEY_BUFF_LEN		64
#define APPLE80211_MAX_RATES		15
#define APPLE80211_MAX_IE_LEN		512

// link states
enum apple80211_state {
	APPLE80211_S_INIT	= 0,
	APPLE80211_S_SCAN	= 1,
	APPLE80211_S_AUTH	= 2,
	APPLE80211_S_ASSOC	= 3,
	APPLE80211_S_RUN	= 4,
};

// phy modes, as a mask in capabilities and one bit for the current mode
#define APPLE80211_MODE_11A	0x0002
#define APPLE80211_MODE_11B	0x0004
#define APPLE80211_MODE_11G	0x0008
#define APPLE80211_MODE_11N	0x0010
#define APPLE80211_MODE_11AC	0x0080

// how a network is secured
#define APPLE80211_AUTHTYPE_NONE	0x0000
#define APPLE80211_AUTHTYPE_WPA_PSK	0x0004
#define APPLE80211_AUTHTYPE_WPA2_PSK	0x0010
#define APPLE80211_AUTHTYPE_WEP		0x0100

struct apple80211_channel {
	uint32_t	version;
	uint32_t	channel;
	uint32_t	flags;
};
#define APPLE80211_C_FLAG_2GHZ	0x02
#define APPLE80211_C_FLAG_5GHZ	0x04
#define APPLE80211_C_FLAG_40MHZ	0x08

struct apple80211_ssid_data {
	uint32_t	version;
	uint32_t	ssid_len;
	uint8_t		ssid_bytes[APPLE80211_MAX_SSID_LEN];
};

struct apple80211_bssid_data {
	uint32_t	version;
	uint8_t		bssid[APPLE80211_ADDR_LEN];
};

struct apple80211_state_data {
	uint32_t	version;
	uint32_t	state;
};

struct apple80211_rssi_data {
	uint32_t	version;
	int32_t		rssi;		// dBm
	int32_t		noise;		// dBm
};

struct apple80211_phymode_data {
	uint32_t	version;
	uint32_t	phy_mode;	// supported modes
	uint32_t	active_phy_mode;
};

struct apple80211_power_data {
	uint32_t	version;
	uint32_t	power_state;	// 1 radio on, 0 off
};

struct apple80211_capability_data {
	uint32_t	version;
	uint32_t	phy_modes;
	uint32_t	auth_types;
};

struct apple80211_hwaddr_data {
	uint32_t	version;
	uint8_t		addr[APPLE80211_ADDR_LEN];
};

// one scanned network, SCAN_RESULT returns them one at a time by req_val index
struct apple80211_scan_result {
	uint32_t	version;
	struct apple80211_channel asr_channel;
	int16_t		asr_noise;
	int16_t		asr_rssi;
	uint16_t	asr_beacon_int;
	uint16_t	asr_cap;
	uint8_t		asr_bssid[APPLE80211_ADDR_LEN];
	uint8_t		asr_nrates;
	uint32_t	asr_rates[APPLE80211_MAX_RATES];
	uint8_t		asr_ssid_len;
	uint8_t		asr_ssid[APPLE80211_MAX_SSID_LEN];
	uint32_t	asr_auth_types;	// APPLE80211_AUTHTYPE_* the network offers
	uint32_t	asr_age;	// ms since last seen
};

struct apple80211_scan_data {
	uint32_t	version;
	uint32_t	ssid_len;	// 0 for a broadcast scan
	uint8_t		ssid[APPLE80211_MAX_SSID_LEN];
};

// join a network: open, or wpa2 with the 32-byte pmk userland derived from the passphrase
struct apple80211_assoc_data {
	uint32_t	version;
	uint32_t	ad_auth_type;	// APPLE80211_AUTHTYPE_*
	uint32_t	ad_ssid_len;
	uint8_t		ad_ssid[APPLE80211_MAX_SSID_LEN];
	uint8_t		ad_bssid[APPLE80211_ADDR_LEN];	// zero for any
	uint32_t	ad_key_len;
	uint8_t		ad_key[APPLE80211_KEY_BUFF_LEN];
};

#endif
