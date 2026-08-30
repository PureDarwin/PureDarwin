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

#include <skywalk/nexus/nexus_traffic_rule_inet.h>
#include <skywalk/nexus/netif/nx_netif.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

/*
 * Inet-specific traffic rule.
 */
struct nxctl_traffic_rule_inet {
	struct nxctl_traffic_rule ntri_common;
	SLIST_ENTRY(nxctl_traffic_rule_inet) ntri_storage_link;
	struct ifnet_traffic_descriptor_inet ntri_td;
	struct ifnet_traffic_rule_action_steer ntri_ra;
};

/*
 * Currently supported tuple types.
 */
#define ITRM(proto, laddr, raddr, lport, rport) \
	(IFNET_TRAFFIC_DESCRIPTOR_INET_IPVER | \
	ITDBIT(proto, IFNET_TRAFFIC_DESCRIPTOR_INET_PROTO) | \
	ITDBIT(laddr, IFNET_TRAFFIC_DESCRIPTOR_INET_LADDR) | \
	ITDBIT(raddr, IFNET_TRAFFIC_DESCRIPTOR_INET_RADDR) | \
	ITDBIT(lport, IFNET_TRAFFIC_DESCRIPTOR_INET_LPORT) | \
	ITDBIT(rport, IFNET_TRAFFIC_DESCRIPTOR_INET_RPORT))

static uint8_t nxctl_inet_traffic_rule_masks[] = {
	ITRM(1, 1, 1, 1, 1),
	ITRM(1, 1, 1, 1, 0),
	ITRM(1, 1, 1, 0, 1),
	ITRM(1, 1, 1, 0, 0),
	ITRM(1, 1, 0, 1, 1),
	ITRM(1, 1, 0, 1, 0),
	ITRM(1, 1, 0, 0, 1),
	ITRM(1, 1, 0, 0, 0),
	ITRM(1, 0, 1, 1, 1),
	ITRM(1, 0, 1, 1, 0),
	ITRM(1, 0, 1, 0, 1),
	ITRM(1, 0, 1, 0, 0),
	ITRM(1, 0, 0, 1, 1),
	ITRM(1, 0, 0, 1, 0),
	ITRM(1, 0, 0, 0, 1),
	// ITRM(1, 0, 0, 0, 0), addr or port is required
	ITRM(0, 1, 1, 1, 1),
	ITRM(0, 1, 1, 1, 0),
	ITRM(0, 1, 1, 0, 1),
	ITRM(0, 1, 1, 0, 0),
	ITRM(0, 1, 0, 1, 1),
	ITRM(0, 1, 0, 1, 0),
	ITRM(0, 1, 0, 0, 1),
	ITRM(0, 1, 0, 0, 0),
	ITRM(0, 0, 1, 1, 1),
	ITRM(0, 0, 1, 1, 0),
	ITRM(0, 0, 1, 0, 1),
	ITRM(0, 0, 1, 0, 0),
	ITRM(0, 0, 0, 1, 1),
	ITRM(0, 0, 0, 1, 0),
	ITRM(0, 0, 0, 0, 1),
	// ITRM(0, 0, 0, 0, 0),
};
#define NINETRULEMASKS \
    (sizeof(nxctl_inet_traffic_rule_masks)/sizeof(uint8_t))

/* Per-interface lists of inet traffic rules */
SLIST_HEAD(nxctl_traffic_rule_inet_head, nxctl_traffic_rule_inet);
struct nxctl_traffic_rule_inet_if {
	char rii_ifname[IFNAMSIZ];
	struct nxctl_traffic_rule_inet_head rii_lists[NINETRULEMASKS];
	uint32_t rii_count;
	SLIST_ENTRY(nxctl_traffic_rule_inet_if) rii_link;
};

/* List of per-interface lists */
SLIST_HEAD(nxctl_traffic_rule_inet_if_head, nxctl_traffic_rule_inet_if);
struct nxctl_traffic_rule_inet_storage {
	struct nxctl_traffic_rule_inet_if_head ris_if_list;
	uint32_t ris_count;
};

static struct nxctl_traffic_rule_inet_storage *rs = NULL;
static kern_allocation_name_t nxctl_traffic_rule_tag = NULL;

