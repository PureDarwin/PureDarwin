/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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

#define __KPI__
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>
#include <sys/mcache.h>
#include <kern/kalloc.h>
#include <kern/locks.h>
#include <kern/lock_group.h>
#include <os/log.h>
#include <net/aop/kpi_aop.h>
#include <net/aop/aop_stats.h>
#include <libkern/libkern.h>
#include <netinet/tcp_cc.h>
#include <IOKit/IOBSD.h>

static LCK_GRP_DECLARE(kaop_lock_group, "net_aop");
static LCK_ATTR_DECLARE(kaop_lock_attr, 0, 0);
static LCK_MTX_DECLARE_ATTR(kaop_lock, &kaop_lock_group, &kaop_lock_attr);

#define KAOP_LOCK()                                      \
    lck_mtx_lock(&kaop_lock)
#define KAOP_LOCK_ASSERT_HELD()                          \
    LCK_MTX_ASSERT(&kaop_lock, LCK_MTX_ASSERT_OWNED)
#define KAOP_LOCK_ASSERT_NOTHELD()                       \
    LCK_MTX_ASSERT(&kaop_lock, LCK_MTX_ASSERT_NOTOWNED)
#define KAOP_UNLOCK()                                    \
    lck_mtx_unlock(&kaop_lock)

os_log_t kaop_log_handle = NULL;

#define _KAOPLOG(level, type, fmt, ...) do {    \
	os_log_with_type(kaop_log_handle, type, "%s - " fmt, __func__, ##__VA_ARGS__); \
} while(0)

#define KAOPLOG(fmt, ...)         _KAOPLOG(kaop_log_handle, OS_LOG_TYPE_DEFAULT, fmt, ##__VA_ARGS__)
#define KAOPLOG_DEBUG(fmt, ...)   _KAOPLOG(kaop_log_handle, OS_LOG_TYPE_DEBUG,   fmt, ##__VA_ARGS__)
#define KAOPLOG_INFO(fmt, ...)    _KAOPLOG(kaop_log_handle, OS_LOG_TYPE_INFO,    fmt, ##__VA_ARGS__)
#define KAOPLOG_ERR(fmt, ...)   _KAOPLOG(kaop_log_handle, OS_LOG_TYPE_ERROR,   fmt, ##__VA_ARGS__)

os_refgrp_decl(static, kaop_refgrp, "kaop_ref_group", NULL);

#define KAOP_DRIVER_STATS               (((uint32_t)1) << 1)
#define KAOP_PROC_ACTIVITY_BITMAPS      (((uint32_t)1) << 2)

#define KAOP_IP_STATS                   (((uint32_t)1) << 24)
#define KAOP_IP6_STATS                  (((uint32_t)1) << 25)
#define KAOP_TCP_STATS                  (((uint32_t)1) << 26)
#define KAOP_UDP_STATS                  (((uint32_t)1) << 27)

#define KAOP_PROTOCOL_STATS     (KAOP_IP_STATS | KAOP_IP6_STATS \
	                            | KAOP_TCP_STATS | KAOP_UDP_STATS)

/*
 * sysctl interfaces
 */
static int net_aop_stats_get_sysctl SYSCTL_HANDLER_ARGS;

SYSCTL_NODE(_net, OID_AUTO, aop, CTLFLAG_RW | CTLFLAG_LOCKED, 0, "AOP");
SYSCTL_PROC(_net_aop, OID_AUTO, driver_stats,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, KAOP_DRIVER_STATS,
    net_aop_stats_get_sysctl, "S,aop_driver_stats",
    "AOP driver statistics counter");
SYSCTL_PROC(_net_aop, OID_AUTO, protocol_stats,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, KAOP_PROTOCOL_STATS,
    net_aop_stats_get_sysctl, "S,net_aop_protocol_stats",
    "AOP protocol statistics counter");
