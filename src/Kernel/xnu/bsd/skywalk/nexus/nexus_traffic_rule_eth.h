/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef _SKYWALK_NEXUS_TRAFFIC_RULE_ETH_H_
#define _SKYWALK_NEXUS_TRAFFIC_RULE_ETH_H_

#include <skywalk/nexus/nexus_traffic_rule.h>

__BEGIN_DECLS
void eth_traffic_rule_init(kern_allocation_name_t rule_tag);

int eth_traffic_rule_validate(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra);

int eth_traffic_rule_find(const char *ifname,
    struct ifnet_traffic_descriptor_common *td, uint32_t flags,
    struct nxctl_traffic_rule **ntrp);

int eth_traffic_rule_find_by_uuid(
	uuid_t uuid, struct nxctl_traffic_rule **ntrp);

void eth_traffic_rule_link(struct nxctl_traffic_rule *ntr);

void eth_traffic_rule_unlink(struct nxctl_traffic_rule *ntr);

int eth_traffic_rule_notify(struct nxctl_traffic_rule *ntr, uint32_t flags);

int eth_traffic_rule_get_count(const char *ifname, uint32_t *count);

int eth_traffic_rule_create(
	const char *ifname, struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra, uint32_t flags,
	struct nxctl_traffic_rule **ntrp);

void eth_traffic_rule_destroy(struct nxctl_traffic_rule *ntr);

int eth_traffic_rule_get_all(uint32_t size,
    uint32_t *count, user_addr_t uaddr);

__END_DECLS

#endif /* _SKYWALK_NEXUS_TRAFFIC_RULE_ETH_H_ */
