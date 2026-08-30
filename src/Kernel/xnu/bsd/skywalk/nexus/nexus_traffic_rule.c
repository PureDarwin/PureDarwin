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
#include <skywalk/nexus/nexus_traffic_rule_eth.h>

static struct nxctl_traffic_rule_type nxctl_rule_types[] = {
	{
		.ntrt_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_INET,
		.ntrt_validate = inet_traffic_rule_validate,
		.ntrt_find = inet_traffic_rule_find,
		.ntrt_find_by_uuid = inet_traffic_rule_find_by_uuid,
		.ntrt_link = inet_traffic_rule_link,
		.ntrt_unlink = inet_traffic_rule_unlink,
		.ntrt_notify = inet_traffic_rule_notify,
		.ntrt_create = inet_traffic_rule_create,
		.ntrt_destroy = inet_traffic_rule_destroy,
		.ntrt_get_all = inet_traffic_rule_get_all,
		.ntrt_get_count = inet_traffic_rule_get_count,
	},
	{
		.ntrt_type = IFNET_TRAFFIC_DESCRIPTOR_TYPE_ETH,
		.ntrt_validate = eth_traffic_rule_validate,
		.ntrt_find = eth_traffic_rule_find,
		.ntrt_find_by_uuid = eth_traffic_rule_find_by_uuid,
		.ntrt_link = eth_traffic_rule_link,
		.ntrt_unlink = eth_traffic_rule_unlink,
		.ntrt_notify = eth_traffic_rule_notify,
		.ntrt_create = eth_traffic_rule_create,
		.ntrt_destroy = eth_traffic_rule_destroy,
		.ntrt_get_all = eth_traffic_rule_get_all,
		.ntrt_get_count = eth_traffic_rule_get_count,
	},
};
#define NRULETYPES \
    (sizeof(nxctl_rule_types)/sizeof(struct nxctl_traffic_rule_type))

/* Per-fd list kept at the nxctl */
SLIST_HEAD(nxctl_traffic_rule_head, nxctl_traffic_rule);
struct nxctl_traffic_rule_storage {
	struct nxctl_traffic_rule_head rs_list;
	uint32_t rs_count;
};

static LCK_RW_DECLARE_ATTR(nxctl_traffic_rule_lock, &sk_lock_group, &sk_lock_attr);

SK_INLINE_ATTRIBUTE
void
nxtr_wlock(void)
{
	lck_rw_lock_exclusive(&nxctl_traffic_rule_lock);
}

SK_INLINE_ATTRIBUTE
void
nxtr_wunlock(void)
{
	lck_rw_unlock_exclusive(&nxctl_traffic_rule_lock);
}

SK_INLINE_ATTRIBUTE
void
nxtr_rlock(void)
{
	lck_rw_lock_shared(&nxctl_traffic_rule_lock);
}

SK_INLINE_ATTRIBUTE
void
nxtr_runlock(void)
{
	lck_rw_unlock_shared(&nxctl_traffic_rule_lock);
}

static struct nxctl_traffic_rule_type *find_traffic_rule_type(uint8_t type);
static int remove_traffic_rule(struct nxctl *nxctl, uuid_t uuid,
    struct nxctl_traffic_rule **ntrp);
static int notify_traffic_rule(struct nxctl_traffic_rule *ntr, uint32_t flags);

#define NXCTL_TRAFFIC_RULE_TAG "com.apple.skywalk.nexus.traffic_rule"
static kern_allocation_name_t nxctl_traffic_rule_tag;
static struct nxctl_traffic_rule_type *inet_traffic_rule_type = NULL;
static struct nxctl_traffic_rule_type *eth_traffic_rule_type = NULL;