SYSCTL_PROC(_net_aop, OID_AUTO, proc_activity_bitmaps,
    CTLTYPE_STRUCT | CTLFLAG_RD | CTLFLAG_LOCKED, 0, KAOP_PROC_ACTIVITY_BITMAPS,
    net_aop_stats_get_sysctl, "S,aop_proc_activity_bitmap",
    "AOP process activity bitmaps");

#define KAOP_CAPAB_FLOW_SETUP      0x00000001
#define KAOP_CAPAB_FLOW_STATS      0x00000002
#define KAOP_CAPAB_STATS           0x00000004
#define KAOP_CAPAB_PROC_ACTIVITY_BITMAPS    0x00000008

struct net_aop_flow_setup {
	net_aop_flow_setup_fn_t fsp_flow_setup;
	void *fsp_prov_ctx;
};

struct net_aop_flow_stats {
	net_aop_flow_stats_fn_t fs_flow_stats;
	void *fs_prov_ctx;
};

struct net_aop_stats {
	net_aop_stats_fn_t gs_stats;
	void *gs_prov_ctx;
};

struct net_aop_proc_activity_bitmaps {
	net_aop_proc_activity_bitmap_fn_t pab_activity_bitmap;
	void *pab_prov_ctx;
};

#define KAOP_FLAG_ATTACHED  0x00000001

struct net_aop_provider_handle {
	struct net_aop_provider_init         kaop_ext;
	void                                 *kaop_prov_ctx;
	struct net_aop_flow_setup            kaop_fsp;
	struct net_aop_flow_stats            kaop_fs;
	struct net_aop_stats                 kaop_gs;
	struct net_aop_proc_activity_bitmaps kaop_pb;
	uint32_t                             kaop_capabilities;
	uint32_t                             kaop_flags;
	os_refcnt_t                          kaop_refcnt;
};

static struct net_aop_provider_handle g_aop_net_provider;

static errno_t
net_aop_validate_init_params(
	const struct net_aop_provider_init *init, const uint32_t init_len)
{
	errno_t err = 0;

	static_assert(__builtin_offsetof(struct net_aop_provider_init, kaopi_version) == 0);
	static_assert(sizeof(init->kaopi_version) == sizeof(uint32_t));

	if (init == NULL) {
		KAOPLOG_ERR("init is null");
		return EINVAL;
	}

	if (init_len < sizeof(uint32_t)) {
		KAOPLOG_ERR("init_len[%u] < sizeof(uint32_t)", init_len);
		return EINVAL;
	}

	switch (init->kaopi_version) {
	case NET_AOP_VERSION_1:
		if (init_len != sizeof(struct net_aop_provider_init)) {
			KAOPLOG_ERR("init_len[%u] != sizeof(struct net_aop_provider_init", init_len);
			err = EINVAL;
			break;
		}
		if (init->kaopi_config_capab == NULL) {
			KAOPLOG_ERR("kaopi_config_capab is null");
			err = EINVAL;
			break;
		}
		break;
	default:
		KAOPLOG_ERR("invalid version[%u]", init->kaopi_version);
		err = EINVAL;
		break;
	}

	return err;
}

static int
configure_capab_flow_setup(net_aop_provider_handle_t prov,
    net_aop_config_fn_t capab_fn)
{
	struct net_aop_capab_flow_setup capab;
	uint32_t capab_len;
	int error;

	bzero(&capab, sizeof(capab));
	capab.kaopcfsp_version = NET_AOP_CAPAB_FLOW_SETUP_VERSION_1;
	capab_len = sizeof(capab);

	error = capab_fn(prov->kaop_prov_ctx, NET_AOP_CAPAB_FLOW_SETUP,
	    &capab, &capab_len);
	if (error != 0) {
		KAOPLOG_ERR("Failed to get flow setup capability for provider");
		return error;
	}

	VERIFY(capab.kaopcfsp_config != NULL);
	VERIFY(capab.kaopcfsp_prov_ctx != NULL);
	prov->kaop_fsp.fsp_prov_ctx = capab.kaopcfsp_prov_ctx;
	prov->kaop_fsp.fsp_flow_setup = capab.kaopcfsp_config;
	prov->kaop_capabilities |= KAOP_CAPAB_FLOW_SETUP;
	return 0;
}

