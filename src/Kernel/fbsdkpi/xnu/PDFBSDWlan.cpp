// SPDX-License-Identifier: MIT
// the apple80211 requests of a FreeBSD wifi driver, answered from its
// net80211 station vap through the pd_wlan calls

#include <IOKit/IOLib.h>
#include <sys/errno.h>

#include "PDFBSDEthernet.h"
#include "pd_fbsd_attach.h"

#define NEED(type) do { if (*len < sizeof(type)) return EINVAL; *len = sizeof(type); } while (0)

static uint32_t
channel_flags(uint16_t flags)
{
	// net80211's IEEE80211_CHAN_2GHZ, IEEE80211_CHAN_5GHZ and the ht40 pair
	uint32_t out = 0;
	if (flags & 0x0080)
		out |= APPLE80211_C_FLAG_2GHZ;
	if (flags & 0x0100)
		out |= APPLE80211_C_FLAG_5GHZ;
	if (flags & 0x60000)
		out |= APPLE80211_C_FLAG_40MHZ;
	return out;
}

static uint32_t
apple_state(uint32_t state, bool authorized)
{
	switch (state) {
	case PD_WLAN_S_SCAN:
		return APPLE80211_S_SCAN;
	case PD_WLAN_S_AUTH:
		return APPLE80211_S_AUTH;
	case PD_WLAN_S_ASSOC:
		return APPLE80211_S_ASSOC;
	case PD_WLAN_S_RUN:
		// a wpa2 link only carries data once the handshake opened the port
		return authorized ? APPLE80211_S_RUN : APPLE80211_S_ASSOC;
	default:
		return APPLE80211_S_INIT;
	}
}

