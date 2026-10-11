/*
 * wifiutil - scan for, join and leave 802.11 networks, and show the link,
 * through the interface's SIOCGA80211 and SIOCSA80211 requests.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <CommonCrypto/CommonKeyDerivation.h>
#include "apple80211_ioctl.h"

static const char *ifname = "en1";
static int sock = -1;

static int request(int set, int type, int value, void *data, uint32_t len)
{
	struct apple80211req req;

	memset(&req, 0, sizeof(req));
	strlcpy(req.req_if_name, ifname, sizeof(req.req_if_name));
	req.req_type = type;
	req.req_val = value;
	req.req_len = len;
	req.req_data = data;
	return ioctl(sock, set ? SIOCSA80211 : SIOCGA80211, &req);
}

static const char *security(uint32_t auth)
{
	if (auth & APPLE80211_AUTHTYPE_WPA2_PSK)
		return "WPA2";
	if (auth & APPLE80211_AUTHTYPE_WPA_PSK)
		return "WPA";
	if (auth & APPLE80211_AUTHTYPE_WEP)
		return "WEP";
	return "open";
}

static int scan(void)
{
	struct apple80211_scan_data req = { APPLE80211_VERSION };
	struct apple80211_scan_result r;

	if (request(1, APPLE80211_IOC_SCAN_REQ, 0, &req, sizeof(req)) < 0) {
		perror("scan");
		return 1;
	}
	// the radio hops every channel, give it a few seconds
	sleep(4);
	printf("%-32s %-17s %4s %4s  %s\n", "SSID", "BSSID", "RSSI", "CH", "SECURITY");
	for (int i = 0; request(0, APPLE80211_IOC_SCAN_RESULT, i, &r, sizeof(r)) == 0; i++) {
		char ssid[APPLE80211_MAX_SSID_LEN + 1] = { 0 };

		memcpy(ssid, r.asr_ssid, r.asr_ssid_len);
		printf("%-32s %02x:%02x:%02x:%02x:%02x:%02x %4d %4u  %s\n", ssid[0] ? ssid : "(hidden)",
		    r.asr_bssid[0], r.asr_bssid[1], r.asr_bssid[2], r.asr_bssid[3], r.asr_bssid[4], r.asr_bssid[5],
		    r.asr_rssi, r.asr_channel.channel, security(r.asr_auth_types));
	}
	return 0;
}

static int info(void)
{
	static const char *states[] = { "init", "scanning", "authenticating", "associating", "running" };
	struct apple80211_state_data st;
	struct apple80211_ssid_data ssid;
	struct apple80211_bssid_data bssid;
	struct apple80211_rssi_data rssi;
	struct apple80211_channel ch;
	struct apple80211_hwaddr_data hw;

	if (request(0, APPLE80211_IOC_STATE, 0, &st, sizeof(st)) < 0) {
		perror(ifname);
		return 1;
	}
	if (request(0, APPLE80211_IOC_HW_ADDR, 0, &hw, sizeof(hw)) == 0)
		printf("address: %02x:%02x:%02x:%02x:%02x:%02x\n", hw.addr[0], hw.addr[1], hw.addr[2], hw.addr[3], hw.addr[4], hw.addr[5]);
	printf("state:   %s\n", st.state < 5 ? states[st.state] : "?");
	if (request(0, APPLE80211_IOC_SSID, 0, &ssid, sizeof(ssid)) == 0)
		printf("ssid:    %.*s\n", (int)ssid.ssid_len, ssid.ssid_bytes);
	if (request(0, APPLE80211_IOC_BSSID, 0, &bssid, sizeof(bssid)) == 0)
		printf("bssid:   %02x:%02x:%02x:%02x:%02x:%02x\n", bssid.bssid[0], bssid.bssid[1], bssid.bssid[2],
		    bssid.bssid[3], bssid.bssid[4], bssid.bssid[5]);
	if (request(0, APPLE80211_IOC_CHANNEL, 0, &ch, sizeof(ch)) == 0)
		printf("channel: %u\n", ch.channel);
	if (request(0, APPLE80211_IOC_RSSI, 0, &rssi, sizeof(rssi)) == 0)
		printf("rssi:    %d dBm\n", rssi.rssi);
	return 0;
}

// wpa2 takes the pmk, pbkdf2-hmac-sha1 of the passphrase salted with the ssid, 4096 rounds
static int join(const char *name, const char *password)
{
	struct apple80211_assoc_data a;

	memset(&a, 0, sizeof(a));
	a.version = APPLE80211_VERSION;
	a.ad_ssid_len = (uint32_t)strlen(name);
	if (a.ad_ssid_len > APPLE80211_MAX_SSID_LEN) {
		fprintf(stderr, "ssid too long\n");
		return 1;
	}
	memcpy(a.ad_ssid, name, a.ad_ssid_len);
	if (password) {
		size_t len = strlen(password);

		if (len < 8 || len > 63) {
			fprintf(stderr, "a WPA passphrase is 8 to 63 characters\n");
			return 1;
		}
		if (CCKeyDerivationPBKDF(kCCPBKDF2, password, len, a.ad_ssid, a.ad_ssid_len, kCCPRFHmacAlgSHA1,
		        4096, a.ad_key, 32) != 0) {
			fprintf(stderr, "key derivation failed\n");
			return 1;
		}
		a.ad_key_len = 32;
		a.ad_auth_type = APPLE80211_AUTHTYPE_WPA2_PSK;
	}
	if (request(1, APPLE80211_IOC_ASSOCIATE, 0, &a, sizeof(a)) < 0) {
		perror("join");
		return 1;
	}
	return 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: wifiutil [-i interface] scan | info | join ssid [passphrase] | leave | band 2.4|5|any | off | on\n");
	exit(2);
}

int main(int argc, char **argv)
{
	int ch;

	while ((ch = getopt(argc, argv, "i:")) != -1) {
		if (ch == 'i')
			ifname = optarg;
		else
			usage();
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return 1;
	}
	if (strcmp(argv[0], "scan") == 0)
		return scan();
	if (strcmp(argv[0], "info") == 0)
		return info();
	if (strcmp(argv[0], "join") == 0 && argc >= 2)
		return join(argv[1], argc >= 3 ? argv[2] : NULL);
	if (strcmp(argv[0], "leave") == 0)
		return request(1, APPLE80211_IOC_DISASSOCIATE, 0, NULL, 0) < 0;
	if (strcmp(argv[0], "band") == 0 && argc >= 2) {
		struct apple80211_phymode_data m = { APPLE80211_VERSION };

		if (strcmp(argv[1], "2.4") == 0)
			m.phy_mode = APPLE80211_MODE_11G;
		else if (strcmp(argv[1], "5") == 0)
			m.phy_mode = APPLE80211_MODE_11A;
		else if (strcmp(argv[1], "any") != 0)
			usage();
		if (request(1, APPLE80211_IOC_PHY_MODE, 0, &m, sizeof(m)) < 0) {
			perror("band");
			return 1;
		}
		return 0;
	}
	if (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "off") == 0) {
		struct apple80211_power_data p = { APPLE80211_VERSION, argv[0][1] == 'n' };
		return request(1, APPLE80211_IOC_POWER, 0, &p, sizeof(p)) < 0;
	}
	usage();
	return 2;
}