static void
unconfigure_capab_flow_setup(net_aop_provider_handle_t prov)
{
	if ((prov->kaop_capabilities & KAOP_CAPAB_FLOW_SETUP) == 0) {
		return;
	}

	bzero(&prov->kaop_fsp, sizeof(prov->kaop_fsp));
	prov->kaop_capabilities &= ~KAOP_CAPAB_FLOW_SETUP;
}

static int
configure_capab_flow_stats(net_aop_provider_handle_t prov,
    net_aop_config_fn_t capab_fn)
{
	struct net_aop_capab_flow_stats capab;
	uint32_t capab_len;
	int error;

	bzero(&capab, sizeof(capab));
	capab.kaopcfs_version = NET_AOP_CAPAB_FLOW_STATS_VERSION_1;
	capab_len = sizeof(capab);
	error = capab_fn(prov->kaop_prov_ctx, NET_AOP_CAPAB_FLOW_STATS,
	    &capab, &capab_len);
	if (error != 0) {
		KAOPLOG_ERR("Failed to get flow stats capability for KAOP provider");
		return error;
	}

	VERIFY(capab.kaopcfs_config != NULL);
	VERIFY(capab.kaopcfs_prov_ctx != NULL);
	prov->kaop_fs.fs_prov_ctx = capab.kaopcfs_prov_ctx;
	prov->kaop_fs.fs_flow_stats = capab.kaopcfs_config;
	prov->kaop_capabilities |= KAOP_CAPAB_FLOW_STATS;
	return 0;
}

static void
unconfigure_capab_flow_stats(net_aop_provider_handle_t prov)
{
	if ((prov->kaop_capabilities & KAOP_CAPAB_FLOW_STATS) == 0) {
		return;
	}

	bzero(&prov->kaop_fs, sizeof(prov->kaop_fs));
	prov->kaop_capabilities &= ~KAOP_CAPAB_FLOW_STATS;
}

static int
configure_capab_stats(net_aop_provider_handle_t prov,
    net_aop_config_fn_t capab_fn)
{
	struct net_aop_capab_stats capab;
	uint32_t capab_len;
	int error;

	bzero(&capab, sizeof(capab));
	capab.kaopcgs_version = NET_AOP_CAPAB_STATS_VERSION_1;
	capab_len = sizeof(capab);
	error = capab_fn(prov->kaop_prov_ctx, NET_AOP_CAPAB_STATS,
	    &capab, &capab_len);
	if (error != 0) {
		KAOPLOG_ERR("Failed to get stats capability provider");
		return error;
	}

	VERIFY(capab.kaopcgs_config != NULL);
	VERIFY(capab.kaopcgs_prov_ctx != NULL);
	prov->kaop_gs.gs_prov_ctx = capab.kaopcgs_prov_ctx;
	prov->kaop_gs.gs_stats = capab.kaopcgs_config;
	prov->kaop_capabilities |= KAOP_CAPAB_STATS;
	return 0;
}

static void
unconfigure_capab_stats(net_aop_provider_handle_t prov)
{
	if ((prov->kaop_capabilities & KAOP_CAPAB_STATS) == 0) {
		return;
	}

	bzero(&prov->kaop_gs, sizeof(prov->kaop_gs));
	prov->kaop_capabilities &= ~KAOP_CAPAB_STATS;
}