static boolean_t inet_v6addr_cmp(struct ifnet_ip_addr *a1,
    struct ifnet_ip_addr *a2);

/*
 * If an interface attaches after rule(s) are added, this function is used
 * retrieve the current rule count for that interface.
 */
int
nxctl_inet_traffic_rule_get_count(const char *ifname, uint32_t *count)
{
	int err;

	NXTR_RLOCK();
	err = inet_traffic_rule_get_count(ifname, count);
	NXTR_RUNLOCK();

	return err;
}

/*
 * Used for finding the qset id associated with a traffic descriptor.
 */
int
nxctl_inet_traffic_rule_find_qset_id(const char *ifname,
    struct ifnet_traffic_descriptor_inet *td, uint64_t *qset_id)
{
	struct nxctl_traffic_rule_inet *__single ntri = NULL;
	struct nxctl_traffic_rule *__single ntr = NULL;
	int err;

	NXTR_RLOCK();
	err = inet_traffic_rule_find(ifname, &td->inet_common, 0, &ntr);
	if (err != 0) {
		goto fail;
	}
	ntri = __container_of(ntr, struct nxctl_traffic_rule_inet, ntri_common);
	*qset_id = ntri->ntri_ra.ras_qset_id;
	NXTR_RUNLOCK();
	return 0;
fail:
	NXTR_RUNLOCK();
	return err;
}

/*
 * Based on flow_pkt_classify().
 * This function populates struct ifnet_traffic_descriptor_inet instead of struct __flow.
 */
