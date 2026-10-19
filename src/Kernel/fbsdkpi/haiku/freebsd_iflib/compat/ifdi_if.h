/*
* This file is produced automatically.
* Do not modify anything in here by hand.
*
* Created from source file
*   sys/net/ifdi_if.m
#
# Copyright (c) 2014-2018, Matthew Macy (mmacy@mattmacy.io)
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#
#  2. Neither the name of Matthew Macy nor the names of its
#     contributors may be used to endorse or promote products derived from
#     this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef _ifdi_if_h_
#define _ifdi_if_h_

#ifndef __HAIKU__
extern struct kobjop_desc ifdi_knlist_add_desc;
typedef int ifdi_knlist_add_t(if_ctx_t _ctx, struct knote *_kn);

static __inline int IFDI_KNLIST_ADD(if_ctx_t _ctx, struct knote *_kn)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_knlist_add);
	rc = ((ifdi_knlist_add_t *) _m)(_ctx, _kn);
	return (rc);
}

extern struct kobjop_desc ifdi_knote_event_desc;
typedef int ifdi_knote_event_t(if_ctx_t _ctx, struct knote *_kn, int hint);

static __inline int IFDI_KNOTE_EVENT(if_ctx_t _ctx, struct knote *_kn, int hint)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_knote_event);
	rc = ((ifdi_knote_event_t *) _m)(_ctx, _kn, hint);
	return (rc);
}
#endif

extern struct kobjop_desc ifdi_object_info_get_desc;
typedef int ifdi_object_info_get_t(if_ctx_t _ctx, void *data, int size);

static __inline int IFDI_OBJECT_INFO_GET(if_ctx_t _ctx, void *data, int size)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_object_info_get);
	rc = ((ifdi_object_info_get_t *) _m)(_ctx, data, size);
	return (rc);
}

extern struct kobjop_desc ifdi_attach_pre_desc;
typedef int ifdi_attach_pre_t(if_ctx_t _ctx);

static __inline int IFDI_ATTACH_PRE(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_attach_pre);
	rc = ((ifdi_attach_pre_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_attach_post_desc;
typedef int ifdi_attach_post_t(if_ctx_t _ctx);

static __inline int IFDI_ATTACH_POST(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_attach_post);
	rc = ((ifdi_attach_post_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_reinit_pre_desc;
typedef int ifdi_reinit_pre_t(if_ctx_t _ctx);

static __inline int IFDI_REINIT_PRE(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_reinit_pre);
	rc = ((ifdi_reinit_pre_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_reinit_post_desc;
typedef int ifdi_reinit_post_t(if_ctx_t _ctx);

static __inline int IFDI_REINIT_POST(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_reinit_post);
	rc = ((ifdi_reinit_post_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_cloneattach_desc;
typedef int ifdi_cloneattach_t(if_ctx_t _ctx, struct if_clone *_ifc,
							const char *_name, caddr_t params);

static __inline int IFDI_CLONEATTACH(if_ctx_t _ctx, struct if_clone *_ifc,
								 const char *_name, caddr_t params)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_cloneattach);
	rc = ((ifdi_cloneattach_t *) _m)(_ctx, _ifc, _name, params);
	return (rc);
}

extern struct kobjop_desc ifdi_detach_desc;
typedef int ifdi_detach_t(if_ctx_t _ctx);

static __inline int IFDI_DETACH(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_detach);
	rc = ((ifdi_detach_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_suspend_desc;
typedef int ifdi_suspend_t(if_ctx_t _ctx);

static __inline int IFDI_SUSPEND(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_suspend);
	rc = ((ifdi_suspend_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_shutdown_desc;
typedef int ifdi_shutdown_t(if_ctx_t _ctx);

static __inline int IFDI_SHUTDOWN(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_shutdown);
	rc = ((ifdi_shutdown_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_resume_desc;
typedef int ifdi_resume_t(if_ctx_t _ctx);

static __inline int IFDI_RESUME(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_resume);
	rc = ((ifdi_resume_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_tx_queues_alloc_desc;
typedef int ifdi_tx_queues_alloc_t(if_ctx_t _ctx, caddr_t *_vaddrs,
								uint64_t *_paddrs, int ntxqs, int ntxqsets);

static __inline int IFDI_TX_QUEUES_ALLOC(if_ctx_t _ctx, caddr_t *_vaddrs,
									 uint64_t *_paddrs, int ntxqs,
									 int ntxqsets)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_tx_queues_alloc);
	rc = ((ifdi_tx_queues_alloc_t *) _m)(_ctx, _vaddrs, _paddrs, ntxqs, ntxqsets);
	return (rc);
}

extern struct kobjop_desc ifdi_rx_queues_alloc_desc;
typedef int ifdi_rx_queues_alloc_t(if_ctx_t _ctx, caddr_t *_vaddrs,
								uint64_t *_paddrs, int nrxqs, int nrxqsets);

static __inline int IFDI_RX_QUEUES_ALLOC(if_ctx_t _ctx, caddr_t *_vaddrs,
									 uint64_t *_paddrs, int nrxqs,
									 int nrxqsets)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_rx_queues_alloc);
	rc = ((ifdi_rx_queues_alloc_t *) _m)(_ctx, _vaddrs, _paddrs, nrxqs, nrxqsets);
	return (rc);
}

extern struct kobjop_desc ifdi_queues_free_desc;
typedef void ifdi_queues_free_t(if_ctx_t _ctx);

static __inline void IFDI_QUEUES_FREE(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_queues_free);
	((ifdi_queues_free_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_rx_clset_desc;
typedef void ifdi_rx_clset_t(if_ctx_t _ctx, uint16_t _fl, uint16_t _qsetid,
						 caddr_t *_sdcl);

static __inline void IFDI_RX_CLSET(if_ctx_t _ctx, uint16_t _fl,
								uint16_t _qsetid, caddr_t *_sdcl)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_rx_clset);
	((ifdi_rx_clset_t *) _m)(_ctx, _fl, _qsetid, _sdcl);
}

extern struct kobjop_desc ifdi_init_desc;
typedef void ifdi_init_t(if_ctx_t _ctx);

static __inline void IFDI_INIT(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_init);
	((ifdi_init_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_stop_desc;
typedef void ifdi_stop_t(if_ctx_t _ctx);

static __inline void IFDI_STOP(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_stop);
	((ifdi_stop_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_msix_intr_assign_desc;
typedef int ifdi_msix_intr_assign_t(if_ctx_t _sctx, int msix);

static __inline int IFDI_MSIX_INTR_ASSIGN(if_ctx_t _sctx, int msix)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_sctx)->ops,ifdi_msix_intr_assign);
	rc = ((ifdi_msix_intr_assign_t *) _m)(_sctx, msix);
	return (rc);
}

extern struct kobjop_desc ifdi_intr_enable_desc;
typedef void ifdi_intr_enable_t(if_ctx_t _ctx);

static __inline void IFDI_INTR_ENABLE(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_intr_enable);
	((ifdi_intr_enable_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_intr_disable_desc;
typedef void ifdi_intr_disable_t(if_ctx_t _ctx);

static __inline void IFDI_INTR_DISABLE(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_intr_disable);
	((ifdi_intr_disable_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_rx_queue_intr_enable_desc;
typedef int ifdi_rx_queue_intr_enable_t(if_ctx_t _ctx, uint16_t _qid);

static __inline int IFDI_RX_QUEUE_INTR_ENABLE(if_ctx_t _ctx, uint16_t _qid)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_rx_queue_intr_enable);
	rc = ((ifdi_rx_queue_intr_enable_t *) _m)(_ctx, _qid);
	return (rc);
}

extern struct kobjop_desc ifdi_tx_queue_intr_enable_desc;
typedef int ifdi_tx_queue_intr_enable_t(if_ctx_t _ctx, uint16_t _qid);

static __inline int IFDI_TX_QUEUE_INTR_ENABLE(if_ctx_t _ctx, uint16_t _qid)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_tx_queue_intr_enable);
	rc = ((ifdi_tx_queue_intr_enable_t *) _m)(_ctx, _qid);
	return (rc);
}

extern struct kobjop_desc ifdi_link_intr_enable_desc;
typedef void ifdi_link_intr_enable_t(if_ctx_t _ctx);

static __inline void IFDI_LINK_INTR_ENABLE(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_link_intr_enable);
	((ifdi_link_intr_enable_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_multi_set_desc;
typedef void ifdi_multi_set_t(if_ctx_t _ctx);

static __inline void IFDI_MULTI_SET(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_multi_set);
	((ifdi_multi_set_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_mtu_set_desc;
typedef int ifdi_mtu_set_t(if_ctx_t _ctx, uint32_t _mtu);

static __inline int IFDI_MTU_SET(if_ctx_t _ctx, uint32_t _mtu)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_mtu_set);
	rc = ((ifdi_mtu_set_t *) _m)(_ctx, _mtu);
	return (rc);
}

extern struct kobjop_desc ifdi_mac_set_desc;
typedef int ifdi_mac_set_t(if_ctx_t _ctx, const uint8_t *_mac);

static __inline int IFDI_MAC_SET(if_ctx_t _ctx, const uint8_t *_mac)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_mac_set);
	rc = ((ifdi_mac_set_t *) _m)(_ctx, _mac);
	return (rc);
}

extern struct kobjop_desc ifdi_media_set_desc;
typedef void ifdi_media_set_t(if_ctx_t _ctx);

static __inline void IFDI_MEDIA_SET(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_media_set);
	((ifdi_media_set_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_promisc_set_desc;
typedef int ifdi_promisc_set_t(if_ctx_t _ctx, int _flags);

static __inline int IFDI_PROMISC_SET(if_ctx_t _ctx, int _flags)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_promisc_set);
	rc = ((ifdi_promisc_set_t *) _m)(_ctx, _flags);
	return (rc);
}

extern struct kobjop_desc ifdi_crcstrip_set_desc;
typedef void ifdi_crcstrip_set_t(if_ctx_t _ctx, int _onoff, int _strip);

static __inline void IFDI_CRCSTRIP_SET(if_ctx_t _ctx, int _onoff, int _strip)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_crcstrip_set);
	((ifdi_crcstrip_set_t *) _m)(_ctx, _onoff, _strip);
}

extern struct kobjop_desc ifdi_vflr_handle_desc;
typedef void ifdi_vflr_handle_t(if_ctx_t _ctx);

static __inline void IFDI_VFLR_HANDLE(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_vflr_handle);
	((ifdi_vflr_handle_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_iov_init_desc;
typedef int ifdi_iov_init_t(if_ctx_t _ctx, uint16_t num_vfs,
						const nvlist_t * params);

static __inline int IFDI_IOV_INIT(if_ctx_t _ctx, uint16_t num_vfs,
							  const nvlist_t * params)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_iov_init);
	rc = ((ifdi_iov_init_t *) _m)(_ctx, num_vfs, params);
	return (rc);
}

extern struct kobjop_desc ifdi_iov_uninit_desc;
typedef void ifdi_iov_uninit_t(if_ctx_t _ctx);

static __inline void IFDI_IOV_UNINIT(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_iov_uninit);
	((ifdi_iov_uninit_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_iov_vf_add_desc;
typedef int ifdi_iov_vf_add_t(if_ctx_t _ctx, uint16_t num_vfs,
						  const nvlist_t * params);

static __inline int IFDI_IOV_VF_ADD(if_ctx_t _ctx, uint16_t num_vfs,
								const nvlist_t * params)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_iov_vf_add);
	rc = ((ifdi_iov_vf_add_t *) _m)(_ctx, num_vfs, params);
	return (rc);
}

extern struct kobjop_desc ifdi_update_admin_status_desc;
typedef void ifdi_update_admin_status_t(if_ctx_t _ctx);

static __inline void IFDI_UPDATE_ADMIN_STATUS(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_update_admin_status);
	((ifdi_update_admin_status_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_media_status_desc;
typedef void ifdi_media_status_t(if_ctx_t _ctx, struct ifmediareq *_ifm);

static __inline void IFDI_MEDIA_STATUS(if_ctx_t _ctx, struct ifmediareq *_ifm)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_media_status);
	((ifdi_media_status_t *) _m)(_ctx, _ifm);
}

extern struct kobjop_desc ifdi_media_change_desc;
typedef int ifdi_media_change_t(if_ctx_t _ctx);

static __inline int IFDI_MEDIA_CHANGE(if_ctx_t _ctx)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_media_change);
	rc = ((ifdi_media_change_t *) _m)(_ctx);
	return (rc);
}

extern struct kobjop_desc ifdi_get_counter_desc;
typedef uint64_t ifdi_get_counter_t(if_ctx_t _ctx, ift_counter cnt);

static __inline uint64_t IFDI_GET_COUNTER(if_ctx_t _ctx, ift_counter cnt)
{
	kobjop_t _m;
	uint64_t rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_get_counter);
	rc = ((ifdi_get_counter_t *) _m)(_ctx, cnt);
	return (rc);
}

extern struct kobjop_desc ifdi_priv_ioctl_desc;
typedef int ifdi_priv_ioctl_t(if_ctx_t _ctx, u_long _cmd, caddr_t _data);

static __inline int IFDI_PRIV_IOCTL(if_ctx_t _ctx, u_long _cmd, caddr_t _data)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_priv_ioctl);
	rc = ((ifdi_priv_ioctl_t *) _m)(_ctx, _cmd, _data);
	return (rc);
}

extern struct kobjop_desc ifdi_i2c_req_desc;
typedef int ifdi_i2c_req_t(if_ctx_t _ctx, struct ifi2creq *_req);

static __inline int IFDI_I2C_REQ(if_ctx_t _ctx, struct ifi2creq *_req)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_i2c_req);
	rc = ((ifdi_i2c_req_t *) _m)(_ctx, _req);
	return (rc);
}

extern struct kobjop_desc ifdi_txq_setup_desc;
typedef int ifdi_txq_setup_t(if_ctx_t _ctx, uint32_t _txqid);

static __inline int IFDI_TXQ_SETUP(if_ctx_t _ctx, uint32_t _txqid)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_txq_setup);
	rc = ((ifdi_txq_setup_t *) _m)(_ctx, _txqid);
	return (rc);
}

extern struct kobjop_desc ifdi_rxq_setup_desc;
typedef int ifdi_rxq_setup_t(if_ctx_t _ctx, uint32_t _txqid);

static __inline int IFDI_RXQ_SETUP(if_ctx_t _ctx, uint32_t _txqid)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_rxq_setup);
	rc = ((ifdi_rxq_setup_t *) _m)(_ctx, _txqid);
	return (rc);
}

extern struct kobjop_desc ifdi_timer_desc;
typedef void ifdi_timer_t(if_ctx_t _ctx, uint16_t _txqid);

static __inline void IFDI_TIMER(if_ctx_t _ctx, uint16_t _txqid)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_timer);
	((ifdi_timer_t *) _m)(_ctx, _txqid);
}

extern struct kobjop_desc ifdi_watchdog_reset_desc;
typedef void ifdi_watchdog_reset_t(if_ctx_t _ctx);

static __inline void IFDI_WATCHDOG_RESET(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_watchdog_reset);
	((ifdi_watchdog_reset_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_watchdog_reset_queue_desc;
typedef void ifdi_watchdog_reset_queue_t(if_ctx_t _ctx, uint16_t _q);

static __inline void IFDI_WATCHDOG_RESET_QUEUE(if_ctx_t _ctx, uint16_t _q)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_watchdog_reset_queue);
	((ifdi_watchdog_reset_queue_t *) _m)(_ctx, _q);
}

extern struct kobjop_desc ifdi_led_func_desc;
typedef void ifdi_led_func_t(if_ctx_t _ctx, int _onoff);

static __inline void IFDI_LED_FUNC(if_ctx_t _ctx, int _onoff)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_led_func);
	((ifdi_led_func_t *) _m)(_ctx, _onoff);
}

extern struct kobjop_desc ifdi_vlan_register_desc;
typedef void ifdi_vlan_register_t(if_ctx_t _ctx, uint16_t _vtag);

static __inline void IFDI_VLAN_REGISTER(if_ctx_t _ctx, uint16_t _vtag)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_vlan_register);
	((ifdi_vlan_register_t *) _m)(_ctx, _vtag);
}

extern struct kobjop_desc ifdi_vlan_unregister_desc;
typedef void ifdi_vlan_unregister_t(if_ctx_t _ctx, uint16_t _vtag);

static __inline void IFDI_VLAN_UNREGISTER(if_ctx_t _ctx, uint16_t _vtag)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_vlan_unregister);
	((ifdi_vlan_unregister_t *) _m)(_ctx, _vtag);
}

extern struct kobjop_desc ifdi_sysctl_int_delay_desc;
typedef int ifdi_sysctl_int_delay_t(if_ctx_t _sctx, if_int_delay_info_t _iidi);

static __inline int IFDI_SYSCTL_INT_DELAY(if_ctx_t _sctx,
	if_int_delay_info_t _iidi)
{
	kobjop_t _m;
	int rc;
	KOBJOPLOOKUP(((kobj_t)_sctx)->ops,ifdi_sysctl_int_delay);
	rc = ((ifdi_sysctl_int_delay_t *) _m)(_sctx, _iidi);
	return (rc);
}

extern struct kobjop_desc ifdi_debug_desc;
typedef void ifdi_debug_t(if_ctx_t _ctx);

static __inline void IFDI_DEBUG(if_ctx_t _ctx)
{
	kobjop_t _m;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_debug);
	((ifdi_debug_t *) _m)(_ctx);
}

extern struct kobjop_desc ifdi_needs_restart_desc;
typedef bool ifdi_needs_restart_t(if_ctx_t _ctx, enum iflib_restart_event _event);

static __inline bool IFDI_NEEDS_RESTART(if_ctx_t _ctx, enum iflib_restart_event _event)
{
	kobjop_t _m;
	bool rc;
	KOBJOPLOOKUP(((kobj_t)_ctx)->ops,ifdi_needs_restart);
	rc = ((ifdi_needs_restart_t *) _m)(_ctx, _event);
	return (rc);
}

#endif