static int
configure_capab_process_bitmaps(net_aop_provider_handle_t prov,
    net_aop_config_fn_t capab_fn)
{
	struct net_aop_capab_proc_activity_bitmap capab;
	uint32_t capab_len;
	int error;

	bzero(&capab, sizeof(capab));
	capab.kaopbm_version = NET_AOP_CAPAB_PROC_ACTIVITY_BITMAP_VERSION_1;
	capab_len = sizeof(capab);
	error = capab_fn(prov->kaop_prov_ctx, NET_AOP_CAPAB_PROC_ACTIVITY_BITMAP,
	    &capab, &capab_len);
	if (error != 0) {
		KAOPLOG_ERR("Failed to get proc bitmap capability provider");
		return error;
	}

	VERIFY(capab.kaopbm_config != NULL);
	VERIFY(capab.kaopbm_prov_ctx != NULL);
	prov->kaop_pb.pab_prov_ctx = capab.kaopbm_prov_ctx;
	prov->kaop_pb.pab_activity_bitmap = capab.kaopbm_config;
	prov->kaop_capabilities |= KAOP_CAPAB_PROC_ACTIVITY_BITMAPS;
	return 0;
}

static void
unconfigure_capab_process_bitmaps(net_aop_provider_handle_t prov)
{
	if ((prov->kaop_capabilities & KAOP_CAPAB_PROC_ACTIVITY_BITMAPS) == 0) {
		return;
	}

	bzero(&prov->kaop_pb, sizeof(prov->kaop_pb));
	prov->kaop_capabilities &= ~KAOP_CAPAB_PROC_ACTIVITY_BITMAPS;
}

static int
net_aop_provider_initialize(net_aop_provider_handle_t prov)
{
	net_aop_config_fn_t capab_fn = prov->kaop_ext.kaopi_config_capab;
	if (capab_fn == NULL) {
		KAOPLOG_ERR("kaop provider missing capability function");
		return EINVAL;
	}

	configure_capab_flow_setup(prov, capab_fn);
	configure_capab_flow_stats(prov, capab_fn);
	configure_capab_stats(prov, capab_fn);
	configure_capab_process_bitmaps(prov, capab_fn);
	return 0;
}

static void
net_aop_provider_cleanup(net_aop_provider_handle_t prov)
{
	KAOP_LOCK_ASSERT_HELD();

	prov->kaop_flags &= ~KAOP_FLAG_ATTACHED;

	if (os_ref_release(&prov->kaop_refcnt) != 0) {
		while (os_ref_get_count(&prov->kaop_refcnt) > 0) {
			msleep(&prov->kaop_refcnt,
			    &kaop_lock, (PZERO + 1), __FUNCTION__, NULL);
		}
	}

	unconfigure_capab_flow_setup(prov);
	unconfigure_capab_flow_stats(prov);
	unconfigure_capab_stats(prov);
	unconfigure_capab_process_bitmaps(prov);
	memset(&prov->kaop_ext, 0, sizeof(prov->kaop_ext));
	prov->kaop_prov_ctx = NULL;
}

static void
net_aop_release_refcnt(net_aop_provider_handle_t prov)
{
	KAOP_LOCK();
	if (os_ref_release(&prov->kaop_refcnt) == 0) {
		wakeup(&prov->kaop_refcnt);
	}
	KAOP_UNLOCK();
}

int
net_aop_setup_flow(uint32_t flow_id, bool add, uint32_t *stats_index)
{
	net_aop_flow_setup_fn_t fsp = NULL;
	void *__single fsp_ctx = NULL;
	int err = 0;

	if (stats_index == NULL) {
		KAOPLOG_ERR("invalid stats index param");
		return EINVAL;
	}

	KAOP_LOCK();
	if ((g_aop_net_provider.kaop_capabilities & KAOP_CAPAB_FLOW_SETUP) == 0) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kern aop provider does not support flow setup");
		return ENOTSUP;
	}

	if (!(g_aop_net_provider.kaop_flags & KAOP_FLAG_ATTACHED) ||
	    !os_ref_retain_try(&g_aop_net_provider.kaop_refcnt)) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kernel aop provider is not valid");
		return ENOENT;
	}

	fsp = g_aop_net_provider.kaop_fsp.fsp_flow_setup;
	fsp_ctx = g_aop_net_provider.kaop_fsp.fsp_prov_ctx;
	KAOP_UNLOCK();

	err = fsp(fsp_ctx, flow_id, add, stats_index);
	net_aop_release_refcnt(&g_aop_net_provider);
	return err;
}