static int
fill_inet_td(struct __kern_packet *pkt, struct ifnet_traffic_descriptor_inet *td)
{
	union {
		volatile struct ip *__indexable _iph;
		volatile struct ip6_hdr *__indexable _ip6;
	} _l3;
	#define iph _l3._iph
	#define ip6 _l3._ip6
	union {
		volatile struct tcphdr *_tcph;
		volatile struct udphdr *_udph;
	} _l4;
	#define tcph _l4._tcph
	#define udph _l4._udph
	uint8_t *pkt_buf, *l3_hdr;
	uint32_t bdlen, bdlim, bdoff, cls_len;
	size_t pkt_len;
	uint8_t ipv, l3hlen = 0; /* IP header length */
	uint16_t l3tlen = 0;     /* total length of IP packet */
	uint8_t l4hlen = 0;      /* TCP/UDP header length */
	uint16_t ulen = 0;       /* user data length */
	int err;

	ASSERT(pkt->pkt_l2_len <= pkt->pkt_length);
	pkt_len = pkt->pkt_length - pkt->pkt_l2_len;

	MD_BUFLET_ADDR_ABS_DLEN(pkt, pkt_buf, bdlen, bdlim, bdoff);
	cls_len = bdlim - bdoff;
	cls_len -= pkt->pkt_l2_len;
	cls_len = (uint32_t)MIN(cls_len, pkt_len);
	VERIFY(pkt_len >= cls_len);
	if (cls_len == 0) {
		SK_ERR("cls_len == 0");
		err = EINVAL;
		goto fail;
	}
	l3_hdr = pkt_buf + pkt->pkt_headroom + pkt->pkt_l2_len;
	iph = (volatile struct ip *)(void *)l3_hdr;
	ipv = iph->ip_v;

	switch (ipv) {
	case 4:
		if (cls_len < sizeof(struct ip)) {
			SK_ERR("cls_len < sizeof(struct ip) (%d < %lu)",
			    cls_len, sizeof(struct ip));
			err = EINVAL;
			goto fail;
		}
		l3hlen = (uint8_t)(iph->ip_hl << 2);
		if (l3hlen < sizeof(struct ip)) {
			SK_ERR("l3hlen < sizeof(struct ip) (%d < %lu)",
			    l3hlen, sizeof(struct ip));
			err = EINVAL;
			goto fail;
		}
		if (cls_len < l3hlen) {
			SK_ERR("cls_len < l3hlen (%d < %d)", cls_len, l3hlen);
			err = EINVAL;
			goto fail;
		}
		l3tlen = ntohs(iph->ip_len);
		if (l3tlen < l3hlen) {
			SK_ERR("l3tlen < l3hlen (%d < %d)", l3tlen, l3hlen);
			err = EINVAL;
			goto fail;
		}
		if (pkt_len < l3tlen) {
			SK_ERR("pkt_len < l3tlen (%zu < %d)", pkt_len, l3tlen);
			err = EINVAL;
			goto fail;
		}
		td->inet_ipver = IPVERSION;
		td->inet_proto = iph->ip_p;
		bcopy(__DECONST(void *, &iph->ip_src), &td->inet_laddr.iia_v4addr,
		    sizeof(iph->ip_src));
		bcopy(__DECONST(void *, &iph->ip_dst), &td->inet_raddr.iia_v4addr,
		    sizeof(iph->ip_dst));
		break;
	case 6:
		l3hlen = sizeof(struct ip6_hdr);
		if (cls_len < l3hlen) {
			SK_ERR("cls_len < l3hlen (%d < %d)", cls_len, l3hlen);
			err = EINVAL;
			goto fail;
		}
		l3tlen = l3hlen + ntohs(ip6->ip6_plen);
		if (pkt_len < l3tlen) {
			SK_ERR("pkt_len < l3tlen (%zu < %d)", pkt_len, l3tlen);
			err = EINVAL;
			goto fail;
		}
		td->inet_ipver = IPV6_VERSION;
		td->inet_proto = ip6->ip6_nxt;
		bcopy(__DECONST(void *, &ip6->ip6_src), &td->inet_laddr,
		    sizeof(ip6->ip6_src));
		bcopy(__DECONST(void *, &ip6->ip6_dst), &td->inet_raddr,
		    sizeof(ip6->ip6_dst));
		break;
	default:
		SK_ERR("ipv == %d", ipv);
		err = EINVAL;
		goto fail;
	}
	tcph = __DECONST(volatile struct tcphdr *, (volatile uint8_t *)iph + l3hlen);
	ulen = (l3tlen - l3hlen);
	if (td->inet_proto == IPPROTO_TCP) {
		if (cls_len < l3hlen + sizeof(*tcph) || ulen < sizeof(*tcph)) {
			SK_ERR("cls_len < l3hlen + sizeof(*tcph) || ulen < sizeof(*tcph) "
			    "(%d < %d + %lu || %d < %lu)", cls_len, l3hlen, sizeof(*tcph),
			    ulen, sizeof(*tcph));
			err = EINVAL;
			goto fail;
		}
		l4hlen = (uint8_t)(tcph->th_off << 2);
		if (l4hlen < sizeof(*tcph)) {
			SK_ERR("l4hlen < sizeof(*tcph) (%d < %lu)", l4hlen, sizeof(*tcph));
			err = EINVAL;
			goto fail;
		}
		if (l4hlen > ulen) {
			SK_ERR("l4hlen > ulen (%d > %d)", l4hlen, ulen);
			err = EINVAL;
			goto fail;
		}
		bcopy(__DECONST(void *, &tcph->th_sport), &td->inet_lport,
		    sizeof(td->inet_lport));
		bcopy(__DECONST(void *, &tcph->th_dport), &td->inet_rport,
		    sizeof(td->inet_rport));
	} else if (td->inet_proto == IPPROTO_UDP) {
		if (cls_len < l3hlen + sizeof(*udph) || ulen < sizeof(*udph)) {
			SK_ERR("cls_len < l3hlen + sizeof(*udph) || ulen < sizeof(*udph) "
			    "(%d < %d + %lu || %d < %lu)", cls_len, l3hlen, sizeof(*udph),
			    ulen, sizeof(*udph));
			err = EINVAL;
			goto fail;
		}
		l4hlen = sizeof(*udph);
		if (l4hlen > ulen) {
			SK_ERR("l4hlen > ulen (%d > %d)", l4hlen, ulen);
			err = EINVAL;
			goto fail;
		}
		bcopy(__DECONST(void *, &udph->uh_sport), &td->inet_lport,
		    sizeof(td->inet_lport));
		bcopy(__DECONST(void *, &udph->uh_dport), &td->inet_rport,
		    sizeof(td->inet_rport));
	} else {
		err = ENOTSUP;
		goto fail;
	}

	td->inet_common.itd_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_INET;
	td->inet_common.itd_len = sizeof(*td);
	td->inet_common.itd_flags = IFNET_TRAFFIC_DESCRIPTOR_FLAG_INBOUND |
	    IFNET_TRAFFIC_DESCRIPTOR_FLAG_OUTBOUND;
	td->inet_mask |= (IFNET_TRAFFIC_DESCRIPTOR_INET_IPVER |
	    IFNET_TRAFFIC_DESCRIPTOR_INET_PROTO |
	    IFNET_TRAFFIC_DESCRIPTOR_INET_LADDR |
	    IFNET_TRAFFIC_DESCRIPTOR_INET_RADDR |
	    IFNET_TRAFFIC_DESCRIPTOR_INET_LPORT |
	    IFNET_TRAFFIC_DESCRIPTOR_INET_RPORT);
	return 0;
fail:
	DTRACE_SKYWALK5(classify__failed, struct ip *, iph, size_t, pkt_len,
	    uint8_t, pkt->pkt_l2_len, struct ifnet_traffic_descriptor_inet *, td,
	    int, err);
	bzero(td, sizeof(*td));
	return err;
	#undef iph
	#undef ip6
	#undef tcph
	#undef udph
}

