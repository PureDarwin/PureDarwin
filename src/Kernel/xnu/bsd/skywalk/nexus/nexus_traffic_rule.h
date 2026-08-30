/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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

#ifndef _SKYWALK_NEXUS_TRAFFIC_RULE_H_
#define _SKYWALK_NEXUS_TRAFFIC_RULE_H_

#include <skywalk/os_skywalk_private.h>

__BEGIN_DECLS
struct nxctl_traffic_rule;

/*
 * These callbacks need to be implemented for each rule type.
 */

/* Validate user provided parameters. */
typedef int (nxctl_traffic_rule_validate_cb_t)(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra);
/*
 * Each rule type has its own global structure for storing rules.
 * These callbacks access this global structure.
 */
#define NTR_FIND_FLAG_EXACT 0x0001
typedef int (nxctl_traffic_rule_find_cb_t)(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	uint32_t flags,
	struct nxctl_traffic_rule **ntrp);

typedef int (nxctl_traffic_rule_find_by_uuid_cb_t)(
	uuid_t uuid,
	struct nxctl_traffic_rule **ntrp);

typedef void (nxctl_traffic_rule_link_cb_t)(
	struct nxctl_traffic_rule *ntr);

typedef void (nxctl_traffic_rule_unlink_cb_t)(
	struct nxctl_traffic_rule *ntr);

/*
 * Notifies lower layers of the addition/removal of a rule.
 * This is called outside of nxctl_traffic_rule_lock to avoid potential
 * locking issues.
 */
#define NTR_NOTIFY_FLAG_ADD 0x0001
#define NTR_NOTIFY_FLAG_REMOVE 0x0002
typedef int (nxctl_traffic_rule_notify_cb_t)(
	struct nxctl_traffic_rule *ntr,
	uint32_t flags);

/*
 * Callback for a rule type to get rule count.
 */
typedef int (nxctl_traffic_rule_get_count_cb_t)(
	const char *ifname, uint32_t *count);

/*
 * Create/Destroy callbacks for a rule type.
 */
typedef int (nxctl_traffic_rule_create_cb_t)(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra,
	uint32_t flags,
	struct nxctl_traffic_rule **ntrp);

typedef void (nxctl_traffic_rule_destroy_cb_t)(
	struct nxctl_traffic_rule *ntr);

/*
 * This is used for copying all rules for a type (including generic
 * and type-specific info) to userspace.
 */
typedef int (nxctl_traffic_rule_get_all_cb_t)(
	uint32_t size,
	uint32_t *count,
	user_addr_t uaddr);

struct nxctl_traffic_rule_type {
	uint8_t ntrt_type;
	nxctl_traffic_rule_validate_cb_t *ntrt_validate;
	nxctl_traffic_rule_find_cb_t *ntrt_find;
	nxctl_traffic_rule_find_by_uuid_cb_t *ntrt_find_by_uuid;
	nxctl_traffic_rule_link_cb_t *ntrt_link;
	nxctl_traffic_rule_unlink_cb_t *ntrt_unlink;
	nxctl_traffic_rule_notify_cb_t *ntrt_notify;
	nxctl_traffic_rule_create_cb_t *ntrt_create;
	nxctl_traffic_rule_destroy_cb_t *ntrt_destroy;
	nxctl_traffic_rule_get_all_cb_t *ntrt_get_all;
	nxctl_traffic_rule_get_count_cb_t *ntrt_get_count;
};

/*
 * Generic traffic rule.
 * Contains fields common to all traffic rules.
 */
#define NTR_FLAG_PERSIST 0x0001
#define NTR_FLAG_ON_NXCTL_LIST 0x0002
struct nxctl_traffic_rule {
	uint8_t ntrt_type;
	uint32_t ntr_flags;
	os_refcnt_t ntr_refcnt;
	uuid_t ntr_uuid;
	char ntr_procname[NTR_PROCNAME_SZ];
	char ntr_ifname[IFNAMSIZ];
	SLIST_ENTRY(nxctl_traffic_rule) ntr_storage_link;
};

#define ITDBIT(set, bit) (((set) != 0) ? (bit) : 0)


void nxtr_wlock(void);
void nxtr_wunlock(void);
void nxtr_rlock(void);
void nxtr_runlock(void);

#define NXTR_WLOCK()  nxtr_wlock()
#define NXTR_WUNLOCK()  nxtr_wunlock()
#define NXTR_RLOCK()  nxtr_rlock()
#define NXTR_RUNLOCK()  nxtr_runlock()

void retain_traffic_rule(struct nxctl_traffic_rule *ntr);
void release_traffic_rule(struct nxctl_traffic_rule *ntr);

__END_DECLS

#endif /* _SKYWALK_NEXUS_TRAFFIC_RULE_H_ */