int
net_aop_get_flow_stats(uint32_t stats_index, struct aop_flow_stats *flow_stats)
{
	net_aop_flow_stats_fn_t fs = NULL;
	void *__single fs_ctx = NULL;
	int err = 0;

	if (flow_stats == NULL) {
		KAOPLOG_ERR("invalid flow stats param");
		return EINVAL;
	}

	KAOP_LOCK();
	if ((g_aop_net_provider.kaop_capabilities & KAOP_CAPAB_FLOW_STATS) == 0) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kern aop provider does not support flow stats");
		return ENOTSUP;
	}

	if (!(g_aop_net_provider.kaop_flags & KAOP_FLAG_ATTACHED) ||
	    !os_ref_retain_try(&g_aop_net_provider.kaop_refcnt)) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kernel aop provider is not valid");
		return ENOENT;
	}

	fs = g_aop_net_provider.kaop_fs.fs_flow_stats;
	fs_ctx = g_aop_net_provider.kaop_fs.fs_prov_ctx;
	KAOP_UNLOCK();

	err = fs(fs_ctx, stats_index, flow_stats);
	net_aop_release_refcnt(&g_aop_net_provider);
	return err;
}

int
net_aop_get_stats(net_aop_stats_type_t type, uint8_t *__sized_by(stats_len) stats, size_t stats_len)
{
	net_aop_stats_fn_t gs = NULL;
	void *__single gs_ctx = NULL;
	int err = 0;

	if (type == NET_AOP_STATS_TYPE_INVALID ||
	    type > NET_AOP_STATS_TYPE_MAX) {
		KAOPLOG_ERR("invalid stats type %u", type);
		return EINVAL;
	}

	if (stats == NULL || stats_len == 0) {
		KAOPLOG_ERR("invalid stats param");
		return EINVAL;
	}

	KAOP_LOCK();
	if ((g_aop_net_provider.kaop_capabilities & KAOP_CAPAB_STATS) == 0) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kern aop provider does not support stats");
		return ENOTSUP;
	}

	if (!(g_aop_net_provider.kaop_flags & KAOP_FLAG_ATTACHED) ||
	    !os_ref_retain_try(&g_aop_net_provider.kaop_refcnt)) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kernel aop provider is not valid");
		return ENOENT;
	}

	gs = g_aop_net_provider.kaop_gs.gs_stats;
	gs_ctx = g_aop_net_provider.kaop_gs.gs_prov_ctx;
	KAOP_UNLOCK();

	err = gs(gs_ctx, type, stats, stats_len);
	net_aop_release_refcnt(&g_aop_net_provider);
	return err;
}

int
net_aop_get_proc_activity_bitmaps(struct aop_proc_activity_bitmap *proc_activity_bitmaps,
    uint16_t *inout_count)
{
	net_aop_proc_activity_bitmap_fn_t pb = NULL;
	void *__single pb_ctx = NULL;
	int err = 0;

	if (inout_count == NULL) {
		KAOPLOG_ERR("invalid inout_count param");
		return EINVAL;
	}

	KAOP_LOCK();
	if ((g_aop_net_provider.kaop_capabilities & KAOP_CAPAB_PROC_ACTIVITY_BITMAPS) == 0) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kern aop provider does not support proc bitmaps");
		return ENOTSUP;
	}

	if (!(g_aop_net_provider.kaop_flags & KAOP_FLAG_ATTACHED) ||
	    !os_ref_retain_try(&g_aop_net_provider.kaop_refcnt)) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kernel aop provider is not valid");
		return ENOENT;
	}

	pb = g_aop_net_provider.kaop_pb.pab_activity_bitmap;
	pb_ctx = g_aop_net_provider.kaop_pb.pab_prov_ctx;
	KAOP_UNLOCK();

	err = pb(pb_ctx, proc_activity_bitmaps, inout_count);
	net_aop_release_refcnt(&g_aop_net_provider);
	return err;
}

