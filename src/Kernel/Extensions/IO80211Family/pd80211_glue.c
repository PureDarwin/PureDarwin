/*
 * pd80211_glue.c - IO80211Controller's requests carried out on net80211. A
 * configuration change resets the radio the way OpenBSD drivers answer
 * ENETRESET, by taking the interface down and up again.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_priv.h>
#include "pd80211_glue.h"


// from openbsd's ieee80211_ioctl.c, which the apple80211 requests replace
void
ieee80211_disable_wep(struct ieee80211com *ic)
{
	struct ieee80211_key *k;
	int i;
	
	for (i = 0; i < IEEE80211_WEP_NKID; i++) {
		k = &ic->ic_nw_keys[i];
		if (k->k_cipher != IEEE80211_CIPHER_NONE)
			(*ic->ic_delete_key)(ic, NULL, k);
		explicit_bzero(k, sizeof(*k));
	}
	ic->ic_flags &= ~IEEE80211_F_WEPON;
}

void
ieee80211_disable_rsn(struct ieee80211com *ic)
{
	ic->ic_flags &= ~(IEEE80211_F_PSK | IEEE80211_F_RSNON);
	ic->ic_flags &= ~IEEE80211_F_MFPR;
	explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
	ic->ic_rsnprotos = 0;
	ic->ic_rsnakms = 0;
	ic->ic_rsngroupcipher = 0;
	ic->ic_rsnciphers = 0;
}


// drivers hand down what they do not handle, the stack has no ioctls of its own here
int
ieee80211_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	(void)ifp;
	(void)data;
	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		return 0;
	default:
		return ENOTTY;
	}
}

struct pd80211_hooks {
	void			*owner;
	pd80211_input_fn	input;
	pd80211_link_fn		link;
};

static void
glue_input(struct ifnet *ifp, struct mbuf *m)
{
	struct pd80211_hooks *h = ifp->if_pdowner;

	if (h && h->input)
		h->input(h->owner, m);
	else
		m_freem(m);
}

static void
glue_link(struct ifnet *ifp)
{
	struct pd80211_hooks *h = ifp->if_pdowner;

	if (h && h->link)
		h->link(h->owner, LINK_STATE_IS_UP(ifp->if_link_state));
}

void
pd80211_hook(struct ieee80211com *ic, void *owner, pd80211_input_fn input, pd80211_link_fn link)
{
	struct ifnet *ifp = &ic->ic_if;
	struct pd80211_hooks *h = malloc(sizeof(*h), M_DEVBUF, M_WAITOK | M_ZERO);

	h->owner = owner;
	h->input = input;
	h->link = link;
	pd80211_lock();
	ifp->if_pdowner = h;
	ifp->if_pdinput = glue_input;
	ifp->if_pdlinkchange = glue_link;
	pd80211_unlock();
}

const uint8_t *
pd80211_macaddr(struct ieee80211com *ic)
{
	return ic->ic_myaddr;
}

int
pd80211_output(struct ieee80211com *ic, mbuf_t m)
{
	struct ifnet *ifp = &ic->ic_if;
	int error;

	pd80211_lock();
	if ((ifp->if_flags & IFF_RUNNING) == 0 || ic->ic_state != IEEE80211_S_RUN) {
		pd80211_unlock();
		m_freem(m);
		return ENETDOWN;
	}
	error = if_enqueue(ifp, m);
	pd80211_unlock();
	return error;
}

static int
glue_ioctl_flags(struct ifnet *ifp)
{
	return ifp->if_ioctl ? ifp->if_ioctl(ifp, SIOCSIFFLAGS, NULL) : ENODEV;
}

int
pd80211_set_up(struct ieee80211com *ic, int up)
{
	struct ifnet *ifp = &ic->ic_if;
	int error;

	pd80211_lock();
	if (up)
		ifp->if_flags |= IFF_UP;
	else
		ifp->if_flags &= ~IFF_UP;
	error = glue_ioctl_flags(ifp);
	pd80211_unlock();
	return error;
}

int
pd80211_is_up(struct ieee80211com *ic)
{
	return (ic->ic_if.if_flags & IFF_RUNNING) != 0;
}

// a changed configuration takes effect on a fresh start of the radio
static int
glue_reset(struct ieee80211com *ic)
{
	struct ifnet *ifp = &ic->ic_if;
	int error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING)) {
		ifp->if_flags |= IFF_UP;
		return glue_ioctl_flags(ifp);
	}
	ifp->if_flags &= ~IFF_UP;
	glue_ioctl_flags(ifp);
	ifp->if_flags |= IFF_UP;
	error = glue_ioctl_flags(ifp);
	return error;
}

int
pd80211_state(struct ieee80211com *ic)
{
	switch (ic->ic_state) {
	case IEEE80211_S_SCAN:
		return APPLE80211_S_SCAN;
	case IEEE80211_S_AUTH:
		return APPLE80211_S_AUTH;
	case IEEE80211_S_ASSOC:
		return APPLE80211_S_ASSOC;
	case IEEE80211_S_RUN:
		return APPLE80211_S_RUN;
	default:
		return APPLE80211_S_INIT;
	}
}

int
pd80211_ssid(struct ieee80211com *ic, struct apple80211_ssid_data *out)
{
	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	pd80211_lock();
	if (ic->ic_state == IEEE80211_S_RUN && ic->ic_bss) {
		out->ssid_len = ic->ic_bss->ni_esslen;
		memcpy(out->ssid_bytes, ic->ic_bss->ni_essid, out->ssid_len);
	} else {
		out->ssid_len = ic->ic_des_esslen;
		memcpy(out->ssid_bytes, ic->ic_des_essid, out->ssid_len);
	}
	pd80211_unlock();
	return out->ssid_len ? 0 : ENXIO;
}

int
pd80211_bssid(struct ieee80211com *ic, struct apple80211_bssid_data *out)
{
	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	pd80211_lock();
	if (ic->ic_state != IEEE80211_S_RUN || ic->ic_bss == NULL) {
		pd80211_unlock();
		return ENXIO;
	}
	IEEE80211_ADDR_COPY(out->bssid, ic->ic_bss->ni_bssid);
	pd80211_unlock();
	return 0;
}

static void
glue_channel(struct ieee80211com *ic, struct ieee80211_channel *c, struct apple80211_channel *out)
{
	out->version = APPLE80211_VERSION;
	out->channel = ieee80211_chan2ieee(ic, c);
	out->flags = 0;
	if (c != IEEE80211_CHAN_ANYC) {
		out->flags |= IEEE80211_IS_CHAN_2GHZ(c) ? APPLE80211_C_FLAG_2GHZ : APPLE80211_C_FLAG_5GHZ;
		if (c->ic_flags & IEEE80211_CHAN_40MHZ)
			out->flags |= APPLE80211_C_FLAG_40MHZ;
	}
}

int
pd80211_channel(struct ieee80211com *ic, struct apple80211_channel *out)
{
	memset(out, 0, sizeof(*out));
	pd80211_lock();
	glue_channel(ic, ic->ic_bss && ic->ic_state == IEEE80211_S_RUN ? ic->ic_bss->ni_chan : ic->ic_ibss_chan, out);
	pd80211_unlock();
	return 0;
}

// dBm from what the driver reports, relative to ic_max_rssi when it has one
static int
glue_rssi(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	uint8_t rssi = (*ic->ic_node_getrssi)(ic, ni);

	if (ic->ic_max_rssi)
		return -95 + (int)rssi * 65 / ic->ic_max_rssi;
	return rssi < 128 ? -(int)rssi : (int)(int8_t)rssi;
}

int
pd80211_rssi(struct ieee80211com *ic, struct apple80211_rssi_data *out)
{
	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	out->noise = -95;
	pd80211_lock();
	if (ic->ic_state != IEEE80211_S_RUN || ic->ic_bss == NULL) {
		pd80211_unlock();
		return ENXIO;
	}
	out->rssi = glue_rssi(ic, ic->ic_bss);
	pd80211_unlock();
	return 0;
}

static uint32_t
glue_modes(struct ieee80211com *ic)
{
	uint32_t modes = 0;

	if (ic->ic_modecaps & (1 << IEEE80211_MODE_11A))
		modes |= APPLE80211_MODE_11A;
	if (ic->ic_modecaps & (1 << IEEE80211_MODE_11B))
		modes |= APPLE80211_MODE_11B;
	if (ic->ic_modecaps & (1 << IEEE80211_MODE_11G))
		modes |= APPLE80211_MODE_11G;
	if (ic->ic_modecaps & (1 << IEEE80211_MODE_11N))
		modes |= APPLE80211_MODE_11N;
	if (ic->ic_modecaps & (1 << IEEE80211_MODE_11AC))
		modes |= APPLE80211_MODE_11AC;
	return modes;
}

int
pd80211_phymode(struct ieee80211com *ic, struct apple80211_phymode_data *out)
{
	static const uint32_t current[] = {
		[IEEE80211_MODE_11A] = APPLE80211_MODE_11A, [IEEE80211_MODE_11B] = APPLE80211_MODE_11B,
		[IEEE80211_MODE_11G] = APPLE80211_MODE_11G, [IEEE80211_MODE_11N] = APPLE80211_MODE_11N,
		[IEEE80211_MODE_11AC] = APPLE80211_MODE_11AC,
	};

	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	out->phy_mode = glue_modes(ic);
	if (ic->ic_curmode < nitems(current))
		out->active_phy_mode = current[ic->ic_curmode];
	return 0;
}

// limit the radio to one band, or both: 11G is 2.4 GHz only, 11A 5 GHz only, auto both
int
pd80211_set_band(struct ieee80211com *ic, uint32_t phy_mode)
{
	enum ieee80211_phymode mode = IEEE80211_MODE_AUTO;
	int error;

	if (phy_mode & APPLE80211_MODE_11A)
		mode = IEEE80211_MODE_11A;
	else if (phy_mode & (APPLE80211_MODE_11G | APPLE80211_MODE_11B))
		mode = IEEE80211_MODE_11G;
	pd80211_lock();
	error = ieee80211_setmode(ic, mode);
	if (error == 0)
		error = glue_reset(ic);
	pd80211_unlock();
	return error;
}

int
pd80211_capabilities(struct ieee80211com *ic, struct apple80211_capability_data *out)
{
	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	out->phy_modes = glue_modes(ic);
	out->auth_types = APPLE80211_AUTHTYPE_NONE;
	if (ic->ic_caps & IEEE80211_C_RSN)
		out->auth_types |= APPLE80211_AUTHTYPE_WPA_PSK | APPLE80211_AUTHTYPE_WPA2_PSK;
	return 0;
}

// a stopped radio is started, which scans by itself, a running one rescans when not joined
int
pd80211_scan(struct ieee80211com *ic, const struct apple80211_scan_data *req)
{
	struct ifnet *ifp = &ic->ic_if;
	int error = 0;

	pd80211_lock();
	if ((ifp->if_flags & IFF_RUNNING) == 0) {
		ifp->if_flags |= IFF_UP;
		error = glue_ioctl_flags(ifp);
	} else if (ic->ic_state != IEEE80211_S_RUN) {
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
	} else {
		ieee80211_begin_bgscan(ifp);
	}
	pd80211_unlock();
	return error;
}

static uint32_t
glue_auth_types(struct ieee80211_node *ni)
{
	uint32_t types = 0;

	if (ni->ni_supported_rsnprotos & IEEE80211_PROTO_RSN)
		types |= APPLE80211_AUTHTYPE_WPA2_PSK;
	if (ni->ni_supported_rsnprotos & IEEE80211_PROTO_WPA)
		types |= APPLE80211_AUTHTYPE_WPA_PSK;
	if (types == 0 && (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY))
		types = APPLE80211_AUTHTYPE_WEP;
	return types;
}

// the networks seen so far, ENOENT past the last one
int
pd80211_scan_result(struct ieee80211com *ic, int index, struct apple80211_scan_result *out)
{
	struct ieee80211_node *ni;
	int i = 0;

	memset(out, 0, sizeof(*out));
	out->version = APPLE80211_VERSION;
	pd80211_lock();
	RBT_FOREACH(ni, ieee80211_tree, &ic->ic_tree) {
		if (ni->ni_esslen == 0 && !(ni->ni_capinfo & IEEE80211_CAPINFO_ESS))
			continue;
		if (i++ != index)
			continue;
		glue_channel(ic, ni->ni_chan, &out->asr_channel);
		out->asr_rssi = glue_rssi(ic, ni);
		out->asr_noise = -95;
		out->asr_beacon_int = ni->ni_intval;
		out->asr_cap = ni->ni_capinfo;
		IEEE80211_ADDR_COPY(out->asr_bssid, ni->ni_bssid);
		out->asr_nrates = MIN(ni->ni_rates.rs_nrates, APPLE80211_MAX_RATES);
		for (int r = 0; r < out->asr_nrates; r++)
			out->asr_rates[r] = (ni->ni_rates.rs_rates[r] & IEEE80211_RATE_VAL) * 500;
		out->asr_ssid_len = MIN(ni->ni_esslen, APPLE80211_MAX_SSID_LEN);
		memcpy(out->asr_ssid, ni->ni_essid, out->asr_ssid_len);
		out->asr_auth_types = glue_auth_types(ni);
		pd80211_unlock();
		return 0;
	}
	pd80211_unlock();
	return ENOENT;
}

// the desired network, then wpa2 with the pmk or open, as SIOCS80211NWID and WPAPSK set them
int
pd80211_associate(struct ieee80211com *ic, const struct apple80211_assoc_data *req)
{
	int error;

	if (req->ad_ssid_len > IEEE80211_NWID_LEN)
		return EINVAL;
	pd80211_lock();
	memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
	ic->ic_des_esslen = req->ad_ssid_len;
	memcpy(ic->ic_des_essid, req->ad_ssid, req->ad_ssid_len);
	ic->ic_flags &= ~IEEE80211_F_AUTO_JOIN;
	ieee80211_disable_rsn(ic);
	ieee80211_disable_wep(ic);

	if (req->ad_auth_type & (APPLE80211_AUTHTYPE_WPA_PSK | APPLE80211_AUTHTYPE_WPA2_PSK)) {
		if (!(ic->ic_caps & IEEE80211_C_RSN) || req->ad_key_len != IEEE80211_PMK_LEN) {
			pd80211_unlock();
			return EINVAL;
		}
		ic->ic_rsnprotos = 0;
		if (req->ad_auth_type & APPLE80211_AUTHTYPE_WPA2_PSK)
			ic->ic_rsnprotos |= IEEE80211_PROTO_RSN;
		if (req->ad_auth_type & APPLE80211_AUTHTYPE_WPA_PSK)
			ic->ic_rsnprotos |= IEEE80211_PROTO_WPA;
		ic->ic_rsnakms = IEEE80211_AKM_PSK;
		ic->ic_rsngroupcipher = (ic->ic_rsnprotos & IEEE80211_PROTO_WPA) ? IEEE80211_CIPHER_TKIP : IEEE80211_CIPHER_CCMP;
		ic->ic_rsnciphers = IEEE80211_CIPHER_CCMP;
		if (ic->ic_rsnprotos & IEEE80211_PROTO_WPA)
			ic->ic_rsnciphers |= IEEE80211_CIPHER_TKIP;
		ic->ic_flags |= IEEE80211_F_RSNON | IEEE80211_F_PSK;
		memcpy(ic->ic_psk, req->ad_key, IEEE80211_PMK_LEN);
	}
	error = glue_reset(ic);
	pd80211_unlock();
	return error;
}

int
pd80211_disassociate(struct ieee80211com *ic)
{
	int error;

	pd80211_lock();
	memset(ic->ic_des_essid, 0, IEEE80211_NWID_LEN);
	ic->ic_des_esslen = 0;
	ieee80211_disable_rsn(ic);
	explicit_bzero(ic->ic_psk, sizeof(ic->ic_psk));
	ic->ic_flags &= ~IEEE80211_F_PSK;
	error = glue_reset(ic);
	pd80211_unlock();
	return error;
}
