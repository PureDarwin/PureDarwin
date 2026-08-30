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
#include <skywalk/nexus/nexus_traffic_rule_eth.h>
#include <skywalk/nexus/netif/nx_netif.h>
#include <net/ethernet.h>

/*
 * Eth-specific traffic rule.
 */
struct nxctl_traffic_rule_eth {
	struct nxctl_traffic_rule ntre_common;
	SLIST_ENTRY(nxctl_traffic_rule_eth) ntre_storage_link;
	struct ifnet_traffic_descriptor_eth ntre_td;
	struct ifnet_traffic_rule_action_steer ntre_ra;
};

/*
 * Currently supported tuple types.
 */
#define ETRM(type, raddr) \
	ITDBIT(type,  IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_ETHER_TYPE) | \
	ITDBIT(raddr, IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_RADDR)

static uint8_t nxctl_eth_traffic_rule_masks[] = {
	ETRM(1, 0),
	ETRM(0, 1),
};
#define NETHRULEMASKS \
    (sizeof(nxctl_eth_traffic_rule_masks)/sizeof(uint8_t))

/* Per-interface lists of eth traffic rules */
SLIST_HEAD(nxctl_traffic_rule_eth_head, nxctl_traffic_rule_eth);
struct nxctl_traffic_rule_eth_if {
	char rei_ifname[IFNAMSIZ];
	struct nxctl_traffic_rule_eth_head rei_lists[NETHRULEMASKS];
	uint32_t rei_count;
	SLIST_ENTRY(nxctl_traffic_rule_eth_if) rei_link;
};

/* List of per-interface lists */
SLIST_HEAD(nxctl_traffic_rule_eth_if_head, nxctl_traffic_rule_eth_if);
struct nxctl_traffic_rule_eth_storage {
	struct nxctl_traffic_rule_eth_if_head res_if_list;
	uint32_t res_count;
};

static struct nxctl_traffic_rule_eth_storage *rs = NULL;
static kern_allocation_name_t nxctl_traffic_rule_tag = NULL;

/*
 * If an interface attaches after rule(s) are added, this function is used
 * retrieve the current rule count for that interface.
 */
int
nxctl_eth_traffic_rule_get_count(const char *ifname, uint32_t *count)
{
	int err;

	NXTR_RLOCK();
	err = eth_traffic_rule_get_count(ifname, count);
	NXTR_RUNLOCK();

	return err;
}

/*
 * Used for finding the qset id associated with a ether type and ether remote addr.
 */
int
nxctl_eth_traffic_rule_find_qset_id(const char *ifname,
    uint16_t eth_type, ether_addr_t *eth_raddr, uint64_t *qset_id)
{
	struct nxctl_traffic_rule_eth *__single ntre = NULL;
	struct nxctl_traffic_rule *__single ntr = NULL;
	struct ifnet_traffic_descriptor_eth td = {0};
	int err;

	td.eth_common.itd_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_ETH;
	td.eth_common.itd_len = sizeof(td);
	td.eth_common.itd_flags = IFNET_TRAFFIC_DESCRIPTOR_FLAG_INBOUND |
	    IFNET_TRAFFIC_DESCRIPTOR_FLAG_OUTBOUND;

	td.eth_type = eth_type;
	td.eth_mask = IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_ETHER_TYPE;

	if (eth_raddr) {
		bcopy(eth_raddr, &td.eth_raddr, ETHER_ADDR_LEN);
		td.eth_mask |= IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_RADDR;
	}

	NXTR_RLOCK();
	err = eth_traffic_rule_find(ifname, &td.eth_common, 0, &ntr);
	if (err != 0) {
		goto fail;
	}
	ntre = __container_of(ntr, struct nxctl_traffic_rule_eth, ntre_common);
	*qset_id = ntre->ntre_ra.ras_qset_id;
	NXTR_RUNLOCK();
	return 0;
fail:
	NXTR_RUNLOCK();
	return err;
}