net_aop_provider_handle_t
net_aop_register_provider(const struct net_aop_provider_init *init,
    const uint32_t init_len, void *ctx)
{
	errno_t err = 0;

	err = net_aop_validate_init_params(init, init_len);
	if (err != 0) {
		return NULL;
	}

	KAOP_LOCK();
	if (g_aop_net_provider.kaop_flags & KAOP_FLAG_ATTACHED) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("kernel aop provider already registered");
		return NULL;
	}

	os_ref_init(&g_aop_net_provider.kaop_refcnt, &kaop_refgrp);
	memcpy(&g_aop_net_provider.kaop_ext, init, sizeof(g_aop_net_provider.kaop_ext));
	g_aop_net_provider.kaop_prov_ctx = ctx;

	err = net_aop_provider_initialize(&g_aop_net_provider);
	if (err != 0) {
		KAOP_UNLOCK();
		KAOPLOG_ERR("provider type failed to initialize");
		goto done;
	}

	g_aop_net_provider.kaop_flags |= KAOP_FLAG_ATTACHED;
	KAOP_UNLOCK();
done:
	KAOP_LOCK_ASSERT_NOTHELD();
	if (err != 0) {
		KAOP_LOCK();
		net_aop_provider_cleanup(&g_aop_net_provider);
		KAOP_UNLOCK();
		return NULL;
	}
	return &g_aop_net_provider;
}

void
net_aop_deregister_provider(net_aop_provider_handle_t prov)
{
	if (prov == NULL) {
		return;
	}

	KAOP_LOCK();
	ASSERT(prov->kaop_flags & KAOP_FLAG_ATTACHED);
	net_aop_provider_cleanup(prov);
	KAOP_UNLOCK();
	return;
}

static int
net_aop_get_protocol_stats(struct net_aop_protocol_stats *aop_proto_stats)
{
	int error = 0;

	error = net_aop_get_stats(NET_AOP_STATS_TYPE_IP,
	    (uint8_t *)&aop_proto_stats->aop_ip, sizeof(aop_proto_stats->aop_ip));
	if (error != 0) {
		return error;
	}
	error = net_aop_get_stats(NET_AOP_STATS_TYPE_IPV6,
	    (uint8_t *)&aop_proto_stats->aop_ip6, sizeof(aop_proto_stats->aop_ip6));
	if (error != 0) {
		return error;
	}
	error = net_aop_get_stats(NET_AOP_STATS_TYPE_TCP,
	    (uint8_t *)&aop_proto_stats->aop_tcp, sizeof(aop_proto_stats->aop_tcp));
	if (error != 0) {
		return error;
	}
	error = net_aop_get_stats(NET_AOP_STATS_TYPE_UDP,
	    (uint8_t *)&aop_proto_stats->aop_udp, sizeof(aop_proto_stats->aop_udp));
	if (error != 0) {
		return error;
	}

	return error;
}

static int
net_aop_get_driver_stats(struct aop_driver_stats *driver_stats)
{
	int error = 0;
	error = net_aop_get_stats(NET_AOP_STATS_TYPE_DRIVER,
	    (uint8_t *)(struct aop_driver_stats *__bidi_indexable)driver_stats, sizeof(struct aop_driver_stats));
	return error;
}