void
nxctl_traffic_rule_init(void)
{
	ASSERT(nxctl_traffic_rule_tag == NULL);
	nxctl_traffic_rule_tag =
	    kern_allocation_name_allocate(NXCTL_TRAFFIC_RULE_TAG, 0);
	ASSERT(nxctl_traffic_rule_tag != NULL);

	ASSERT(inet_traffic_rule_type == NULL);
	inet_traffic_rule_type =
	    find_traffic_rule_type(IFNET_TRAFFIC_DESCRIPTOR_TYPE_INET);
	ASSERT(inet_traffic_rule_type != NULL);

	ASSERT(eth_traffic_rule_type == NULL);
	eth_traffic_rule_type =
	    find_traffic_rule_type(IFNET_TRAFFIC_DESCRIPTOR_TYPE_ETH);
	ASSERT(eth_traffic_rule_type != NULL);

	inet_traffic_rule_init(nxctl_traffic_rule_tag);
	eth_traffic_rule_init(nxctl_traffic_rule_tag);
}

void
nxctl_traffic_rule_fini(void)
{
	if (nxctl_traffic_rule_tag != NULL) {
		kern_allocation_name_release(nxctl_traffic_rule_tag);
		nxctl_traffic_rule_tag = NULL;
	}
	inet_traffic_rule_type = NULL;
	eth_traffic_rule_type = NULL;
}

SK_NO_INLINE_ATTRIBUTE
static struct nxctl_traffic_rule_storage *
nxctl_traffic_rule_storage_create(void)
{
	struct nxctl_traffic_rule_storage *rs;

	rs = sk_alloc_type(struct nxctl_traffic_rule_storage,
	    Z_WAITOK | Z_NOFAIL, nxctl_traffic_rule_tag);
	SLIST_INIT(&rs->rs_list);
	rs->rs_count = 0;
	return rs;
}

SK_NO_INLINE_ATTRIBUTE
static void
nxctl_traffic_rule_storage_destroy(struct nxctl_traffic_rule_storage *rs)
{
	ASSERT(rs->rs_count == 0);
	ASSERT(SLIST_EMPTY(&rs->rs_list));
	sk_free_type(struct nxctl_traffic_rule_storage, rs);
}

/*
 * This is meant to be called during closure of the nxctl's fd.
 * This will cleanup all rules linked to this nxctl. Rules that
 * are marked persistent won't be added to the nxctl list.
 */
void
nxctl_traffic_rule_clean(struct nxctl *nxctl)
{
	struct nxctl_traffic_rule_storage *rs;
	struct nxctl_traffic_rule *ntr, *next;
	int err;

	lck_mtx_lock(&nxctl->nxctl_lock);
	if ((rs = nxctl->nxctl_traffic_rule_storage) == NULL) {
		lck_mtx_unlock(&nxctl->nxctl_lock);
		return;
	}
	ntr = SLIST_FIRST(&rs->rs_list);
	SLIST_INIT(&rs->rs_list);
	rs->rs_count = 0;
	nxctl_traffic_rule_storage_destroy(rs);
	nxctl->nxctl_traffic_rule_storage = NULL;
	lck_mtx_unlock(&nxctl->nxctl_lock);

	while (ntr != NULL) {
		next = SLIST_NEXT(ntr, ntr_storage_link);
		/*
		 * Clearing the flag to tell remove_traffic_rule() not to
		 * remove from the nxctl list again.
		 */
		ntr->ntr_flags &= ~NTR_FLAG_ON_NXCTL_LIST;

		/* Passing NULL because we already hold a reference */
		err = remove_traffic_rule(nxctl, ntr->ntr_uuid, NULL);
		if (err == 0) {
			(void) notify_traffic_rule(ntr, NTR_NOTIFY_FLAG_REMOVE);
		}
		release_traffic_rule(ntr);
		ntr = next;
	}
}

SK_NO_INLINE_ATTRIBUTE
static void
add_traffic_rule_to_nxctl(struct nxctl *nxctl, struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_storage *rs;

	lck_mtx_lock(&nxctl->nxctl_lock);
	if ((rs = nxctl->nxctl_traffic_rule_storage) == NULL) {
		rs = nxctl_traffic_rule_storage_create();
		nxctl->nxctl_traffic_rule_storage = rs;
	}
	ntr->ntr_flags |= NTR_FLAG_ON_NXCTL_LIST;
	retain_traffic_rule(ntr);
	SLIST_INSERT_HEAD(&rs->rs_list, ntr, ntr_storage_link);
	rs->rs_count++;
	lck_mtx_unlock(&nxctl->nxctl_lock);
}