static int
parse_eth_hdr(struct __kern_packet *pkt, uint16_t *eth_type, ether_addr_t *eth_raddr)
{
	volatile ether_header_t *_l2 = NULL;
	uint8_t *pkt_buf, *l2_hdr;
	uint32_t bdlen, bdlim, bdoff, cls_len;
	int err;

	ASSERT(pkt->pkt_l2_len <= pkt->pkt_length);

	MD_BUFLET_ADDR_ABS_DLEN(pkt, pkt_buf, bdlen, bdlim, bdoff);
	cls_len = bdlim - bdoff;
	cls_len = (uint32_t)MIN(cls_len, pkt->pkt_length);
	VERIFY(pkt->pkt_length >= cls_len);
	if (cls_len == 0) {
		SK_ERR("cls_len == 0");
		err = EINVAL;
		goto fail;
	}

	l2_hdr = pkt_buf + pkt->pkt_headroom;
	_l2 = (volatile ether_header_t *)(void *)l2_hdr;

	*eth_type = ntohs(_l2->ether_type);
	bcopy(__DECONST(void *, &_l2->ether_dhost), eth_raddr, ETHER_ADDR_LEN);

	return 0;

fail:
	DTRACE_SKYWALK4(classify__failed, ether_header_t *, _l2, size_t, pkt->pkt_length,
	    uint8_t, pkt->pkt_l2_len, int, err);
	return err;
}

int
nxctl_eth_traffic_rule_find_qset_id_with_pkt(const char *ifname,
    struct __kern_packet *pkt, uint64_t *qset_id)
{
	ether_addr_t eth_raddr;
	uint16_t eth_type;
	int err;

	err = parse_eth_hdr(pkt, &eth_type, &eth_raddr);
	if (err != 0) {
		return err;
	}
	return nxctl_eth_traffic_rule_find_qset_id(ifname, eth_type, &eth_raddr, qset_id);
}

void
eth_traffic_rule_init(kern_allocation_name_t rule_tag)
{
	ASSERT(nxctl_traffic_rule_tag == NULL);
	nxctl_traffic_rule_tag = rule_tag;
}

int
eth_traffic_rule_validate(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra)
{
	char buf[IFNAMSIZ];
	int unit, i;
	struct ifnet_traffic_descriptor_eth *tdi;

	if (ifunit_extract(ifname, buf, sizeof(buf), &unit) < 0) {
		SK_ERR("invalid ifname: %s", ifname);
		return EINVAL;
	}
	if (td->itd_len != sizeof(*tdi)) {
		SK_ERR("invalid td len: expected %lu, actual %d",
		    sizeof(*tdi), td->itd_len);
		return EINVAL;
	}
	if (td->itd_flags == 0 ||
	    (td->itd_flags &
	    ~(IFNET_TRAFFIC_DESCRIPTOR_FLAG_INBOUND |
	    IFNET_TRAFFIC_DESCRIPTOR_FLAG_OUTBOUND)) != 0) {
		SK_ERR("invalid td flags: 0x%x", td->itd_flags);
		return EINVAL;
	}
	tdi = (struct ifnet_traffic_descriptor_eth *)td;
	for (i = 0; i < NETHRULEMASKS; i++) {
		if (tdi->eth_mask == nxctl_eth_traffic_rule_masks[i]) {
			break;
		}
	}
	if (i == NETHRULEMASKS) {
		SK_ERR("invalid eth mask: 0x%x", tdi->eth_mask);
		return EINVAL;
	}
	if ((tdi->eth_mask & IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_ETHER_TYPE) != 0) {
		if (tdi->eth_type != ETHERTYPE_PAE &&
		    tdi->eth_type != ETHERTYPE_WAI) {
			SK_ERR("invalid eth type 0x%x", tdi->eth_type);
			return EINVAL;
		}
	}
	if ((tdi->eth_mask & IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_RADDR) != 0) {
		ether_addr_t mac_zeros_addr = {0};
		if (!_ether_cmp(&tdi->eth_raddr, &mac_zeros_addr)) {
			SK_ERR("eth raddr cannot be unspecified");
			return EINVAL;
		}
	}
	if (ra->ra_len != sizeof(struct ifnet_traffic_rule_action_steer)) {
		SK_ERR("invalid ra len: expected %lu, actual %d",
		    sizeof(struct ifnet_traffic_rule_action_steer), ra->ra_len);
		return EINVAL;
	}
	return 0;
}

