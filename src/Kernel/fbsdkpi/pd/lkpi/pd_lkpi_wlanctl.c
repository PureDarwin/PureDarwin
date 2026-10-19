// SPDX-License-Identifier: MIT
// station control for the iokit side, through net80211's own ioctls the way
// wpa_supplicant and ifconfig drive it on FreeBSD

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_ioctl.h>

#include <string.h>

#include "../pd_wlan.h"

int pd_wpa_configure(struct ieee80211vap *vap, const uint8_t *pmk,
	const uint8_t *rsnie, int rsnie_len);
void pd_wpa_reset(struct ieee80211vap *vap);
int pd_fbsd_up(void *ifp);
int pdos_bootarg_int(const char *name, int def);

#define PD_SCAN_BUFFER	(24 * 1024)

static int
wlan_ioctl(struct ifnet *ifp, u_long cmd, int type, int val, void *data, int len,
	int *outlen)
{
	struct ieee80211req ireq;

	memset(&ireq, 0, sizeof(ireq));
	strlcpy(ireq.i_name, ifp->if_xname, sizeof(ireq.i_name));
	ireq.i_type = type;
	ireq.i_val = val;
	ireq.i_len = len;
	ireq.i_data = data;
	int error = ifp->if_ioctl(ifp, cmd, (caddr_t)&ireq);
	if (outlen != NULL)
		*outlen = ireq.i_len;
	return error;
}

static int
wlan_set(struct ifnet *ifp, int type, int val, void *data, int len)
{
	int error = wlan_ioctl(ifp, SIOCS80211, type, val, data, len, NULL);
	if (error != 0)
		if_printf(ifp, "net80211 set %d (value %d) failed: %d\n", type, val, error);
	return error;
}

int
pd_wlan_scan(void *cookie, const uint8_t *ssid, uint32_t ssid_len)
{
	struct ifnet *ifp = cookie;
	struct ieee80211_scan_req sr;

	// bringing the interface up cycles it and drops the scan cache, only do it once
	int error = (ifp->if_flags & IFF_UP) == 0 ? pd_fbsd_up(ifp) : 0;
	if (error != 0)
		return error;

	memset(&sr, 0, sizeof(sr));
	sr.sr_flags = IEEE80211_IOC_SCAN_ACTIVE | IEEE80211_IOC_SCAN_ONCE
		| IEEE80211_IOC_SCAN_NOJOIN | IEEE80211_IOC_SCAN_FLUSH;
	sr.sr_duration = IEEE80211_IOC_SCAN_FOREVER;
	if (ssid_len > 0 && ssid_len <= IEEE80211_NWID_LEN) {
		sr.sr_nssid = 1;
		sr.sr_ssid[0].len = ssid_len;
		memcpy(sr.sr_ssid[0].ssid, ssid, ssid_len);
	}

	// a manual roaming station only scans and joins when told to, and a scan
	// still running makes net80211 refuse a new one with EBUSY
	wlan_set(ifp, IEEE80211_IOC_ROAMING, IEEE80211_ROAMING_MANUAL, NULL, 0);
	ieee80211_cancel_scan(ifp->if_softc);
	return wlan_set(ifp, IEEE80211_IOC_SCAN_REQ, 0, &sr, sizeof(sr));
}