SK_NO_INLINE_ATTRIBUTE
static void
remove_traffic_rule_from_nxctl(struct nxctl *nxctl,
    struct nxctl_traffic_rule *ntr)
{
	struct nxctl_traffic_rule_storage *rs;

	lck_mtx_lock(&nxctl->nxctl_lock);
	if ((ntr->ntr_flags & NTR_FLAG_ON_NXCTL_LIST) == 0) {
		lck_mtx_unlock(&nxctl->nxctl_lock);
		return;
	}
	rs = nxctl->nxctl_traffic_rule_storage;
	SLIST_REMOVE(&rs->rs_list, ntr, nxctl_traffic_rule, ntr_storage_link);
	rs->rs_count--;
	ntr->ntr_flags &= ~NTR_FLAG_ON_NXCTL_LIST;
	release_traffic_rule(ntr);
	if (rs->rs_count == 0) {
		nxctl_traffic_rule_storage_destroy(rs);
		nxctl->nxctl_traffic_rule_storage = NULL;
	}
	lck_mtx_unlock(&nxctl->nxctl_lock);
}

SK_NO_INLINE_ATTRIBUTE
void
retain_traffic_rule(struct nxctl_traffic_rule *ntr)
{
#if (DEVELOPMENT || DEBUG)
	os_ref_count_t count = os_ref_get_count(&ntr->ntr_refcnt);
	DTRACE_SKYWALK2(ntr__retain, struct nxctl_traffic_rule *, ntr,
	    os_ref_count_t, count);
#endif
	os_ref_retain(&ntr->ntr_refcnt);
}

SK_NO_INLINE_ATTRIBUTE
void
release_traffic_rule(struct nxctl_traffic_rule *ntr)
{
#if (DEVELOPMENT || DEBUG)
	os_ref_count_t count = os_ref_get_count(&ntr->ntr_refcnt);
	DTRACE_SKYWALK2(ntr__release, struct nxctl_traffic_rule *, ntr,
	    os_ref_count_t, count);
#endif
	if (os_ref_release(&ntr->ntr_refcnt) == 0) {
		struct nxctl_traffic_rule_type *type;

		type = find_traffic_rule_type(ntr->ntrt_type);
		ASSERT(type);

		type->ntrt_destroy(ntr);
	}
}

SK_NO_INLINE_ATTRIBUTE
static int
notify_traffic_rule(struct nxctl_traffic_rule *ntr, uint32_t flags)
{
	struct nxctl_traffic_rule_type *type;

	type = find_traffic_rule_type(ntr->ntrt_type);
	ASSERT(type);

	return type->ntrt_notify(ntr, flags);
}

static void
link_traffic_rule(struct nxctl *nxctl, struct nxctl_traffic_rule *ntr)
{
	/*
	 * The persist flag means: do not clean up rule upon nxctl fd close.
	 * This means we only add the rule to the nxctl list if persist
	 * is not set.
	 */
	if ((ntr->ntr_flags & NTR_FLAG_PERSIST) == 0) {
		add_traffic_rule_to_nxctl(nxctl, ntr);
	}

	struct nxctl_traffic_rule_type *type;

	type = find_traffic_rule_type(ntr->ntrt_type);
	ASSERT(type);

	type->ntrt_link(ntr);
}

static void
unlink_traffic_rule(struct nxctl *nxctl, struct nxctl_traffic_rule *ntr)
{
	if ((ntr->ntr_flags & NTR_FLAG_PERSIST) == 0) {
		remove_traffic_rule_from_nxctl(nxctl, ntr);
	}

	struct nxctl_traffic_rule_type *type;

	type = find_traffic_rule_type(ntr->ntrt_type);
	ASSERT(type);

	type->ntrt_unlink(ntr);
}