static int
aop_get_process_activity_bitmaps(struct aop_proc_activity_bitmap **bitmaps, size_t requested_buffer_space,
    size_t *out_len)
{
	size_t bitmap_size = 0;
	struct aop_proc_activity_bitmap *__sized_by(bitmap_size) proc_activity_bitmap = NULL;
	uint16_t proc_bitmap_count = 0;
	int err = 0;

	net_aop_get_proc_activity_bitmaps(NULL, &proc_bitmap_count);

	if (proc_bitmap_count > 0) {
		size_t requested_count = (requested_buffer_space / (sizeof(struct aop_proc_activity_bitmap)));
		requested_count = (requested_count > proc_bitmap_count) ? proc_bitmap_count : requested_count;

		size_t required_buffer_len = (requested_count * sizeof(struct aop_proc_activity_bitmap));
		proc_activity_bitmap = (struct aop_proc_activity_bitmap *)kalloc_data(required_buffer_len, Z_WAITOK | Z_ZERO);
		bitmap_size = required_buffer_len;
		if (proc_activity_bitmap == NULL) {
			return ENOBUFS;
		}

		err = net_aop_get_proc_activity_bitmaps(proc_activity_bitmap, (uint16_t *)&requested_count);
		if (err != 0) {
			kfree_data_sized_by(proc_activity_bitmap, bitmap_size);
			return err;
		}

		*bitmaps = proc_activity_bitmap;
		*out_len = required_buffer_len;
		return 0;
	}

	return ENOENT;
}

static int
net_aop_stats_get_sysctl SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	struct net_aop_protocol_stats proto_stats = {};
	struct aop_driver_stats driver_stats = {};
	struct proc *p = NULL;
	task_t __single task = NULL;
	size_t buffer_space;
	uint8_t *out_buffer = NULL;
	size_t out_size = 0;
	int error = 0;

	if (!kauth_cred_issuser(kauth_cred_get())) {
		p = current_proc();
		task = proc_task(p);
		bool has_aop_stats_entitlement = IOTaskHasEntitlement(task, "com.apple.private.network.aop_stats");
		if (!has_aop_stats_entitlement) {
			KAOPLOG_ERR("aop stats request rejected, EPERM");
			return EPERM;
		}
	}

	buffer_space = req->oldlen;
	if (req->oldptr != USER_ADDR_NULL && buffer_space != 0) {
		if (oidp->oid_arg2 == KAOP_PROTOCOL_STATS) {
			if (buffer_space < sizeof(proto_stats)) {
				return ENOMEM;
			}

			error = net_aop_get_protocol_stats(&proto_stats);
			out_buffer = (uint8_t *)&proto_stats;
			out_size = sizeof(proto_stats);
		} else if (oidp->oid_arg2 == KAOP_DRIVER_STATS) {
			if (buffer_space < sizeof(driver_stats)) {
				return ENOMEM;
			}

			error = net_aop_get_driver_stats(&driver_stats);
			out_buffer = (uint8_t *)(&driver_stats);
			out_size = sizeof(driver_stats);
		} else if (oidp->oid_arg2 == KAOP_PROC_ACTIVITY_BITMAPS) {
			struct aop_proc_activity_bitmap *__single bitmap = NULL;
			error = aop_get_process_activity_bitmaps(&bitmap, buffer_space, &out_size);
			out_buffer = (uint8_t *)bitmap;
		}

		if (error == 0) {
			error = SYSCTL_OUT(req, out_buffer, out_size);
		}
	} else if (req->oldptr == USER_ADDR_NULL) {
		if (oidp->oid_arg2 == KAOP_PROTOCOL_STATS) {
			buffer_space = sizeof(proto_stats);
		} else if (oidp->oid_arg2 == KAOP_DRIVER_STATS) {
			buffer_space = sizeof(driver_stats);
		} else if (oidp->oid_arg2 == KAOP_PROC_ACTIVITY_BITMAPS) {
			uint16_t proc_bitmap_count = 0;
			net_aop_get_proc_activity_bitmaps(NULL, &proc_bitmap_count);
			buffer_space = (proc_bitmap_count * sizeof(struct aop_proc_activity_bitmap));
		}
		error = SYSCTL_OUT(req, NULL, buffer_space);
	}

	return error;
}

void
net_aop_init(void)
{
	kaop_log_handle = os_log_create("com.apple.xnu.net.aopnet", "aopnet");
}