int
pd_wlan_scan_results(void *cookie, struct pd_wlan_bss *out, int max)
{
	struct ifnet *ifp = cookie;
	int len = 0, count = 0;

	uint8_t *buffer = malloc(PD_SCAN_BUFFER, M_DEVBUF, M_WAITOK | M_ZERO);
	if (buffer == NULL)
		return -ENOMEM;

	int error = wlan_ioctl(ifp, SIOCG80211, IEEE80211_IOC_SCAN_RESULTS, 0, buffer,
		PD_SCAN_BUFFER, &len);
	if (error != 0) {
		free(buffer, M_DEVBUF);
		return -error;
	}

	for (int offset = 0; offset + (int)sizeof(struct ieee80211req_scan_result) <= len
			&& count < max;) {
		struct ieee80211req_scan_result *sr
			= (struct ieee80211req_scan_result *)(buffer + offset);
		if (sr->isr_len == 0)
			break;

		struct pd_wlan_bss *bss = &out[count++];
		memset(bss, 0, sizeof(*bss));
		memcpy(bss->bssid, sr->isr_bssid, 6);
		bss->freq = sr->isr_freq;
		bss->chan_flags = sr->isr_flags;
		bss->channel = ieee80211_mhz2ieee(sr->isr_freq, sr->isr_flags);
		bss->rssi = sr->isr_rssi;
		bss->noise = sr->isr_noise;
		bss->capinfo = sr->isr_capinfo;
		bss->intval = sr->isr_intval;

		const uint8_t *ies = (const uint8_t *)sr + sr->isr_ie_off;
		bss->ssid_len = MIN(sr->isr_ssid_len, PD_WLAN_MAX_SSID);
		memcpy(bss->ssid, ies, bss->ssid_len);

		// the ie data follows the ssid and the mesh id
		const uint8_t *ie = ies + sr->isr_ssid_len + sr->isr_meshid_len;
		const uint8_t *end = ie + sr->isr_ie_len;
		while (ie + 2 <= end && ie + 2 + ie[1] <= end) {
			if (ie[0] == IEEE80211_ELEMID_RSN)
				bss->has_rsn = 1;
			else if (ie[0] == IEEE80211_ELEMID_VENDOR && ie[1] >= 4
				&& ie[2] == 0x00 && ie[3] == 0x50 && ie[4] == 0xf2 && ie[5] == 0x01)
				bss->has_wpa = 1;
			ie += 2 + ie[1];
		}

		offset += sr->isr_len;
	}

	free(buffer, M_DEVBUF);
	return count;
}

// the rsn ie this station sends: ccmp pairwise, psk, the ap's group cipher
static int
build_rsn_ie(uint8_t *ie, uint8_t group_suite)
{
	static const uint8_t oui[3] = { 0x00, 0x0f, 0xac };
	int n = 0;

	ie[n++] = IEEE80211_ELEMID_RSN;
	ie[n++] = 0;
	ie[n++] = 1;
	ie[n++] = 0;
	memcpy(&ie[n], oui, 3); n += 3; ie[n++] = group_suite;
	ie[n++] = 1; ie[n++] = 0;
	memcpy(&ie[n], oui, 3); n += 3; ie[n++] = 4;
	ie[n++] = 1; ie[n++] = 0;
	memcpy(&ie[n], oui, 3); n += 3; ie[n++] = 2;
	ie[n++] = 0; ie[n++] = 0;
	ie[1] = n - 2;
	return n;
}

struct group_lookup {
	const uint8_t *ssid;
	uint32_t ssid_len;
	const uint8_t *bssid;
	uint8_t group_suite;
	// the strongest matching ap, MLME ASSOC needs its bssid
	bool found;
	int rssi;
	uint8_t best[IEEE80211_ADDR_LEN];
};

static void
find_group_cipher(void *arg, const struct ieee80211_scan_entry *se)
{
	struct group_lookup *lookup = arg;
	const uint8_t *rsn = se->se_ies.rsn_ie;

	if (se->se_ssid[1] != lookup->ssid_len
		|| memcmp(se->se_ssid + 2, lookup->ssid, lookup->ssid_len) != 0)
		return;
	if (lookup->bssid != NULL && memcmp(se->se_bssid, lookup->bssid, 6) != 0)
		return;
	// pd_wlan_band=2 or 5 restricts joins to one band, for testing
	int band = pdos_bootarg_int("pd_wlan_band", 0);
	if (band == 2 && !IEEE80211_IS_CHAN_2GHZ(se->se_chan))
		return;
	if (band == 5 && !IEEE80211_IS_CHAN_5GHZ(se->se_chan))
		return;
	if (lookup->found && se->se_rssi <= lookup->rssi)
		return;
	lookup->found = true;
	lookup->rssi = se->se_rssi;
	memcpy(lookup->best, se->se_macaddr, IEEE80211_ADDR_LEN);
	// version then the group suite selector
	if (rsn != NULL && rsn[1] >= 6)
		lookup->group_suite = rsn[7];
}

