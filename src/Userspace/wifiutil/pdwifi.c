/*
 * pdwifi.c - Wi-Fi for the command line and the desktop, see pdwifi.h.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <time.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include "apple80211_ioctl.h"
#include "pdwifi.h"

#define STATE_ASSOCIATING 3
#define STATE_RUNNING 4
// left by a disconnect, so autojoin waits for the next join or restart
#define LEFT_PATH "/var/run/org.puredarwin.wifi.left"
// held while a join runs, a scan from any process would retune the radio under it
#define JOINING_PATH "/var/run/org.puredarwin.wifi.joining"

static char ifname[16];

static int request(int set, int type, int value, void *data, uint32_t len)
{
	struct apple80211req req;
	int s = socket(AF_INET, SOCK_DGRAM, 0), rc;

	if (s < 0)
		return -1;
	memset(&req, 0, sizeof(req));
	strlcpy(req.req_if_name, ifname, sizeof(req.req_if_name));
	req.req_type = type;
	req.req_val = value;
	req.req_len = len;
	req.req_data = data;
	rc = ioctl(s, set ? SIOCSA80211 : SIOCGA80211, &req);
	close(s);
	return rc;
}

void pdwifi_set_interface(const char *name)
{
	strlcpy(ifname, name, sizeof(ifname));
}

const char *pdwifi_interface(void)
{
	struct apple80211_state_data st;

	if (ifname[0] && request(0, APPLE80211_IOC_STATE, 0, &st, sizeof(st)) == 0)
		return ifname;
	for (int i = 0; i < 8; i++) {
		snprintf(ifname, sizeof(ifname), "en%d", i);
		if (request(0, APPLE80211_IOC_STATE, 0, &st, sizeof(st)) == 0)
			return ifname;
	}
	ifname[0] = 0;
	return NULL;
}

int pdwifi_power(void)
{
	struct apple80211_power_data p = { APPLE80211_VERSION };

	if (!pdwifi_interface() || request(0, APPLE80211_IOC_POWER, 0, &p, sizeof(p)) < 0)
		return 0;
	return p.power_state != 0;
}

pdwifi_result pdwifi_set_power(int on)
{
	struct apple80211_power_data p = { APPLE80211_VERSION, on != 0 };

	if (!pdwifi_interface())
		return PDWIFI_NO_HARDWARE;
	if (request(1, APPLE80211_IOC_POWER, 0, &p, sizeof(p)) < 0)
		return PDWIFI_FAILED;
	return PDWIFI_OK;
}

static const char *security(uint32_t auth)
{
	if (auth & APPLE80211_AUTHTYPE_WPA2_PSK)
		return "WPA2";
	if (auth & APPLE80211_AUTHTYPE_WPA_PSK)
		return "WPA";
	if (auth & APPLE80211_AUTHTYPE_WEP)
		return "WEP";
	return NULL;
}

// a join marker older than a join can last was left by a process that died
static int joining(void)
{
	struct stat st;

	return stat(JOINING_PATH, &st) == 0 && time(NULL) - st.st_mtime < 60;
}

static int by_rssi(const void *a, const void *b)
{
	return ((const pdwifi_network *)b)->rssi - ((const pdwifi_network *)a)->rssi;
}

int pdwifi_scan(pdwifi_network *out, int max)
{
	struct apple80211_scan_data req = { APPLE80211_VERSION };
	struct apple80211_scan_result r;
	int n = 0;

	if (!pdwifi_interface() || !pdwifi_power())
		return 0;
	// a scan in the middle of a join would retune the radio away from the access point
	if (joining() || pdwifi_state() == 2 || pdwifi_state() == STATE_ASSOCIATING)
		return 0;
	if (request(1, APPLE80211_IOC_SCAN_REQ, 0, &req, sizeof(req)) < 0)
		return 0;
	// the radio hops every channel, give it a few seconds
	sleep(4);
	for (int i = 0; request(0, APPLE80211_IOC_SCAN_RESULT, i, &r, sizeof(r)) == 0; i++) {
		pdwifi_network *net = NULL;
		char ssid[33] = { 0 };

		memcpy(ssid, r.asr_ssid, r.asr_ssid_len > 32 ? 32 : r.asr_ssid_len);
		// hidden networks are joined by name through Other Network
		if (ssid[0] == 0)
			continue;
		for (int k = 0; k < n; k++) {
			if (strcmp(out[k].ssid, ssid) == 0)
				net = &out[k];
		}
		if (net != NULL && net->rssi >= r.asr_rssi)
			continue;
		if (net == NULL) {
			if (n == max)
				continue;
			net = &out[n++];
		}
		memset(net, 0, sizeof(*net));
		strlcpy(net->ssid, ssid, sizeof(net->ssid));
		memcpy(net->bssid, r.asr_bssid, 6);
		net->rssi = r.asr_rssi;
		net->channel = r.asr_channel.channel;
		net->security = security(r.asr_auth_types);
		net->secure = net->security != NULL;
	}
	qsort(out, n, sizeof(*out), by_rssi);
	return n;
}

int pdwifi_state(void)
{
	struct apple80211_state_data st;

	if (!pdwifi_interface() || request(0, APPLE80211_IOC_STATE, 0, &st, sizeof(st)) < 0)
		return -1;
	return (int)st.state;
}

int pdwifi_current(pdwifi_network *out)
{
	struct apple80211_ssid_data ssid;
	struct apple80211_bssid_data bssid;
	struct apple80211_rssi_data rssi;
	struct apple80211_channel ch;

	memset(out, 0, sizeof(*out));
	if (pdwifi_state() != STATE_RUNNING)
		return 0;
	if (request(0, APPLE80211_IOC_SSID, 0, &ssid, sizeof(ssid)) == 0)
		snprintf(out->ssid, sizeof(out->ssid), "%.*s", (int)(ssid.ssid_len > 32 ? 32 : ssid.ssid_len), ssid.ssid_bytes);
	if (request(0, APPLE80211_IOC_BSSID, 0, &bssid, sizeof(bssid)) == 0)
		memcpy(out->bssid, bssid.bssid, 6);
	if (request(0, APPLE80211_IOC_RSSI, 0, &rssi, sizeof(rssi)) == 0)
		out->rssi = rssi.rssi;
	if (request(0, APPLE80211_IOC_CHANNEL, 0, &ch, sizeof(ch)) == 0)
		out->channel = ch.channel;
	// the link does not report its cipher, a remembered network is a secured one
	out->secure = pdwifi_is_known(out->ssid) == 2;
	out->security = out->secure ? "WPA2" : NULL;
	return out->ssid[0] != 0;
}

int pdwifi_hwaddr(uint8_t mac[6])
{
	struct apple80211_hwaddr_data hw;

	if (!pdwifi_interface() || request(0, APPLE80211_IOC_HW_ADDR, 0, &hw, sizeof(hw)) < 0)
		return 0;
	memcpy(mac, hw.addr, 6);
	return 1;
}

// the remembered networks: "ssid<TAB>pmk" lines, pmk empty for an open network
typedef struct {
	char ssid[33];
	char pmk[65];
} known_entry;

static int load_known(known_entry *e, int max)
{
	FILE *f = fopen(PDWIFI_KNOWN_PATH, "r");
	char line[160];
	int n = 0;

	if (f == NULL)
		return 0;
	while (n < max && fgets(line, sizeof(line), f) != NULL) {
		char *tab = strchr(line, '\t');

		line[strcspn(line, "\n")] = 0;
		if (tab == NULL)
			continue;
		*tab = 0;
		strlcpy(e[n].ssid, line, sizeof(e[n].ssid));
		strlcpy(e[n].pmk, tab + 1, sizeof(e[n].pmk));
		n++;
	}
	fclose(f);
	return n;
}

static void save_known(const known_entry *e, int n)
{
	char tmp[] = PDWIFI_KNOWN_PATH ".new";
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	FILE *f;

	if (fd < 0)
		return;
	f = fdopen(fd, "w");
	for (int i = 0; i < n; i++)
		fprintf(f, "%s\t%s\n", e[i].ssid, e[i].pmk);
	fclose(f);
	rename(tmp, PDWIFI_KNOWN_PATH);
}

static void remember(const char *ssid, const uint8_t *pmk)
{
	known_entry e[PDWIFI_MAX_NETWORKS];
	int n = load_known(e, PDWIFI_MAX_NETWORKS), at = n;

	for (int i = 0; i < n; i++) {
		if (strcmp(e[i].ssid, ssid) == 0)
			at = i;
	}
	// the most recently joined goes first, so autojoin prefers it among equals
	if (at == n && n == PDWIFI_MAX_NETWORKS)
		at = --n;
	memmove(&e[1], &e[0], sizeof(e[0]) * at);
	if (at == n)
		n++;
	strlcpy(e[0].ssid, ssid, sizeof(e[0].ssid));
	e[0].pmk[0] = 0;
	for (int i = 0; pmk != NULL && i < 32; i++)
		snprintf(&e[0].pmk[i * 2], 3, "%02x", pmk[i]);
	save_known(e, n);
}

int pdwifi_known(char names[][33], int max)
{
	known_entry e[PDWIFI_MAX_NETWORKS];
	int n = load_known(e, PDWIFI_MAX_NETWORKS);

	for (int i = 0; i < n && i < max; i++)
		strlcpy(names[i], e[i].ssid, 33);
	return n < max ? n : max;
}

// 0 not known, 1 known and open, 2 known with a key
int pdwifi_is_known(const char *ssid)
{
	known_entry e[PDWIFI_MAX_NETWORKS];
	int n = load_known(e, PDWIFI_MAX_NETWORKS);

	for (int i = 0; i < n; i++) {
		if (strcmp(e[i].ssid, ssid) == 0)
			return e[i].pmk[0] ? 2 : 1;
	}
	return 0;
}

void pdwifi_forget(const char *ssid)
{
	known_entry e[PDWIFI_MAX_NETWORKS];
	int n = load_known(e, PDWIFI_MAX_NETWORKS), w = 0;

	for (int i = 0; i < n; i++) {
		if (strcmp(e[i].ssid, ssid) != 0)
			e[w++] = e[i];
	}
	save_known(e, w);
}

// associates with the key given, then waits for the link to run
static pdwifi_result associate(const char *ssid, const uint8_t *pmk)
{
	struct apple80211_assoc_data a;
	int state = -1, furthest = -1;

	memset(&a, 0, sizeof(a));
	a.version = APPLE80211_VERSION;
	a.ad_ssid_len = (uint32_t)strlen(ssid);
	memcpy(a.ad_ssid, ssid, a.ad_ssid_len);
	if (pmk != NULL) {
		memcpy(a.ad_key, pmk, 32);
		a.ad_key_len = 32;
		a.ad_auth_type = APPLE80211_AUTHTYPE_WPA2_PSK;
	}
	close(open(JOINING_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0644));
	if (request(1, APPLE80211_IOC_ASSOCIATE, 0, &a, sizeof(a)) < 0) {
		unlink(JOINING_PATH);
		return PDWIFI_FAILED;
	}
	// the driver scans every channel before it authenticates, about 20 seconds on a USB radio
	for (int i = 0; i < 180; i++) {
		usleep(250000);
		state = pdwifi_state();
		if (state == STATE_RUNNING) {
			unlink(JOINING_PATH);
			return PDWIFI_OK;
		}
		if (state > furthest)
			furthest = state;
		// associated and then dropped back to scanning is the access point refusing the key
		if (pmk != NULL && furthest >= STATE_ASSOCIATING && state <= 1)
			break;
	}
	request(1, APPLE80211_IOC_DISASSOCIATE, 0, NULL, 0);
	unlink(JOINING_PATH);
	if (pmk != NULL && furthest >= STATE_ASSOCIATING && state <= 1)
		return PDWIFI_BAD_PASSWORD;
	return furthest >= 2 ? PDWIFI_TIMEOUT : PDWIFI_NOT_FOUND;
}

static int parse_pmk(const char *hex, uint8_t *pmk)
{
	if (strlen(hex) != 64)
		return 0;
	for (int i = 0; i < 32; i++) {
		unsigned v;

		if (sscanf(hex + i * 2, "%2x", &v) != 1)
			return 0;
		pmk[i] = (uint8_t)v;
	}
	return 1;
}

// wpa2 takes the pmk, pbkdf2-hmac-sha1 of the passphrase salted with the ssid, 4096 rounds
pdwifi_result pdwifi_join(const char *ssid, const char *password, int keep)
{
	uint8_t pmk[32];
	size_t slen = strlen(ssid);
	pdwifi_result r;

	if (!pdwifi_interface())
		return PDWIFI_NO_HARDWARE;
	if (!pdwifi_power())
		return PDWIFI_OFF;
	if (slen == 0 || slen > APPLE80211_MAX_SSID_LEN)
		return PDWIFI_FAILED;
	if (password != NULL) {
		size_t len = strlen(password);

		if (len < 8 || len > 63)
			return PDWIFI_BAD_PASSWORD;
		if (CCKeyDerivationPBKDF(kCCPBKDF2, password, len, (const uint8_t *)ssid, slen, kCCPRFHmacAlgSHA1,
		        4096, pmk, sizeof(pmk)) != 0)
			return PDWIFI_FAILED;
	}
	unlink(LEFT_PATH);
	r = associate(ssid, password != NULL ? pmk : NULL);
	if (r == PDWIFI_OK && keep)
		remember(ssid, password != NULL ? pmk : NULL);
	return r;
}

pdwifi_result pdwifi_join_known(const char *ssid)
{
	known_entry e[PDWIFI_MAX_NETWORKS];
	int n = load_known(e, PDWIFI_MAX_NETWORKS);
	uint8_t pmk[32];

	if (!pdwifi_interface())
		return PDWIFI_NO_HARDWARE;
	if (!pdwifi_power())
		return PDWIFI_OFF;
	for (int i = 0; i < n; i++) {
		pdwifi_result r;

		if (strcmp(e[i].ssid, ssid) != 0)
			continue;
		if (e[i].pmk[0] && !parse_pmk(e[i].pmk, pmk))
			return PDWIFI_FAILED;
		unlink(LEFT_PATH);
		r = associate(ssid, e[i].pmk[0] ? pmk : NULL);
		if (r == PDWIFI_OK)
			remember(ssid, e[i].pmk[0] ? pmk : NULL);
		return r;
	}
	return PDWIFI_FAILED;
}

pdwifi_result pdwifi_leave(void)
{
	if (!pdwifi_interface())
		return PDWIFI_NO_HARDWARE;
	close(open(LEFT_PATH, O_WRONLY | O_CREAT, 0644));
	return request(1, APPLE80211_IOC_DISASSOCIATE, 0, NULL, 0) < 0 ? PDWIFI_FAILED : PDWIFI_OK;
}

// joins the strongest remembered network in range, unless already on one
pdwifi_result pdwifi_autojoin(void)
{
	pdwifi_network nets[PDWIFI_MAX_NETWORKS], cur;
	int n;

	if (!pdwifi_interface())
		return PDWIFI_NO_HARDWARE;
	if (!pdwifi_power())
		return PDWIFI_OFF;
	if (pdwifi_current(&cur) || access(LEFT_PATH, F_OK) == 0 || joining())
		return PDWIFI_OK;
	n = pdwifi_scan(nets, PDWIFI_MAX_NETWORKS);
	for (int i = 0; i < n; i++) {
		if (pdwifi_is_known(nets[i].ssid) && pdwifi_join_known(nets[i].ssid) == PDWIFI_OK)
			return PDWIFI_OK;
	}
	return PDWIFI_FAILED;
}

int pdwifi_bars(int rssi)
{
	if (rssi >= -60)
		return 3;
	if (rssi >= -72)
		return 2;
	return 1;
}

const char *pdwifi_message(pdwifi_result r)
{
	switch (r) {
	case PDWIFI_OK: return "Connected.";
	case PDWIFI_NO_HARDWARE: return "No Wi-Fi hardware was found.";
	case PDWIFI_OFF: return "Wi-Fi is turned off.";
	case PDWIFI_BAD_PASSWORD: return "Couldn't join. Check the password and try again.";
	case PDWIFI_TIMEOUT: return "The network didn't accept the connection.";
	case PDWIFI_NOT_FOUND: return "The network wasn't found. Move closer and try again.";
	default: return "The network couldn't be joined.";
	}
}