SK_NO_INLINE_ATTRIBUTE
static void
eth_traffic_rule_storage_create(void)
{
	rs = sk_alloc_type(struct nxctl_traffic_rule_eth_storage,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	SLIST_INIT(&rs->res_if_list);
	rs->res_count = 0;
	return;
}

SK_NO_INLINE_ATTRIBUTE
static void
eth_traffic_rule_storage_destroy(void)
{
	ASSERT(rs->res_count == 0);
	ASSERT(SLIST_EMPTY(&rs->res_if_list));
	sk_free_type(struct nxctl_traffic_rule_eth_storage, rs);
}

SK_NO_INLINE_ATTRIBUTE
static struct nxctl_traffic_rule_eth_if *
eth_traffic_rule_if_create(const char *ifname)
{
	struct nxctl_traffic_rule_eth_if *rif;
	int i;

	rif = sk_alloc_type(struct nxctl_traffic_rule_eth_if,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	for (i = 0; i < NETHRULEMASKS; i++) {
		SLIST_INIT(&rif->rei_lists[i]);
	}
	strlcpy(rif->rei_ifname, ifname, sizeof(rif->rei_ifname));
	rif->rei_count = 0;
	return rif;
}

SK_NO_INLINE_ATTRIBUTE
static void
eth_traffic_rule_if_destroy(struct nxctl_traffic_rule_eth_if *rif)
{
	int i;

	for (i = 0; i < NETHRULEMASKS; i++) {
		ASSERT(SLIST_EMPTY(&rif->rei_lists[i]));
	}
	ASSERT(rif->rei_count == 0);
	sk_free_type(struct nxctl_traffic_rule_eth_if, rif);
}

SK_NO_INLINE_ATTRIBUTE
static boolean_t
eth_traffic_rule_match(struct nxctl_traffic_rule_eth *ntre, const char *ifname,
    uint32_t flags, struct ifnet_traffic_descriptor_eth *tdi)
{
	struct nxctl_traffic_rule *ntr = (struct nxctl_traffic_rule *)ntre;
	struct ifnet_traffic_descriptor_eth *tdi0;
	uint8_t mask;
	boolean_t exact;

	VERIFY(strlcmp(ntr->ntr_ifname, ifname, sizeof(ntr->ntr_ifname)) == 0);
	tdi0 = &ntre->ntre_td;

	exact = ((flags & NTR_FIND_FLAG_EXACT) != 0);
	mask = tdi0->eth_mask & tdi->eth_mask;
	if (exact) {
		ASSERT(tdi0->eth_mask == tdi->eth_mask);
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_ETHER_TYPE) != 0 &&
	    tdi0->eth_type != tdi->eth_type) {
		DTRACE_SKYWALK2(eth_type__mismatch,
		    uint8_t, tdi0->eth_type, uint8_t, tdi->eth_type);
		return FALSE;
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_ETH_MASK_RADDR) != 0 &&
	    _ether_cmp(&tdi0->eth_raddr, &tdi->eth_raddr)) {
		DTRACE_SKYWALK2(eth_raddr__mismatch,
		    ether_addr_t *, &tdi0->eth_raddr,
		    ether_addr_t *, &tdi->eth_raddr);
		return FALSE;
	}
	return TRUE;
}

int
eth_traffic_rule_find(const char *ifname,
    struct ifnet_traffic_descriptor_common *td, uint32_t flags,
    struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_eth *ntre = NULL;
	struct nxctl_traffic_rule_eth_if *rif;
	struct ifnet_traffic_descriptor_eth *tdi =
	    (struct ifnet_traffic_descriptor_eth *)td;
	int i;

	if (rs == NULL) {
		return ENOENT;
	}
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		if (strlcmp(rif->rei_ifname, ifname, sizeof(rif->rei_ifname)) != 0) {
			continue;
		}
		for (i = 0; i < NETHRULEMASKS; i++) {
			if ((flags & NTR_FIND_FLAG_EXACT) != 0 &&
			    tdi->eth_mask != nxctl_eth_traffic_rule_masks[i]) {
				continue;
			}
			SLIST_FOREACH(ntre, &rif->rei_lists[i], ntre_storage_link) {
				if (eth_traffic_rule_match(ntre, ifname, flags, tdi)) {
					*ntrp = (struct nxctl_traffic_rule *)ntre;
					return 0;
				}
			}
		}
	}
	return ENOENT;
}

int
eth_traffic_rule_find_by_uuid(
	uuid_t uuid, struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_eth *ntre;
	struct nxctl_traffic_rule *ntr;
	struct nxctl_traffic_rule_eth_if *rif;
	int i;

	if (rs == NULL) {
		return ENOENT;
	}
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		for (i = 0; i < NETHRULEMASKS; i++) {
			SLIST_FOREACH(ntre, &rif->rei_lists[i], ntre_storage_link) {
				ntr = &ntre->ntre_common;
				if (uuid_compare(ntr->ntr_uuid, uuid) == 0) {
					*ntrp = ntr;
					return 0;
				}
			}
		}
	}
	return ENOENT;
}

static void
eth_update_ifnet_traffic_rule_count(const char *ifname, uint32_t count)
{
	struct ifnet *ifp;

	ifp = ifunit_ref(ifname);
	if (ifp == NULL) {
		DTRACE_SKYWALK1(ifname__not__found, char *, ifname);
		return;
	}
	ifnet_update_eth_traffic_rule_count(ifp, count);
	ifnet_decr_iorefcnt(ifp);
}

