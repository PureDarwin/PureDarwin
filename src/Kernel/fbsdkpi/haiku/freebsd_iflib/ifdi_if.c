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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <machine/bus.h>
#include <sys/bus.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/iflib.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <ifdi_if.h>



	static void
	null_void_op(if_ctx_t _ctx __unused)
	{
	}

#ifndef __HAIKU__
	static int
	null_knlist_add(if_ctx_t _ctx __unused, struct knote *_kn)
	{
	    return (0);
	}

	static int
	null_knote_event(if_ctx_t _ctx __unused, struct knote *_kn, int _hint)
	{
	    return (0);
	}
#endif

	static void
	null_timer_op(if_ctx_t _ctx __unused, uint16_t _qsidx __unused)
	{
	}

	static int
	null_int_op(if_ctx_t _ctx __unused)
	{
		return (0);
	}

	static int
	null_int_int_op(if_ctx_t _ctx __unused, int arg0 __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_queue_intr_enable(if_ctx_t _ctx __unused, uint16_t _qid __unused)
	{
		return (ENOTSUP);
	}

	static void
	null_led_func(if_ctx_t _ctx __unused, int _onoff __unused)
	{
	}

	static void
	null_vlan_register_op(if_ctx_t _ctx __unused, uint16_t vtag __unused)
	{
	}

	static bool
	null_needs_restart_op(if_ctx_t _ctx __unused, enum iflib_restart_event _event __unused)
	{
	}

	static int
	null_q_setup(if_ctx_t _ctx __unused, uint32_t _qid __unused)
	{
		return (0);
	}

	static int
	null_i2c_req(if_ctx_t _sctx __unused, struct ifi2creq *_i2c __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_sysctl_int_delay(if_ctx_t _sctx __unused, if_int_delay_info_t _iidi __unused)
	{
		return (0);
	}

	static int
	null_iov_init(if_ctx_t _ctx __unused, uint16_t num_vfs __unused, const nvlist_t *params __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_vf_add(if_ctx_t _ctx __unused, uint16_t num_vfs __unused, const nvlist_t *params __unused)
	{
		return (ENOTSUP);
	}

	static int
	null_priv_ioctl(if_ctx_t _ctx __unused, u_long command, caddr_t *data __unused)
	{
		return (ENOTSUP);
	}

	static void
	null_media_status(if_ctx_t ctx __unused, struct ifmediareq *ifmr)
	{
	    ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	    ifmr->ifm_active = IFM_ETHER | IFM_FDX;
	}

	static int
	null_cloneattach(if_ctx_t ctx __unused, struct if_clone *ifc __unused,
			 const char *name __unused, caddr_t params __unused)
	{
	    return (0);
	}

	static void
	null_rx_clset(if_ctx_t _ctx __unused, uint16_t _flid __unused,
		      uint16_t _qid __unused, caddr_t *_sdcl __unused)
	{
	}
	static void
	null_object_info_get(if_ctx_t ctx __unused, void *data __unused, int size __unused)
	{
	}
	static int
	default_mac_set(if_ctx_t ctx, const uint8_t *mac)
	{
	    struct ifnet *ifp = iflib_get_ifp(ctx);
	    struct sockaddr_dl *sdl;

	    if (ifp && ifp->if_addr) {
		sdl = (struct sockaddr_dl *)ifp->if_addr->ifa_addr;
		MPASS(sdl->sdl_type == IFT_ETHER);
		memcpy(LLADDR(sdl), mac, ETHER_ADDR_LEN);
	    }
	    return (0);
	}

#ifndef __HAIKU__
struct kobjop_desc ifdi_knlist_add_desc = {
	0, { NULL, ID_ifdi_knlist_add, (kobjop_t)null_knlist_add }
};

struct kobjop_desc ifdi_knote_event_desc = {
	0, { NULL, ID_ifdi_knote_event, (kobjop_t)null_knote_event }
};
#endif

struct kobjop_desc ifdi_object_info_get_desc = {
	0, { NULL, ID_ifdi_object_info_get, (kobjop_t)null_object_info_get }
};

struct kobjop_desc ifdi_attach_pre_desc = {
	0, { NULL, ID_ifdi_attach_pre, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_attach_post_desc = {
	0, { NULL, ID_ifdi_attach_post, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_reinit_pre_desc = {
	0, { NULL, ID_ifdi_reinit_pre, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_reinit_post_desc = {
	0, { NULL, ID_ifdi_reinit_post, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_cloneattach_desc = {
	0, { NULL, ID_ifdi_cloneattach, (kobjop_t)null_cloneattach }
};

struct kobjop_desc ifdi_detach_desc = {
	0, { NULL, ID_ifdi_detach, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_suspend_desc = {
	0, { NULL, ID_ifdi_suspend, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_shutdown_desc = {
	0, { NULL, ID_ifdi_shutdown, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_resume_desc = {
	0, { NULL, ID_ifdi_resume, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_tx_queues_alloc_desc = {
	0, { NULL, ID_ifdi_tx_queues_alloc, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_rx_queues_alloc_desc = {
	0, { NULL, ID_ifdi_rx_queues_alloc, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_queues_free_desc = {
	0, { NULL, ID_ifdi_queues_free, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_rx_clset_desc = {
	0, { NULL, ID_ifdi_rx_clset, (kobjop_t)null_rx_clset }
};

struct kobjop_desc ifdi_init_desc = {
	0, { NULL, ID_ifdi_init, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_stop_desc = {
	0, { NULL, ID_ifdi_stop, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_msix_intr_assign_desc = {
	0, { NULL, ID_ifdi_msix_intr_assign, (kobjop_t)null_int_int_op }
};

struct kobjop_desc ifdi_intr_enable_desc = {
	0, { NULL, ID_ifdi_intr_enable, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_intr_disable_desc = {
	0, { NULL, ID_ifdi_intr_disable, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_rx_queue_intr_enable_desc = {
	0, { NULL, ID_ifdi_rx_queue_intr_enable, (kobjop_t)null_queue_intr_enable }
};

struct kobjop_desc ifdi_tx_queue_intr_enable_desc = {
	0, { NULL, ID_ifdi_tx_queue_intr_enable, (kobjop_t)null_queue_intr_enable }
};

struct kobjop_desc ifdi_link_intr_enable_desc = {
	0, { NULL, ID_ifdi_link_intr_enable, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_multi_set_desc = {
	0, { NULL, ID_ifdi_multi_set, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_mtu_set_desc = {
	0, { NULL, ID_ifdi_mtu_set, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_mac_set_desc = {
	0, { NULL, ID_ifdi_mac_set, (kobjop_t)default_mac_set }
};

struct kobjop_desc ifdi_media_set_desc = {
	0, { NULL, ID_ifdi_media_set, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_promisc_set_desc = {
	0, { NULL, ID_ifdi_promisc_set, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_crcstrip_set_desc = {
	0, { NULL, ID_ifdi_crcstrip_set, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_vflr_handle_desc = {
	0, { NULL, ID_ifdi_vflr_handle, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_iov_init_desc = {
	0, { NULL, ID_ifdi_iov_init, (kobjop_t)null_iov_init }
};

struct kobjop_desc ifdi_iov_uninit_desc = {
	0, { NULL, ID_ifdi_iov_uninit, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_iov_vf_add_desc = {
	0, { NULL, ID_ifdi_iov_vf_add, (kobjop_t)null_vf_add }
};

struct kobjop_desc ifdi_update_admin_status_desc = {
	0, { NULL, ID_ifdi_update_admin_status, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_media_status_desc = {
	0, { NULL, ID_ifdi_media_status, (kobjop_t)null_media_status }
};

struct kobjop_desc ifdi_media_change_desc = {
	0, { NULL, ID_ifdi_media_change, (kobjop_t)null_int_op }
};

struct kobjop_desc ifdi_get_counter_desc = {
	0, { NULL, ID_ifdi_get_counter, (kobjop_t)kobj_error_method }
};

struct kobjop_desc ifdi_priv_ioctl_desc = {
	0, { NULL, ID_ifdi_priv_ioctl, (kobjop_t)null_priv_ioctl }
};

struct kobjop_desc ifdi_i2c_req_desc = {
	0, { NULL, ID_ifdi_i2c_req, (kobjop_t)null_i2c_req }
};

struct kobjop_desc ifdi_txq_setup_desc = {
	0, { NULL, ID_ifdi_txq_setup, (kobjop_t)null_q_setup }
};

struct kobjop_desc ifdi_rxq_setup_desc = {
	0, { NULL, ID_ifdi_rxq_setup, (kobjop_t)null_q_setup }
};

struct kobjop_desc ifdi_timer_desc = {
	0, { NULL, ID_ifdi_timer, (kobjop_t)null_timer_op }
};

struct kobjop_desc ifdi_watchdog_reset_desc = {
	0, { NULL, ID_ifdi_watchdog_reset, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_watchdog_reset_queue_desc = {
	0, { NULL, ID_ifdi_watchdog_reset_queue, (kobjop_t)null_timer_op }
};

struct kobjop_desc ifdi_led_func_desc = {
	0, { NULL, ID_ifdi_led_func, (kobjop_t)null_led_func }
};

struct kobjop_desc ifdi_vlan_register_desc = {
	0, { NULL, ID_ifdi_vlan_register, (kobjop_t)null_vlan_register_op }
};

struct kobjop_desc ifdi_vlan_unregister_desc = {
	0, { NULL, ID_ifdi_vlan_unregister, (kobjop_t)null_vlan_register_op }
};

struct kobjop_desc ifdi_sysctl_int_delay_desc = {
	0, { NULL, ID_ifdi_sysctl_int_delay, (kobjop_t)null_sysctl_int_delay }
};

struct kobjop_desc ifdi_debug_desc = {
	0, { NULL, ID_ifdi_debug, (kobjop_t)null_void_op }
};

struct kobjop_desc ifdi_needs_restart_desc = {
	0, { NULL, ID_ifdi_needs_restart, (kobjop_t)null_needs_restart_op }
};