SInt32
PDFBSD_CLASS::apple80211Request(bool set, int type, int value, void *data, uint32_t *len)
{
	if (fIfp == NULL)
		return ENXIO;

	if (set) {
		switch (type) {
		case APPLE80211_IOC_SCAN_REQ: {
			struct apple80211_scan_data *scan = (struct apple80211_scan_data *)data;
			if (*len < sizeof(*scan) || scan->ssid_len > APPLE80211_MAX_SSID_LEN)
				return pd_wlan_scan(fIfp, NULL, 0);
			return pd_wlan_scan(fIfp, scan->ssid, scan->ssid_len);
		}
		case APPLE80211_IOC_ASSOCIATE: {
			NEED(struct apple80211_assoc_data);
			struct apple80211_assoc_data *assoc = (struct apple80211_assoc_data *)data;
			if (assoc->ad_ssid_len > APPLE80211_MAX_SSID_LEN)
				return EINVAL;
			const uint8_t *pmk = NULL;
			if (assoc->ad_auth_type == APPLE80211_AUTHTYPE_WPA2_PSK) {
				if (assoc->ad_key_len != 32)
					return EINVAL;
				pmk = assoc->ad_key;
			} else if (assoc->ad_auth_type != APPLE80211_AUTHTYPE_NONE) {
				return EOPNOTSUPP;
			}
			return pd_wlan_join(fIfp, assoc->ad_ssid, assoc->ad_ssid_len,
				assoc->ad_bssid, pmk);
		}
		case APPLE80211_IOC_DISASSOCIATE:
			return pd_wlan_leave(fIfp);
		case APPLE80211_IOC_POWER: {
			NEED(struct apple80211_power_data);
			struct apple80211_power_data *power = (struct apple80211_power_data *)data;
			return power->power_state ? pd_fbsd_up(fIfp) : pd_fbsd_down(fIfp);
		}
		default:
			return EOPNOTSUPP;
		}
	}

	struct pd_wlan_status status;
	pd_wlan_get_status(fIfp, &status);

	switch (type) {
	case APPLE80211_IOC_STATE: {
		NEED(struct apple80211_state_data);
		struct apple80211_state_data *out = (struct apple80211_state_data *)data;
		out->version = APPLE80211_VERSION;
		out->state = apple_state(status.state, status.authorized != 0);
		return 0;
	}
	case APPLE80211_IOC_SSID: {
		NEED(struct apple80211_ssid_data);
		struct apple80211_ssid_data *out = (struct apple80211_ssid_data *)data;
		out->version = APPLE80211_VERSION;
		out->ssid_len = status.ssid_len;
		memcpy(out->ssid_bytes, status.ssid, status.ssid_len);
		return status.ssid_len != 0 ? 0 : ENXIO;
	}
	case APPLE80211_IOC_BSSID: {
		NEED(struct apple80211_bssid_data);
		struct apple80211_bssid_data *out = (struct apple80211_bssid_data *)data;
		out->version = APPLE80211_VERSION;
		memcpy(out->bssid, status.bssid, APPLE80211_ADDR_LEN);
		return status.state == PD_WLAN_S_RUN ? 0 : ENXIO;
	}
	case APPLE80211_IOC_CHANNEL: {
		NEED(struct apple80211_channel);
		struct apple80211_channel *out = (struct apple80211_channel *)data;
		out->version = APPLE80211_VERSION;
		out->channel = status.channel;
		out->flags = channel_flags(status.chan_flags);
		return status.channel != 0 ? 0 : ENXIO;
	}
	case APPLE80211_IOC_RSSI: {
		NEED(struct apple80211_rssi_data);
		struct apple80211_rssi_data *out = (struct apple80211_rssi_data *)data;
		out->version = APPLE80211_VERSION;
		out->rssi = status.rssi;
		out->noise = status.noise;
		return 0;
	}
	case APPLE80211_IOC_CARD_CAPABILITIES: {
		NEED(struct apple80211_capability_data);
		struct apple80211_capability_data *out = (struct apple80211_capability_data *)data;
		out->version = APPLE80211_VERSION;
		out->phy_modes = APPLE80211_MODE_11A | APPLE80211_MODE_11B | APPLE80211_MODE_11G
			| APPLE80211_MODE_11N | APPLE80211_MODE_11AC;
		out->auth_types = APPLE80211_AUTHTYPE_NONE | APPLE80211_AUTHTYPE_WPA2_PSK;
		return 0;
	}
	case APPLE80211_IOC_POWER: {
		NEED(struct apple80211_power_data);
		struct apple80211_power_data *out = (struct apple80211_power_data *)data;
		out->version = APPLE80211_VERSION;
		out->power_state = fEnabled ? 1 : 0;
		return 0;
	}
	case APPLE80211_IOC_HW_ADDR: {
		NEED(struct apple80211_hwaddr_data);
		struct apple80211_hwaddr_data *out = (struct apple80211_hwaddr_data *)data;
		out->version = APPLE80211_VERSION;
		return pd_fbsd_get_lladdr(fIfp, out->addr) == 0 ? 0 : ENXIO;
	}
	case APPLE80211_IOC_SCAN_RESULT: {
		NEED(struct apple80211_scan_result);
		if (value == 0) {
			int count = pd_wlan_scan_results(fIfp, fScanCache,
				sizeof(fScanCache) / sizeof(fScanCache[0]));
			fScanCount = count > 0 ? count : 0;
		}
		if (value < 0 || value >= fScanCount)
			return ENOENT;

		const struct pd_wlan_bss *bss = &fScanCache[value];
		struct apple80211_scan_result *out = (struct apple80211_scan_result *)data;
		memset(out, 0, sizeof(*out));
		out->version = APPLE80211_VERSION;
		out->asr_channel.version = APPLE80211_VERSION;
		out->asr_channel.channel = bss->channel;
		out->asr_channel.flags = channel_flags(bss->chan_flags);
		out->asr_noise = bss->noise;
		out->asr_rssi = bss->rssi;
		out->asr_beacon_int = bss->intval;
		out->asr_cap = bss->capinfo;
		memcpy(out->asr_bssid, bss->bssid, APPLE80211_ADDR_LEN);
		out->asr_ssid_len = bss->ssid_len;
		memcpy(out->asr_ssid, bss->ssid, bss->ssid_len);
		if (bss->has_rsn)
			out->asr_auth_types |= APPLE80211_AUTHTYPE_WPA2_PSK;
		if (bss->has_wpa)
			out->asr_auth_types |= APPLE80211_AUTHTYPE_WPA_PSK;
		// the privacy bit alone means wep
		if (!bss->has_rsn && !bss->has_wpa)
			out->asr_auth_types |= (bss->capinfo & 0x0010) ? APPLE80211_AUTHTYPE_WEP
				: APPLE80211_AUTHTYPE_NONE;
		return 0;
	}
	default:
		return EOPNOTSUPP;
	}
}