void
eth_traffic_rule_link(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_eth_if *rif;
	struct nxctl_traffic_rule_eth *ntre =
	    (struct nxctl_traffic_rule_eth *)ntr;
	struct nxctl_traffic_rule_eth_head *list = NULL;
	int i;
	char *__null_terminated ntr_ifname = NULL;
	char *__null_terminated rei_ifname = NULL;

	if (rs == NULL) {
		eth_traffic_rule_storage_create();
	}
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		if (strbufcmp(rif->rei_ifname, ntr->ntr_ifname) == 0) {
			break;
		}
	}
	if (rif == NULL) {
		ntr_ifname = __unsafe_null_terminated_from_indexable(ntr->ntr_ifname);
		rif = eth_traffic_rule_if_create(ntr_ifname);
		SLIST_INSERT_HEAD(&rs->res_if_list, rif, rei_link);
	}
	for (i = 0; i < NETHRULEMASKS; i++) {
		if (ntre->ntre_td.eth_mask ==
		    nxctl_eth_traffic_rule_masks[i]) {
			list = &rif->rei_lists[i];
			break;
		}
	}
	retain_traffic_rule(ntr);
	ASSERT(list != NULL);
	SLIST_INSERT_HEAD(list, ntre, ntre_storage_link);
	/* per-interface count */
	rif->rei_count++;
	rei_ifname = __unsafe_null_terminated_from_indexable(rif->rei_ifname);
	eth_update_ifnet_traffic_rule_count(rei_ifname, rif->rei_count);

	/* global count */
	rs->res_count++;
}

void
eth_traffic_rule_unlink(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_eth_if *rif;
	struct nxctl_traffic_rule_eth *ntre =
	    (struct nxctl_traffic_rule_eth *)ntr;
	struct nxctl_traffic_rule_eth_head *list = NULL;
	int i;
	char *__null_terminated rei_ifname = NULL;

	ASSERT(rs != NULL);
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		if (strbufcmp(rif->rei_ifname, ntr->ntr_ifname) == 0) {
			break;
		}
	}
	ASSERT(rif != NULL);
	for (i = 0; i < NETHRULEMASKS; i++) {
		if (ntre->ntre_td.eth_mask ==
		    nxctl_eth_traffic_rule_masks[i]) {
			list = &rif->rei_lists[i];
			break;
		}
	}
	ASSERT(list != NULL);
	SLIST_REMOVE(list, ntre, nxctl_traffic_rule_eth, ntre_storage_link);
	rif->rei_count--;
	rei_ifname = __unsafe_null_terminated_from_indexable(rif->rei_ifname);
	eth_update_ifnet_traffic_rule_count(rei_ifname, rif->rei_count);

	rs->res_count--;
	release_traffic_rule(ntr);

	if (rif->rei_count == 0) {
		SLIST_REMOVE(&rs->res_if_list, rif, nxctl_traffic_rule_eth_if, rei_link);
		eth_traffic_rule_if_destroy(rif);
	}
	if (rs->res_count == 0) {
		eth_traffic_rule_storage_destroy();
		rs = NULL;
	}
}

/*
 * XXX
 * This may need additional changes to ensure safety against detach/attach.
 */
int
eth_traffic_rule_notify(struct nxctl_traffic_rule *ntr, uint32_t flags)
{
	struct ifnet *ifp;
	struct nx_netif *nif;
	struct netif_qset *__single qset = NULL;
	struct nxctl_traffic_rule_eth *ntre;
	int err = 0;
	char *__null_terminated ntr_ifname = NULL;

	ntr_ifname = __unsafe_null_terminated_from_indexable(ntr->ntr_ifname);
	ifp = ifunit_ref(ntr_ifname);
	if (ifp == NULL) {
		DTRACE_SKYWALK1(ifname__not__found, char *, ntr->ntr_ifname);
		err = ENXIO;
		goto done;
	}
	nif = NA(ifp)->nifna_netif;
	if (!NX_LLINK_PROV(nif->nif_nx)) {
		DTRACE_SKYWALK1(llink__not__enabled, struct ifnet *, ifp);
		err = ENOTSUP;
		goto done;
	}
	ntre = (struct nxctl_traffic_rule_eth *)ntr;
	qset = nx_netif_find_qset(nif, ntre->ntre_ra.ras_qset_id);
	err = nx_netif_notify_steering_info(nif, qset,
	    (struct ifnet_traffic_descriptor_common *)&ntre->ntre_td,
	    ((flags & NTR_NOTIFY_FLAG_ADD) != 0));
done:
	if (qset != NULL) {
		nx_netif_qset_release(&qset);
	}
	if (ifp != NULL) {
		ifnet_decr_iorefcnt(ifp);
	}
	return err;
}