int
pd_wlan_join(void *cookie, const uint8_t *ssid, uint32_t ssid_len,
	const uint8_t *bssid, const uint8_t *pmk)
{
	struct ifnet *ifp = cookie;
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211req_mlme mlme;
	static const uint8_t zero[6];

	if (ssid_len == 0 || ssid_len > IEEE80211_NWID_LEN)
		return EINVAL;
	if (bssid != NULL && memcmp(bssid, zero, 6) == 0)
		bssid = NULL;

	// bringing the interface up cycles it and drops the scan cache, only do it once
	int error = (ifp->if_flags & IFF_UP) == 0 ? pd_fbsd_up(ifp) : 0;
	if (error != 0)
		return error;

	pd_wpa_reset(vap);
	wlan_set(ifp, IEEE80211_IOC_ROAMING, IEEE80211_ROAMING_MANUAL, NULL, 0);

	struct group_lookup lookup = { ssid, ssid_len, bssid, 4 };
	ieee80211_scan_iterate(vap, find_group_cipher, &lookup);
	if (!lookup.found) {
		if_printf(ifp, "join: no scanned ap carries that ssid\n");
		return ENOENT;
	}

	if (pmk != NULL) {
		uint8_t ie[32];

		int ielen = build_rsn_ie(ie, lookup.group_suite);
		error = pd_wpa_configure(vap, pmk, ie, ielen);
		if (error != 0)
			return error;

		wlan_set(ifp, IEEE80211_IOC_WPA, 2, NULL, 0);
		wlan_set(ifp, IEEE80211_IOC_PRIVACY, 1, NULL, 0);
		wlan_set(ifp, IEEE80211_IOC_DROPUNENCRYPTED, 1, NULL, 0);
		error = wlan_set(ifp, IEEE80211_IOC_APPIE, IEEE80211_APPIE_WPA, ie, ielen);
		if (error != 0)
			return error;
	} else {
		wlan_set(ifp, IEEE80211_IOC_WPA, 0, NULL, 0);
		wlan_set(ifp, IEEE80211_IOC_PRIVACY, 0, NULL, 0);
		wlan_set(ifp, IEEE80211_IOC_DROPUNENCRYPTED, 0, NULL, 0);
	}

	// an unfinished scan would hold the radio off the ap's channel
	ieee80211_cancel_scan(vap);
	pause("pdjoin", hz / 5);

	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_ASSOC;
	mlme.im_ssid_len = ssid_len;
	memcpy(mlme.im_ssid, ssid, ssid_len);
	memcpy(mlme.im_macaddr, lookup.best, 6);
	if_printf(ifp, "join: %02x:%02x:%02x:%02x:%02x:%02x, rssi %d\n", lookup.best[0],
		lookup.best[1], lookup.best[2], lookup.best[3], lookup.best[4], lookup.best[5],
		lookup.rssi);
	return wlan_set(ifp, IEEE80211_IOC_MLME, 0, &mlme, sizeof(mlme));
}

int
pd_wlan_leave(void *cookie)
{
	struct ifnet *ifp = cookie;
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211req_mlme mlme;

	pd_wpa_reset(vap);
	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_DISASSOC;
	mlme.im_reason = IEEE80211_REASON_ASSOC_LEAVE;
	if (vap->iv_bss != NULL)
		memcpy(mlme.im_macaddr, vap->iv_bss->ni_bssid, 6);
	return wlan_set(ifp, IEEE80211_IOC_MLME, 0, &mlme, sizeof(mlme));
}

int
pd_wlan_get_status(void *cookie, struct pd_wlan_status *status)
{
	struct ifnet *ifp = cookie;
	struct ieee80211vap *vap = ifp->if_softc;
	struct ieee80211com *ic = vap->iv_ic;
	struct ieee80211_node *ni;

	memset(status, 0, sizeof(*status));
	status->state = vap->iv_state;

	ni = ieee80211_ref_node(vap->iv_bss);
	if (ni == NULL)
		return 0;
	if (vap->iv_state == IEEE80211_S_RUN) {
		status->ssid_len = MIN(ni->ni_esslen, PD_WLAN_MAX_SSID);
		memcpy(status->ssid, ni->ni_essid, status->ssid_len);
		memcpy(status->bssid, ni->ni_bssid, 6);
		if (ni->ni_chan != NULL && ni->ni_chan != IEEE80211_CHAN_ANYC) {
			status->channel = ni->ni_chan->ic_ieee;
			status->chan_flags = ni->ni_chan->ic_flags;
		}
		int8_t rssi = 0, noise = 0;
		ic->ic_node_getsignal(ni, &rssi, &noise);
		status->rssi = rssi;
		status->noise = noise;
		status->authorized = (ni->ni_flags & IEEE80211_NODE_AUTH) != 0;
	}
	ieee80211_free_node(ni);
	return 0;
}