static int
find_traffic_rule_by_uuid(uuid_t uuid, struct nxctl_traffic_rule **ntrp)
{
	int i, err;
	struct nxctl_traffic_rule_type *ntrt;
	struct nxctl_traffic_rule *__single ntr = NULL;

	for (i = 0; i < NRULETYPES; i++) {
		ntrt = &nxctl_rule_types[i];
		err = ntrt->ntrt_find_by_uuid(uuid, &ntr);
		if (err == 0) {
			ASSERT(ntr != NULL);
			*ntrp = ntr;
			return 0;
		}
	}
	return ENOENT;
}

static struct nxctl_traffic_rule_type *
find_traffic_rule_type(uint8_t type)
{
	int i;
	struct nxctl_traffic_rule_type *ntrt;

	for (i = 0; i < NRULETYPES; i++) {
		ntrt = &nxctl_rule_types[i];
		if (ntrt->ntrt_type == type) {
			return ntrt;
		}
	}
	return NULL;
}

SK_NO_INLINE_ATTRIBUTE
static int
add_traffic_rule(struct nxctl *nxctl, const char *ifname,
    struct ifnet_traffic_descriptor_common *td,
    struct ifnet_traffic_rule_action *ra,
    uint32_t flags,
    struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule_type *type = NULL;
	struct nxctl_traffic_rule *__single ntr = NULL;
	int err;

	NXTR_WLOCK();
	type = find_traffic_rule_type(td->itd_type);
	if (type == NULL) {
		SK_ERR("rule type %x not found", td->itd_type);
		err = EINVAL;
		goto fail;
	}
	for (int i = 0; i < NRULETYPES; i++) {
		if (&nxctl_rule_types[i] != type) {
			uint32_t count = 0;
			err = nxctl_rule_types[i].ntrt_get_count(ifname, &count);
			if (!(err == ENOENT || (err == 0 && count == 0))) {
				SK_ERR("other types of rules are added to the same ifname");
				err = EINVAL;
				goto fail;
			}
		}
	}
	err = type->ntrt_validate(ifname, td, ra);
	if (err != 0) {
		SK_ERR("rule validate failed: %d", err);
		goto fail;
	}
	err = type->ntrt_find(ifname, td, NTR_FIND_FLAG_EXACT, &ntr);
	if (err == 0) {
		SK_ERR("rule already exists");
		ASSERT(ntr != NULL);
		err = EEXIST;
		goto fail;
	} else if (err != ENOENT) {
		SK_ERR("rule find failed: %d", err);
		goto fail;
	}
	err = type->ntrt_create(ifname, td, ra, flags, &ntr);
	if (err != 0) {
		SK_ERR("rule create failed: %d", err);
		goto fail;
	}
	link_traffic_rule(nxctl, ntr);
	if (ntrp != NULL) {
		retain_traffic_rule(ntr);
		*ntrp = ntr;
	}
	NXTR_WUNLOCK();
	return 0;
fail:
	NXTR_WUNLOCK();
	return err;
}


SK_NO_INLINE_ATTRIBUTE
static int
remove_traffic_rule(struct nxctl *nxctl, uuid_t uuid,
    struct nxctl_traffic_rule **ntrp)
{
	struct nxctl_traffic_rule *__single ntr;
	int err;

	NXTR_WLOCK();
	err = find_traffic_rule_by_uuid(uuid, &ntr);
	if (err != 0) {
		SK_ERR("traffic rule not found");
		NXTR_WUNLOCK();
		return err;
	}
	if (ntrp != NULL) {
		retain_traffic_rule(ntr);
		*ntrp = ntr;
	}
	unlink_traffic_rule(nxctl, ntr);
	/* release initial reference */
	release_traffic_rule(ntr);
	NXTR_WUNLOCK();
	return 0;
}

