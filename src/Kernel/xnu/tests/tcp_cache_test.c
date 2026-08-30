/*
 * Copyright (c) 2025 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <darwintest.h>
#include <sys/sysctl.h>
#include <netinet/tcp_cache.h>
#include <arpa/inet.h>
#include <stdlib.h>

T_DECL(tcp_cache_list_sysctl, "Test retrieving TCP cache list via sysctl")
{
	size_t size = 0;
	int ret;

	// First call to get the required buffer size
	ret = sysctlbyname("net.inet.tcp.cache_list", NULL, &size, NULL, 0);
	if (ret == -1) {
		T_SKIP("sysctlbyname(\"net.inet.tcp.cache_list\") error: %d", errno);
	}

	T_LOG("TCP cache list size: %zu bytes", size);

	if (size == 0) {
		T_PASS("No TCP cache entries found");
	}

	// Allocate buffer and retrieve the data
	void *buffer = malloc(size);
	T_QUIET; T_ASSERT_NOTNULL(buffer, "malloc buffer");

	ret = sysctlbyname("net.inet.tcp.cache_list", buffer, &size, NULL, 0);
	T_ASSERT_POSIX_SUCCESS(ret, "sysctlbyname to get data");

	// Calculate number of entries
	size_t num_entries = size / sizeof(struct tcp_cache_data);
	T_LOG("Found %zu TCP cache entries", num_entries);

	struct tcp_cache_data *entries = (struct tcp_cache_data *)buffer;

	// Log all fields of each entry
	for (size_t i = 0; i < num_entries; i++) {
		struct tcp_cache_data *entry = &entries[i];

		T_LOG("Entry %zu:", i);
		T_LOG("  tc_last_access: %u", entry->tc_last_access);
		T_LOG("  tc_key.tck_family: %d", entry->tc_key.tck_family);

		// Log source key info
		T_LOG("  tc_key.tck_src.thk_family: %d", entry->tc_key.tck_src.thk_family);
		if (entry->tc_key.tck_src.thk_family == AF_INET) {
			T_LOG("  tc_key.tck_src IP: %s", inet_ntoa(entry->tc_key.tck_src.thk_ip.addr));
		} else if (entry->tc_key.tck_src.thk_family == AF_INET6) {
			char addr_str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &entry->tc_key.tck_src.thk_ip.addr6, addr_str, sizeof(addr_str));
			T_LOG("  tc_key.tck_src IPv6: %s", addr_str);
		}

		// Log destination address
		if (entry->tc_key.tck_family == AF_INET) {
			T_LOG("  tc_key.tck_dst IP: %s", inet_ntoa(entry->tc_key.tck_dst.addr));
		} else if (entry->tc_key.tck_family == AF_INET6) {
			char addr_str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &entry->tc_key.tck_dst.addr6, addr_str, sizeof(addr_str));
			T_LOG("  tc_key.tck_dst IPv6: %s", addr_str);
		}

		// Log TFO cookie info
		T_LOG("  tc_tfo_cookie_len: %u", entry->tc_tfo_cookie_len);
		if (entry->tc_tfo_cookie_len > 0) {
			char cookie_hex[TFO_COOKIE_LEN_MAX * 2 + 1] = {0};
			for (int j = 0; j < entry->tc_tfo_cookie_len && j < TFO_COOKIE_LEN_MAX; j++) {
				snprintf(cookie_hex + j * 2, 3, "%02x", entry->tc_tfo_cookie[j]);
			}
			T_LOG("  tc_tfo_cookie: %s", cookie_hex);
		}

		// Log MPTCP info
		T_LOG("  tc_mptcp_version_confirmed: %u", entry->tc_mptcp_version_confirmed);
		T_LOG("  tc_mptcp_version: %u", entry->tc_mptcp_version);
		T_LOG("  tc_mptcp_next_version_try: %u", entry->tc_mptcp_next_version_try);
		T_LOG(""); // Empty line between entries
	}

	free(buffer);

	T_PASS("%s", __func__);
}

T_DECL(tcp_heuristics_list_sysctl, "Test retrieving TCP heuristics list via sysctl")
{
	size_t size = 0;
	int ret;

	// First call to get the required buffer size
	ret = sysctlbyname("net.inet.tcp.heuristics_list", NULL, &size, NULL, 0);
	if (ret == -1) {
		T_SKIP("sysctlbyname(\"net.inet.tcp.cache_list\") error: %d", errno);
	}

	T_LOG("TCP heuristics list size: %zu bytes", size);

	if (size == 0) {
		T_PASS("No TCP heuristics entries found");
	}

	// Allocate buffer and retrieve the data
	void *buffer = malloc(size);
	T_QUIET; T_ASSERT_NOTNULL(buffer, "malloc buffer");

	ret = sysctlbyname("net.inet.tcp.heuristics_list", buffer, &size, NULL, 0);
	T_ASSERT_POSIX_SUCCESS(ret, "sysctlbyname to get data");

	// Calculate number of entries
	size_t num_entries = size / sizeof(struct tcp_heuristics_data);
	T_LOG("Found %zu TCP heuristics entries", num_entries);

	struct tcp_heuristics_data *entries = (struct tcp_heuristics_data *)buffer;

	// Log all fields of each entry
	for (size_t i = 0; i < num_entries; i++) {
		struct tcp_heuristics_data *entry = &entries[i];

		T_LOG("Heuristics Entry %zu:", i);
		T_LOG("  th_last_access: %u", entry->th_last_access);
		T_LOG("  th_key.thk_family: %d", entry->th_key.thk_family);

		// Log source key info
		if (entry->th_key.thk_family == AF_INET) {
			T_LOG("  th_key.thk_ip IP: %s", inet_ntoa(entry->th_key.thk_ip.addr));
		} else if (entry->th_key.thk_family == AF_INET6) {
			char addr_str[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, &entry->th_key.thk_ip.addr6, addr_str, sizeof(addr_str));
			T_LOG("  th_key.thk_ip IPv6: %s", addr_str);
		}

		// Log TFO heuristics
		T_LOG("  th_tfo_data_loss: %u", entry->th_tfo_data_loss);
		T_LOG("  th_tfo_req_loss: %u", entry->th_tfo_req_loss);
		T_LOG("  th_tfo_data_rst: %u", entry->th_tfo_data_rst);


		T_LOG("  th_tfo_req_rst: %u", entry->th_tfo_req_rst);

		// Log MPTCP heuristics
		T_LOG("  th_mptcp_loss: %u", entry->th_mptcp_loss);
		T_LOG("  th_mptcp_success: %u", entry->th_mptcp_success);

		// Log ECN heuristics
		T_LOG("  th_ecn_droprst: %u", entry->th_ecn_droprst);
		T_LOG("  th_ecn_synrst: %u", entry->th_ecn_synrst);

		// Log timing information
		T_LOG("  th_tfo_enabled_time: %u", entry->th_tfo_enabled_time);
		T_LOG("  th_tfo_backoff_until: %u", entry->th_tfo_backoff_until);
		T_LOG("  th_tfo_backoff: %u", entry->th_tfo_backoff);
		T_LOG("  th_mptcp_backoff: %u", entry->th_mptcp_backoff);
		T_LOG("  th_ecn_backoff: %u", entry->th_ecn_backoff);

		// Log flags
		T_LOG("  th_tfo_in_backoff: %u", entry->th_tfo_in_backoff);
		T_LOG("  th_mptcp_in_backoff: %u", entry->th_mptcp_in_backoff);
		T_LOG("  th_mptcp_heuristic_disabled: %u", entry->th_mptcp_heuristic_disabled);
		T_LOG(""); // Empty line between entries
	}

	free(buffer);

	T_PASS("%s", __func__);
}
