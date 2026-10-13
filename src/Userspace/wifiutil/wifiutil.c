/*
 * wifiutil - scan for, join and leave 802.11 networks, show the link and the
 * networks this computer remembers, through pdwifi.
 *
 * Copyright (c) 2026 The PureDarwin Project
 *
 * SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include "apple80211_ioctl.h"
#include "pdwifi.h"

// libSystem exports notify(3) but this sdk does not ship its header
uint32_t notify_post(const char *name);

static int scan(void)
{
	pdwifi_network nets[PDWIFI_MAX_NETWORKS];
	int n = pdwifi_scan(nets, PDWIFI_MAX_NETWORKS);

	printf("%-32s %-17s %4s %4s  %s\n", "SSID", "BSSID", "RSSI", "CH", "SECURITY");
	for (int i = 0; i < n; i++) {
		const uint8_t *b = nets[i].bssid;

		printf("%-32s %02x:%02x:%02x:%02x:%02x:%02x %4d %4u  %s%s\n", nets[i].ssid, b[0], b[1], b[2], b[3], b[4], b[5],
		    nets[i].rssi, nets[i].channel, nets[i].security ? nets[i].security : "open",
		    pdwifi_is_known(nets[i].ssid) ? "  known" : "");
	}
	return 0;
}

static int info(void)
{
	static const char *states[] = { "init", "scanning", "authenticating", "associating", "running" };
	pdwifi_network cur;
	uint8_t mac[6];
	int st = pdwifi_state();

	printf("interface: %s\n", pdwifi_interface());
	printf("power:   %s\n", pdwifi_power() ? "on" : "off");
	if (pdwifi_hwaddr(mac))
		printf("address: %02x:%02x:%02x:%02x:%02x:%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	printf("state:   %s\n", st >= 0 && st < 5 ? states[st] : "?");
	if (pdwifi_current(&cur)) {
		const uint8_t *b = cur.bssid;

		printf("ssid:    %s\n", cur.ssid);
		printf("bssid:   %02x:%02x:%02x:%02x:%02x:%02x\n", b[0], b[1], b[2], b[3], b[4], b[5]);
		printf("channel: %u\n", cur.channel);
		printf("rssi:    %d dBm\n", cur.rssi);
	}
	return 0;
}

static int known(void)
{
	char names[PDWIFI_MAX_NETWORKS][33];
	int n = pdwifi_known(names, PDWIFI_MAX_NETWORKS);

	for (int i = 0; i < n; i++)
		printf("%s%s\n", names[i], pdwifi_is_known(names[i]) == 2 ? "" : "  (open)");
	return 0;
}

// the menu bar and Settings redraw when the command line changes something
static int result(pdwifi_result r)
{
	notify_post("org.puredarwin.wifi.changed");
	if (r != PDWIFI_OK)
		fprintf(stderr, "wifiutil: %s\n", pdwifi_message(r));
	return r != PDWIFI_OK;
}

// band is a driver setting with no place in pdwifi yet
static int band(const char *which)
{
	struct apple80211_phymode_data m = { APPLE80211_VERSION };
	struct apple80211req req;
	int s, rc;

	if (strcmp(which, "2.4") == 0)
		m.phy_mode = APPLE80211_MODE_11G;
	else if (strcmp(which, "5") == 0)
		m.phy_mode = APPLE80211_MODE_11A;
	else if (strcmp(which, "any") != 0)
		return 2;
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		return 1;
	memset(&req, 0, sizeof(req));
	strlcpy(req.req_if_name, pdwifi_interface(), sizeof(req.req_if_name));
	req.req_type = APPLE80211_IOC_PHY_MODE;
	req.req_len = sizeof(m);
	req.req_data = &m;
	rc = ioctl(s, SIOCSA80211, &req);
	close(s);
	if (rc < 0)
		perror("band");
	return rc < 0;
}

static void usage(void)
{
	fprintf(stderr, "usage: wifiutil [-i interface] [-n] scan | info | join ssid [passphrase] | leave | known | forget ssid\n"
	                "                autojoin | band 2.4|5|any | off | on\n"
	                "  -n  join without remembering the network\n");
	exit(2);
}

int main(int argc, char **argv)
{
	int ch, keep = 1;

	while ((ch = getopt(argc, argv, "i:n")) != -1) {
		if (ch == 'i')
			pdwifi_set_interface(optarg);
		else if (ch == 'n')
			keep = 0;
		else
			usage();
	}
	argc -= optind;
	argv += optind;
	if (argc < 1)
		usage();
	if (strcmp(argv[0], "known") == 0)
		return known();
	if (strcmp(argv[0], "forget") == 0 && argc >= 2) {
		pdwifi_forget(argv[1]);
		notify_post("org.puredarwin.wifi.changed");
		return 0;
	}
	if (pdwifi_interface() == NULL) {
		fprintf(stderr, "wifiutil: %s\n", pdwifi_message(PDWIFI_NO_HARDWARE));
		return 1;
	}
	if (strcmp(argv[0], "scan") == 0)
		return scan();
	if (strcmp(argv[0], "info") == 0)
		return info();
	if (strcmp(argv[0], "join") == 0 && argc >= 2) {
		pdwifi_result r = pdwifi_is_known(argv[1]) && argc < 3 ? pdwifi_join_known(argv[1])
		                                                          : pdwifi_join(argv[1], argc >= 3 ? argv[2] : NULL, keep);
		return result(r);
	}
	if (strcmp(argv[0], "leave") == 0)
		return result(pdwifi_leave());
	if (strcmp(argv[0], "autojoin") == 0)
		return result(pdwifi_autojoin());
	if (strcmp(argv[0], "band") == 0 && argc >= 2) {
		int rc = band(argv[1]);
		if (rc == 2)
			usage();
		return rc;
	}
	if (strcmp(argv[0], "on") == 0 || strcmp(argv[0], "off") == 0)
		return result(pdwifi_set_power(argv[0][1] == 'n'));
	usage();
	return 2;
}
