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
/*!
 *       @header kpi_aop.h
 *       This header defines an SPI to interact with the AOP
 *       using shared memory. The SPIs could be used to collect networking
 *       stats associated with flows in AOP.
 */

#ifndef __NET_KPI_AOP_H__
#define __NET_KPI_AOP_H__

#include <net/aop/aop_stats.h>
#include <net/aop/aop_flow_stats.h>

__BEGIN_DECLS

typedef enum {
	NET_AOP_CAPAB_FLOW_SETUP = 1,
	NET_AOP_CAPAB_FLOW_STATS = 2,
	NET_AOP_CAPAB_STATS = 3,
	NET_AOP_CAPAB_PROC_ACTIVITY_BITMAP = 4,
} net_aop_capab_t;

typedef errno_t (*net_aop_config_fn_t)(void *prov_ctx,
    net_aop_capab_t capab, void *contents, uint32_t *len);

#define NET_AOP_VERSION_1 1
typedef struct net_aop_provider_init {
	uint32_t                 kaopi_version;
	net_aop_config_fn_t kaopi_config_capab;
} net_aop_provider_init_t;

#define NET_AOP_CAPAB_FLOW_SETUP_VERSION_1 1
typedef errno_t (*net_aop_flow_setup_fn_t)(void *prov_ctx,
    uint32_t flow_id, bool add, uint32_t *stats_index);
struct net_aop_capab_flow_setup {
	uint32_t kaopcfsp_version;
	void *kaopcfsp_prov_ctx;
	net_aop_flow_setup_fn_t kaopcfsp_config;
};

#define NET_AOP_CAPAB_FLOW_STATS_VERSION_1 1
typedef errno_t (*net_aop_flow_stats_fn_t)(void *prov_ctx,
    uint32_t stats_index, struct aop_flow_stats *flow_stats);
struct net_aop_capab_flow_stats {
	uint32_t kaopcfs_version;
	void *kaopcfs_prov_ctx;
	net_aop_flow_stats_fn_t kaopcfs_config;
};

typedef enum {
	NET_AOP_STATS_TYPE_INVALID = 0,
	NET_AOP_STATS_TYPE_IP = 1,
	NET_AOP_STATS_TYPE_IPV6 = 2,
	NET_AOP_STATS_TYPE_TCP = 3,
	NET_AOP_STATS_TYPE_UDP = 4,
	NET_AOP_STATS_TYPE_DRIVER = 5,
	NET_AOP_STATS_TYPE_MAX = NET_AOP_STATS_TYPE_DRIVER,
} net_aop_stats_type_t;

#define NET_AOP_CAPAB_STATS_VERSION_1 1
typedef errno_t (*net_aop_stats_fn_t)(void *prov_ctx,
    net_aop_stats_type_t type, uint8_t *stats, size_t stats_len);
struct net_aop_capab_stats {
	uint32_t kaopcgs_version;
	void *kaopcgs_prov_ctx;
	net_aop_stats_fn_t kaopcgs_config;
};

#define NET_AOP_CAPAB_PROC_ACTIVITY_BITMAP_VERSION_1 1
typedef errno_t (*net_aop_proc_activity_bitmap_fn_t)(void *prov_ctx,
    struct aop_proc_activity_bitmap *proc_activity_bitmaps, uint16_t *inout_count);
struct net_aop_capab_proc_activity_bitmap {
	uint32_t kaopbm_version;
	void *kaopbm_prov_ctx;
	net_aop_proc_activity_bitmap_fn_t kaopbm_config;
};

typedef struct net_aop_provider_handle *net_aop_provider_handle_t;

extern net_aop_provider_handle_t
net_aop_register_provider(const struct net_aop_provider_init *init,
    const uint32_t init_len, void *ctx);
extern void
net_aop_deregister_provider(net_aop_provider_handle_t prov);

#ifdef BSD_KERNEL_PRIVATE
void net_aop_init(void);

int net_aop_setup_flow(uint32_t flow_id, bool add, uint32_t *stats_index);

int net_aop_get_flow_stats(uint32_t stats_index, struct aop_flow_stats *flow_stats);

int net_aop_get_stats(net_aop_stats_type_t type,
    uint8_t *__sized_by(stats_len) stats, size_t stats_len);

int net_aop_get_proc_activity_bitmaps(struct aop_proc_activity_bitmap *proc_activity_bitmaps,
    uint16_t *inout_count);
#endif // BSD_KERNEL_PRIVATE

__END_DECLS

#endif /* __NET_KPI_AOP_H__ */