int
nxctl_inet_traffic_rule_find_qset_id_with_pkt(const char *ifname,
    struct __kern_packet *pkt, uint64_t *qset_id)
{
	struct ifnet_traffic_descriptor_inet td;
	int err;

	err = fill_inet_td(pkt, &td);
	if (err != 0) {
		return err;
	}
	return nxctl_inet_traffic_rule_find_qset_id(ifname, &td, qset_id);
}

static struct ifnet_ip_addr v6_zeros_addr = {0};
static boolean_t
inet_v6addr_cmp(struct ifnet_ip_addr *a1, struct ifnet_ip_addr *a2)
{
	return memcmp(a1, a2, sizeof(*a1)) == 0;
}

void
inet_traffic_rule_init(kern_allocation_name_t rule_tag)
{
	ASSERT(nxctl_traffic_rule_tag == NULL);
	nxctl_traffic_rule_tag = rule_tag;
}

int
inet_traffic_rule_validate(
	const char *ifname,
	struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra)
{
	char buf[IFNAMSIZ];
	int unit, i;
	struct ifnet_traffic_descriptor_inet *tdi;
	uint8_t mask = 0, ipver, proto;

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
	tdi = (struct ifnet_traffic_descriptor_inet *)td;
	for (i = 0; i < NINETRULEMASKS; i++) {
		if (tdi->inet_mask == nxctl_inet_traffic_rule_masks[i]) {
			mask = tdi->inet_mask;
			break;
		}
	}
	if (mask == 0) {
		SK_ERR("invalid inet mask: 0x%x", tdi->inet_mask);
		return EINVAL;
	}
	ipver = tdi->inet_ipver;
	if (ipver != IPVERSION && ipver != IPV6_VERSION) {
		SK_ERR("invalid inet ipver: 0x%x", ipver);
		return EINVAL;
	}
	proto = tdi->inet_proto;
	if (proto != IPPROTO_TCP && proto != IPPROTO_UDP) {
		SK_ERR("invalid inet proto: %d", proto);
		return EINVAL;
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_LADDR) != 0) {
		if (ipver == IPVERSION) {
			if (tdi->inet_laddr.iia_v4addr == INADDR_ANY) {
				SK_ERR("inet laddr v4 cannot be unspecified");
				return EINVAL;
			}
		} else {
			if (inet_v6addr_cmp(&tdi->inet_laddr, &v6_zeros_addr)) {
				SK_ERR("inet laddr v4 cannot be unspecified");
				return EINVAL;
			}
		}
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_RADDR) != 0) {
		if (ipver == IPVERSION) {
			if (tdi->inet_raddr.iia_v4addr == INADDR_ANY) {
				SK_ERR("inet raddr v6 cannot be unspecified");
				return EINVAL;
			}
		} else {
			if (inet_v6addr_cmp(&tdi->inet_raddr, &v6_zeros_addr)) {
				SK_ERR("inet raddr v6 cannot be unspecified");
				return EINVAL;
			}
		}
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_LPORT) != 0) {
		if (tdi->inet_lport == 0) {
			SK_ERR("inet lport cannot be unspecified");
			return EINVAL;
		}
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_RPORT) != 0) {
		if (tdi->inet_rport == 0) {
			SK_ERR("inet rport cannot be unspecified");
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
inet_traffic_rule_storage_create(void)
{
	rs = sk_alloc_type(struct nxctl_traffic_rule_inet_storage,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	SLIST_INIT(&rs->ris_if_list);
	rs->ris_count = 0;
	return;
}

SK_NO_INLINE_ATTRIBUTE
static void
inet_traffic_rule_storage_destroy(void)
{
	ASSERT(rs->ris_count == 0);
	ASSERT(SLIST_EMPTY(&rs->ris_if_list));
	sk_free_type(struct nxctl_traffic_rule_inet_storage, rs);
}

SK_NO_INLINE_ATTRIBUTE
static struct nxctl_traffic_rule_inet_if *
inet_traffic_rule_if_create(const char *ifname)
{
	struct nxctl_traffic_rule_inet_if *rif;
	int i;

	rif = sk_alloc_type(struct nxctl_traffic_rule_inet_if,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	for (i = 0; i < NINETRULEMASKS; i++) {
		SLIST_INIT(&rif->rii_lists[i]);
	}
	strlcpy(rif->rii_ifname, ifname, sizeof(rif->rii_ifname));
	rif->rii_count = 0;
	return rif;
}

SK_NO_INLINE_ATTRIBUTE
static void
inet_traffic_rule_if_destroy(struct nxctl_traffic_rule_inet_if *rif)
{
	int i;

	for (i = 0; i < NINETRULEMASKS; i++) {
		ASSERT(SLIST_EMPTY(&rif->rii_lists[i]));
	}
	ASSERT(rif->rii_count == 0);
	sk_free_type(struct nxctl_traffic_rule_inet_if, rif);
}

SK_NO_INLINE_ATTRIBUTE
static boolean_t
inet_traffic_rule_match(struct nxctl_traffic_rule_inet *ntri, const char *ifname,
    uint32_t flags, struct ifnet_traffic_descriptor_inet *tdi)
{
	struct nxctl_traffic_rule *ntr = (struct nxctl_traffic_rule *)ntri;
	struct ifnet_traffic_descriptor_inet *tdi0;
	uint8_t mask;
	boolean_t exact;

	VERIFY(strlcmp(ntr->ntr_ifname, ifname, sizeof(ntr->ntr_ifname)) == 0);
	tdi0 = &ntri->ntri_td;

	exact = ((flags & NTR_FIND_FLAG_EXACT) != 0);
	mask = tdi0->inet_mask & tdi->inet_mask;
	if (exact) {
		ASSERT(tdi0->inet_mask == tdi->inet_mask);
	}
	ASSERT((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_IPVER) != 0);
	if (tdi0->inet_ipver != tdi->inet_ipver) {
		DTRACE_SKYWALK2(ipver__mismatch,
		    uint8_t, tdi0->inet_ipver, uint8_t, tdi->inet_ipver);
		return FALSE;
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_PROTO) != 0 &&
	    tdi0->inet_proto != tdi->inet_proto) {
		DTRACE_SKYWALK2(proto__mismatch,
		    uint8_t, tdi0->inet_proto, uint8_t, tdi->inet_proto);
		return FALSE;
	}
	if (tdi0->inet_ipver == IPVERSION) {
		if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_LADDR) != 0 &&
		    tdi0->inet_laddr.iia_v4addr != tdi->inet_laddr.iia_v4addr) {
			DTRACE_SKYWALK2(v4laddr__mismatch,
			    in_addr_t, tdi0->inet_laddr.iia_v4addr,
			    in_addr_t, tdi->inet_laddr.iia_v4addr);
			return FALSE;
		}
		if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_RADDR) != 0 &&
		    tdi0->inet_raddr.iia_v4addr != tdi->inet_raddr.iia_v4addr) {
			DTRACE_SKYWALK2(v4raddr__mismatch,
			    in_addr_t, tdi0->inet_raddr.iia_v4addr,
			    in_addr_t, tdi->inet_raddr.iia_v4addr);
			return FALSE;
		}
	} else {
		ASSERT(tdi0->inet_ipver == IPV6_VERSION);
		if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_LADDR) != 0 &&
		    !inet_v6addr_cmp(&tdi0->inet_laddr, &tdi->inet_laddr)) {
			DTRACE_SKYWALK2(v6laddr__mismatch,
			    struct in6_addr *, &tdi0->inet_laddr,
			    struct in6_addr *, &tdi->inet_laddr);
			return FALSE;
		}
		if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_RADDR) != 0 &&
		    !inet_v6addr_cmp(&tdi0->inet_raddr, &tdi->inet_raddr)) {
			DTRACE_SKYWALK2(v6raddr__mismatch,
			    struct in6_addr *, &tdi0->inet_raddr,
			    struct in6_addr *, &tdi->inet_raddr);
			return FALSE;
		}
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_LPORT) != 0 &&
	    tdi0->inet_lport != tdi->inet_lport) {
		DTRACE_SKYWALK2(lport__mismatch,
		    uint8_t, tdi0->inet_lport, uint8_t, tdi->inet_lport);
		return FALSE;
	}
	if ((mask & IFNET_TRAFFIC_DESCRIPTOR_INET_RPORT) != 0 &&
	    tdi0->inet_rport != tdi->inet_rport) {
		DTRACE_SKYWALK2(rport__mismatch,
		    uint8_t, tdi0->inet_rport, uint8_t, tdi->inet_rport);
		return FALSE;
	}
	return TRUE;
}