static uint32_t
convert_traffic_rule_ioc_flags(uint32_t flags)
{
	uint32_t f = 0;

	if ((flags & NXIOC_ADD_TRAFFIC_RULE_FLAG_PERSIST) != 0) {
		f |= NTR_FLAG_PERSIST;
	}
	return f;
}

SK_NO_INLINE_ATTRIBUTE
static int
add_traffic_rule_generic(struct nxctl *nxctl, const char *ifname,
    struct ifnet_traffic_descriptor_common *td,
    struct ifnet_traffic_rule_action *ra, uint32_t flags, uuid_t *uuid)
{
	struct nxctl_traffic_rule *__single ntr;
	int err;

	err = add_traffic_rule(nxctl, ifname, td, ra, flags, &ntr);
	if (err != 0) {
		return err;
	}
	(void) notify_traffic_rule(ntr, NTR_NOTIFY_FLAG_ADD);
	uuid_copy(*uuid, ntr->ntr_uuid);
	release_traffic_rule(ntr);
	return 0;
}

int
nxioctl_add_traffic_rule_inet(struct nxctl *nxctl, caddr_t data, proc_t procp)
{
#pragma unused(procp)
	struct nxctl_add_traffic_rule_inet_iocargs *args =
	    (struct nxctl_add_traffic_rule_inet_iocargs *)(void *)data;
	char *__null_terminated atri_ifname = NULL;

	atri_ifname = __unsafe_null_terminated_from_indexable(args->atri_ifname);

	return add_traffic_rule_generic(nxctl, atri_ifname,
	           &args->atri_td.inet_common,
	           &args->atri_ra.ras_common,
	           convert_traffic_rule_ioc_flags(args->atri_flags),
	           &args->atri_uuid);
}

int
nxioctl_add_traffic_rule_eth(struct nxctl *nxctl, caddr_t data, proc_t procp)
{
#pragma unused(procp)
	struct nxctl_add_traffic_rule_eth_iocargs *args =
	    (struct nxctl_add_traffic_rule_eth_iocargs *)(void *)data;
	char *__null_terminated atre_ifname = NULL;

	atre_ifname = __unsafe_null_terminated_from_indexable(args->atre_ifname);

	return add_traffic_rule_generic(nxctl, atre_ifname,
	           &args->atre_td.eth_common,
	           &args->atre_ra.ras_common,
	           convert_traffic_rule_ioc_flags(args->atre_flags),
	           &args->atre_uuid);
}

int
nxioctl_remove_traffic_rule(struct nxctl *nxctl, caddr_t data, proc_t procp)
{
#pragma unused(procp)
	struct nxctl_remove_traffic_rule_iocargs *args =
	    (struct nxctl_remove_traffic_rule_iocargs *)(void *)data;
	struct nxctl_traffic_rule *__single ntr;
	int err;

	err = remove_traffic_rule(nxctl, args->rtr_uuid, &ntr);
	if (err != 0) {
		return err;
	}
	(void) notify_traffic_rule(ntr, NTR_NOTIFY_FLAG_REMOVE);
	release_traffic_rule(ntr);
	return 0;
}

int
nxioctl_get_traffic_rules(struct nxctl *nxctl, caddr_t data, proc_t procp)
{
#pragma unused(nxctl)
	struct nxctl_get_traffic_rules_iocargs *args =
	    (struct nxctl_get_traffic_rules_iocargs *)(void *)data;
	struct nxctl_traffic_rule_type *type;
	user_addr_t uaddr;
	int err;

	NXTR_RLOCK();
	type = find_traffic_rule_type(args->gtr_type);
	if (type == NULL) {
		SK_ERR("rule type %x not found", args->gtr_type);
		err = EINVAL;
		goto fail;
	}
	uaddr = proc_is64bit(procp) ? args->gtr_buf64 :
	    CAST_USER_ADDR_T(args->gtr_buf);
	err = type->ntrt_get_all(args->gtr_size, &args->gtr_count, uaddr);
	if (err != 0) {
		goto fail;
	}
	NXTR_RUNLOCK();
	return 0;
fail:
	NXTR_RUNLOCK();
	return err;
}