int
eth_traffic_rule_get_count(const char *ifname, uint32_t *count)
{
	struct nxctl_traffic_rule_eth_if *rif;
	int err;

	if (rs == NULL) {
		err = ENOENT;
		goto fail;
	}
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		if (strlcmp(rif->rei_ifname, ifname, sizeof(rif->rei_ifname)) == 0) {
			break;
		}
	}
	if (rif == NULL) {
		err = ENOENT;
		goto fail;
	}
	*count = rif->rei_count;
	return 0;
fail:
	return err;
}

int
eth_traffic_rule_create(
	const char *ifname, struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra, uint32_t flags,
	struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_eth *ntre;
	struct nxctl_traffic_rule *ntr;
	struct ifnet_traffic_descriptor_eth *tdi;
	struct ifnet_traffic_rule_action_steer *ras;

	ntre = sk_alloc_type(struct nxctl_traffic_rule_eth,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	ntr = &ntre->ntre_common;

	ntr->ntrt_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_ETH;
	ntr->ntr_flags = flags;
	uuid_generate(ntr->ntr_uuid);
	os_ref_init(&ntr->ntr_refcnt, NULL);

	strlcpy(ntr->ntr_ifname, ifname, sizeof(ntr->ntr_ifname));
	proc_selfname(ntr->ntr_procname, sizeof(ntr->ntr_procname));

	tdi = __container_of(td, struct ifnet_traffic_descriptor_eth, eth_common);
	ras = __container_of(ra, struct ifnet_traffic_rule_action_steer, ras_common);
	bcopy(tdi, &ntre->ntre_td, sizeof(ntre->ntre_td));
	bcopy(ras, &ntre->ntre_ra, sizeof(ntre->ntre_ra));

	*ntrp = ntr;
	return 0;
}

void
eth_traffic_rule_destroy(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_eth *ntre;

	ASSERT(os_ref_get_count(&ntr->ntr_refcnt) == 0);
	ntre = (struct nxctl_traffic_rule_eth *)ntr;
	sk_free_type(struct nxctl_traffic_rule_eth, ntre);
}

static void
convert_ntre_to_iocinfo(struct nxctl_traffic_rule_eth *ntre,
    struct nxctl_traffic_rule_eth_iocinfo *info)
{
	struct nxctl_traffic_rule *ntr;
	struct nxctl_traffic_rule_generic_iocinfo *ginfo;

	bzero(info, sizeof(*info));
	ntr = &ntre->ntre_common;
	ginfo = &info->tre_common;
	static_assert(sizeof(ntr->ntr_procname) == sizeof(ginfo->trg_procname));
	static_assert(sizeof(ntr->ntr_ifname) == sizeof(ginfo->trg_ifname));
	uuid_copy(ginfo->trg_uuid, ntr->ntr_uuid);
	strbufcpy(ginfo->trg_procname, ntr->ntr_procname);
	strbufcpy(ginfo->trg_ifname, ntr->ntr_ifname);
	bcopy(&ntre->ntre_td, &info->tre_td, sizeof(info->tre_td));
	bcopy(&ntre->ntre_ra, &info->tre_ra, sizeof(info->tre_ra));
}

int
eth_traffic_rule_get_all(uint32_t size,
    uint32_t *count, user_addr_t uaddr)
{
	struct nxctl_traffic_rule_eth *ntre = NULL;
	struct nxctl_traffic_rule_eth_if *rif;
	struct nxctl_traffic_rule_eth_iocinfo info;
	int i, err;

	if (size != sizeof(info)) {
		SK_ERR("size: actual %d, expected %lu", size, sizeof(info));
		return EINVAL;
	}
	if (rs == NULL) {
		*count = 0;
		return 0;
	}
	if (*count < rs->res_count) {
		SK_ERR("count: given %d, require: %d", *count, rs->res_count);
		return ENOBUFS;
	}
	SLIST_FOREACH(rif, &rs->res_if_list, rei_link) {
		for (i = 0; i < NETHRULEMASKS; i++) {
			SLIST_FOREACH(ntre, &rif->rei_lists[i], ntre_storage_link) {
				convert_ntre_to_iocinfo(ntre, &info);
				err = copyout(&info, uaddr, sizeof(info));
				if (err != 0) {
					SK_ERR("copyout failed: %d", err);
					return err;
				}
				uaddr += sizeof(info);
			}
		}
	}
	*count = rs->res_count;
	return 0;
}