int
inet_traffic_rule_find(const char *ifname,
    struct ifnet_traffic_descriptor_common *td, uint32_t flags,
    struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_inet *ntri = NULL;
	struct nxctl_traffic_rule_inet_if *rif;
	struct ifnet_traffic_descriptor_inet *tdi =
	    (struct ifnet_traffic_descriptor_inet *)td;
	int i;

	if (rs == NULL) {
		return ENOENT;
	}
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		if (strlcmp(rif->rii_ifname, ifname, sizeof(rif->rii_ifname)) != 0) {
			continue;
		}
		for (i = 0; i < NINETRULEMASKS; i++) {
			if ((flags & NTR_FIND_FLAG_EXACT) != 0 &&
			    tdi->inet_mask != nxctl_inet_traffic_rule_masks[i]) {
				continue;
			}
			SLIST_FOREACH(ntri, &rif->rii_lists[i], ntri_storage_link) {
				if (inet_traffic_rule_match(ntri, ifname, flags, tdi)) {
					*ntrp = (struct nxctl_traffic_rule *)ntri;
					return 0;
				}
			}
		}
	}
	return ENOENT;
}

int
inet_traffic_rule_find_by_uuid(
	uuid_t uuid, struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_inet *ntri;
	struct nxctl_traffic_rule *ntr;
	struct nxctl_traffic_rule_inet_if *rif;
	int i;

	if (rs == NULL) {
		return ENOENT;
	}
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		for (i = 0; i < NINETRULEMASKS; i++) {
			SLIST_FOREACH(ntri, &rif->rii_lists[i], ntri_storage_link) {
				ntr = &ntri->ntri_common;
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
inet_update_ifnet_traffic_rule_count(const char *ifname, uint32_t count)
{
	struct ifnet *ifp;

	ifp = ifunit_ref(ifname);
	if (ifp == NULL) {
		DTRACE_SKYWALK1(ifname__not__found, char *, ifname);
		return;
	}
	ifnet_update_inet_traffic_rule_count(ifp, count);
	ifnet_decr_iorefcnt(ifp);
}

void
inet_traffic_rule_link(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_inet_if *rif;
	struct nxctl_traffic_rule_inet *ntri =
	    (struct nxctl_traffic_rule_inet *)ntr;
	struct nxctl_traffic_rule_inet_head *list = NULL;
	int i;
	char *__null_terminated ntr_ifname = NULL;
	char *__null_terminated rii_ifname = NULL;

	if (rs == NULL) {
		inet_traffic_rule_storage_create();
	}
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		if (strbufcmp(rif->rii_ifname, ntr->ntr_ifname) == 0) {
			break;
		}
	}
	if (rif == NULL) {
		ntr_ifname = __unsafe_null_terminated_from_indexable(ntr->ntr_ifname);
		rif = inet_traffic_rule_if_create(ntr_ifname);
		SLIST_INSERT_HEAD(&rs->ris_if_list, rif, rii_link);
	}
	for (i = 0; i < NINETRULEMASKS; i++) {
		if (ntri->ntri_td.inet_mask ==
		    nxctl_inet_traffic_rule_masks[i]) {
			list = &rif->rii_lists[i];
			break;
		}
	}
	retain_traffic_rule(ntr);
	ASSERT(list != NULL);
	SLIST_INSERT_HEAD(list, ntri, ntri_storage_link);
	/* per-interface count */
	rif->rii_count++;
	rii_ifname = __unsafe_null_terminated_from_indexable(rif->rii_ifname);
	inet_update_ifnet_traffic_rule_count(rii_ifname, rif->rii_count);

	/* global count */
	rs->ris_count++;
}

void
inet_traffic_rule_unlink(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_inet_if *rif;
	struct nxctl_traffic_rule_inet *ntri =
	    (struct nxctl_traffic_rule_inet *)ntr;
	struct nxctl_traffic_rule_inet_head *list = NULL;
	int i;
	char *__null_terminated rii_ifname = NULL;

	ASSERT(rs != NULL);
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		if (strbufcmp(rif->rii_ifname, ntr->ntr_ifname) == 0) {
			break;
		}
	}
	ASSERT(rif != NULL);
	for (i = 0; i < NINETRULEMASKS; i++) {
		if (ntri->ntri_td.inet_mask ==
		    nxctl_inet_traffic_rule_masks[i]) {
			list = &rif->rii_lists[i];
			break;
		}
	}
	ASSERT(list != NULL);
	SLIST_REMOVE(list, ntri, nxctl_traffic_rule_inet, ntri_storage_link);
	rif->rii_count--;
	rii_ifname = __unsafe_null_terminated_from_indexable(rif->rii_ifname);
	inet_update_ifnet_traffic_rule_count(rii_ifname, rif->rii_count);

	rs->ris_count--;
	release_traffic_rule(ntr);

	if (rif->rii_count == 0) {
		SLIST_REMOVE(&rs->ris_if_list, rif, nxctl_traffic_rule_inet_if, rii_link);
		inet_traffic_rule_if_destroy(rif);
	}
	if (rs->ris_count == 0) {
		inet_traffic_rule_storage_destroy();
	}
}

/*
 * XXX
 * This may need additional changes to ensure safety against detach/attach.
 * This is not an issue for the first consumer of llink interfaces, cellular,
 * which does not detach.
 */
int
inet_traffic_rule_notify(struct nxctl_traffic_rule *ntr, uint32_t flags)
{
	struct ifnet *ifp;
	struct nx_netif *nif;
	struct netif_qset *__single qset = NULL;
	struct nxctl_traffic_rule_inet *ntri;
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
	ntri = (struct nxctl_traffic_rule_inet *)ntr;
	qset = nx_netif_find_qset(nif, ntri->ntri_ra.ras_qset_id);
	if (qset == NULL || (qset->nqs_flags & NETIF_QSET_FLAG_EXT_INITED) == 0) {
		DTRACE_SKYWALK1(qset__not__initialized, struct netif_qset *, qset);
		err = ENXIO;
		goto done;
	}
	err = nx_netif_notify_steering_info(nif, qset,
	    (struct ifnet_traffic_descriptor_common *)&ntri->ntri_td,
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
inet_traffic_rule_get_count(const char *ifname, uint32_t *count)
{
	struct nxctl_traffic_rule_inet_if *rif;
	int err;

	if (rs == NULL) {
		err = ENOENT;
		goto fail;
	}
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		if (strlcmp(rif->rii_ifname, ifname, sizeof(rif->rii_ifname)) == 0) {
			break;
		}
	}
	if (rif == NULL) {
		err = ENOENT;
		goto fail;
	}
	*count = rif->rii_count;
	return 0;
fail:
	return err;
}

int
inet_traffic_rule_create(
	const char *ifname, struct ifnet_traffic_descriptor_common *td,
	struct ifnet_traffic_rule_action *ra, uint32_t flags,
	struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_inet *ntri;
	struct nxctl_traffic_rule *ntr;
	struct ifnet_traffic_descriptor_inet *tdi;
	struct ifnet_traffic_rule_action_steer *ras;

	ntri = sk_alloc_type(struct nxctl_traffic_rule_inet,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	ntr = &ntri->ntri_common;

	ntr->ntrt_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_INET;
	ntr->ntr_flags = flags;
	uuid_generate(ntr->ntr_uuid);
	os_ref_init(&ntr->ntr_refcnt, NULL);

	strlcpy(ntr->ntr_ifname, ifname, sizeof(ntr->ntr_ifname));
	proc_selfname(ntr->ntr_procname, sizeof(ntr->ntr_procname));

	tdi = __container_of(td, struct ifnet_traffic_descriptor_inet, inet_common);
	ras = __container_of(ra, struct ifnet_traffic_rule_action_steer, ras_common);
	bcopy(tdi, &ntri->ntri_td, sizeof(ntri->ntri_td));
	bcopy(ras, &ntri->ntri_ra, sizeof(ntri->ntri_ra));

	*ntrp = ntr;
	return 0;
}

void
inet_traffic_rule_destroy(struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_inet *ntri;

	ASSERT(os_ref_get_count(&ntr->ntr_refcnt) == 0);
	ntri = (struct nxctl_traffic_rule_inet *)ntr;
	sk_free_type(struct nxctl_traffic_rule_inet, ntri);
}

static void
convert_ntri_to_iocinfo(struct nxctl_traffic_rule_inet *ntri,
    struct nxctl_traffic_rule_inet_iocinfo *info)
{
	struct nxctl_traffic_rule *ntr;
	struct nxctl_traffic_rule_generic_iocinfo *ginfo;

	bzero(info, sizeof(*info));
	ntr = &ntri->ntri_common;
	ginfo = &info->tri_common;
	static_assert(sizeof(ntr->ntr_procname) == sizeof(ginfo->trg_procname));
	static_assert(sizeof(ntr->ntr_ifname) == sizeof(ginfo->trg_ifname));
	uuid_copy(ginfo->trg_uuid, ntr->ntr_uuid);
	strbufcpy(ginfo->trg_procname, ntr->ntr_procname);
	strbufcpy(ginfo->trg_ifname, ntr->ntr_ifname);
	bcopy(&ntri->ntri_td, &info->tri_td, sizeof(info->tri_td));
	bcopy(&ntri->ntri_ra, &info->tri_ra, sizeof(info->tri_ra));
}

int
inet_traffic_rule_get_all(uint32_t size,
    uint32_t *count, user_addr_t uaddr)
{
	struct nxctl_traffic_rule_inet *ntri = NULL;
	struct nxctl_traffic_rule_inet_if *rif;
	struct nxctl_traffic_rule_inet_iocinfo info;
	int i, err;

	if (size != sizeof(info)) {
		SK_ERR("size: actual %u, expected %lu", size, sizeof(info));
		return EINVAL;
	}
	if (rs == NULL) {
		*count = 0;
		return 0;
	}
	if (*count < rs->ris_count) {
		SK_ERR("count: given %d, require: %d", *count, rs->ris_count);
		return ENOBUFS;
	}
	SLIST_FOREACH(rif, &rs->ris_if_list, rii_link) {
		for (i = 0; i < NINETRULEMASKS; i++) {
			SLIST_FOREACH(ntri, &rif->rii_lists[i], ntri_storage_link) {
				convert_ntri_to_iocinfo(ntri, &info);
				err = copyout(&info, uaddr, sizeof(info));
				if (err != 0) {
					SK_ERR("copyout failed: %d", err);
					return err;
				}
				uaddr += sizeof(info);
			}
		}
	}
	*count = rs->ris_count;
	return 0;
}
