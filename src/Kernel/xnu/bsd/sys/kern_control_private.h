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
 *       @header kern_control.h
 *       This header defines an API to communicate between a kernel
 *       extension and a process outside of the kernel.
 */

#ifndef KPI_KERN_CONTROL_PRIVATE_H
#define KPI_KERN_CONTROL_PRIVATE_H

#include <sys/kern_control.h>

struct xkctl_reg {
	u_int32_t       xkr_len;
	u_int32_t       xkr_kind;
	u_int32_t       xkr_id;
	u_int32_t       xkr_reg_unit;
	u_int32_t       xkr_flags;
	u_int64_t       xkr_kctlref;
	u_int32_t       xkr_recvbufsize;
	u_int32_t       xkr_sendbufsize;
	u_int32_t       xkr_lastunit;
	u_int32_t       xkr_pcbcount;
	u_int64_t       xkr_connect;
	u_int64_t       xkr_disconnect;
	u_int64_t       xkr_send;
	u_int64_t       xkr_send_list;
	u_int64_t       xkr_setopt;
	u_int64_t       xkr_getopt;
	u_int64_t       xkr_rcvd;
	char            xkr_name[MAX_KCTL_NAME];
};

struct xkctlpcb {
	u_int32_t       xkp_len;
	u_int32_t       xkp_kind;
	u_int64_t       xkp_kctpcb;
	u_int32_t       xkp_unit;
	u_int32_t       xkp_kctlid;
	u_int64_t       xkp_kctlref;
	char            xkp_kctlname[MAX_KCTL_NAME];
};

struct kctlstat {
	u_int64_t       kcs_reg_total __attribute__((aligned(8)));
	u_int64_t       kcs_reg_count __attribute__((aligned(8)));
	u_int64_t       kcs_pcbcount __attribute__((aligned(8)));
	u_int64_t       kcs_gencnt __attribute__((aligned(8)));
	u_int64_t       kcs_connections __attribute__((aligned(8)));
	u_int64_t       kcs_conn_fail __attribute__((aligned(8)));
	u_int64_t       kcs_send_fail __attribute__((aligned(8)));
	u_int64_t       kcs_send_list_fail __attribute__((aligned(8)));
	u_int64_t       kcs_enqueue_fail __attribute__((aligned(8)));
	u_int64_t       kcs_enqueue_fullsock __attribute__((aligned(8)));
	u_int64_t       kcs_bad_kctlref __attribute__((aligned(8)));
	u_int64_t       kcs_tbl_size_too_big __attribute__((aligned(8)));
	u_int64_t       kcs_enqdata_mb_alloc_fail __attribute__((aligned(8)));
	u_int64_t       kcs_enqdata_sbappend_fail __attribute__((aligned(8)));
};

#ifdef KERNEL

#ifdef KERNEL_PRIVATE
/*!
 *       @defined CTL_FLAG_REG_EXTENDED
 *   @discussion This flag indicates that this kernel control utilizes the
 *       the extended fields within the kern_ctl_reg structure.
 */
#define CTL_FLAG_REG_EXTENDED   0x8

/*!
 *       @defined CTL_FLAG_REG_CRIT
 *   @discussion This flag indicates that this kernel control utilizes the
 *       the extended fields within the kern_ctl_reg structure.
 */
#define CTL_FLAG_REG_CRIT       0x10

/*!
 *       @defined CTL_FLAG_REG_SETUP
 *   @discussion This flag indicates that this kernel control utilizes the
 *       the setup callback field within the kern_ctl_reg structure.
 */
#define CTL_FLAG_REG_SETUP      0x20

/*!
 *       @defined CTL_DATA_CRIT
 *       @discussion This flag indicates the data is critical to the client
 *               and that it needs to be forced into the socket buffer
 *               by resizing it if needed.
 */
#define CTL_DATA_CRIT   0x4
#endif /* KERNEL_PRIVATE */

__BEGIN_DECLS

#ifdef KERNEL_PRIVATE
/*!
 *       @typedef ctl_rcvd_func
 *       @discussion The ctl_rcvd_func is called when the client reads data from
 *               the kernel control socket. The kernel control can use this callback
 *               in combination with ctl_getenqueuespace() to avoid overflowing
 *               the socket's receive buffer. When ctl_getenqueuespace() returns
 *               0 or ctl_enqueuedata()/ctl_enqueuembuf() return ENOBUFS, the
 *               kernel control can wait until this callback is called before
 *               trying to enqueue the data again.
 *       @param kctlref The control ref of the kernel control.
 *       @param unit The unit number of the kernel control instance.
 *       @param unitinfo The user-defined private data initialized by the
 *               ctl_connect_func callback.
 *       @param flags The recv flags. See the recv(2) man page.
 */
typedef void (*ctl_rcvd_func)(kern_ctl_ref kctlref, u_int32_t unit, void *unitinfo,
    int flags);

/*!
 *       @typedef ctl_send_list_func
 *       @discussion The ctl_send_list_func is used to receive data sent from
 *               the client to the kernel control.
 *       @param kctlref The control ref of the kernel control.
 *       @param unit The unit number of the kernel control instance the client has
 *               connected to.
 *       @param unitinfo The user-defined private data initialized by the
 *               ctl_connect_func callback.
 *       @param m The data sent by the client to the kernel control in an
 *               mbuf packet chain. Your function is responsible for releasing
 *               mbuf packet chain.
 *       @param flags The flags specified by the client when calling
 *               send/sendto/sendmsg (MSG_OOB/MSG_DONTROUTE).
 */
typedef errno_t (*ctl_send_list_func)(kern_ctl_ref kctlref, u_int32_t unit, void *unitinfo,
    mbuf_t m, int flags);

/*!
 *       @typedef ctl_bind_func
 *       @discussion The ctl_bind_func is an optional function that allows the client
 *               to set up their unitinfo prior to connecting.
 *       @param kctlref The control ref for the kernel control the client is
 *               binding to.
 *       @param sac The address used to connect to this control. The field sc_unit
 *               contains the unit number of the kernel control instance the client is
 *               binding to. If CTL_FLAG_REG_ID_UNIT was set when the kernel control
 *               was registered, sc_unit is the ctl_unit of the kern_ctl_reg structure.
 *               If CTL_FLAG_REG_ID_UNIT was not set when the kernel control was
 *               registered, sc_unit is the dynamically allocated unit number of
 *               the new kernel control instance that is used for this connection.
 *       @param unitinfo A placeholder for a pointer to the optional user-defined
 *               private data associated with this kernel control instance.  This
 *               opaque info will be provided to the user when the rest of the
 *               callback routines are executed.  For example, it can be used
 *               to pass a pointer to an instance-specific data structure in
 *               order for the user to keep track of the states related to this
 *               kernel control instance.
 */
typedef errno_t (*ctl_bind_func)(kern_ctl_ref kctlref,
    struct sockaddr_ctl *sac,
    void **unitinfo);

/*!
 *       @typedef ctl_setup_func
 *       @discussion The ctl_setup_func is an optional function that allows the client
 *               to pick a unit number in the case that the caller hasn't specified one
 *       @param unit A placeholder for a pointer to the unit number that is selected with
 *               this kernel control instance
 *       @param unitinfo A placeholder for a pointer to the optional user-defined
 *               private data associated with this kernel control instance.  This
 *               opaque info will be provided to the user when the rest of the
 *               callback routines are executed.  For example, it can be used
 *               to pass a pointer to an instance-specific data structure in
 *               order for the user to keep track of the states related to this
 *               kernel control instance.
 */
typedef errno_t (*ctl_setup_func)(u_int32_t *unit, void **unitinfo);
#endif /* KERNEL_PRIVATE */

#ifdef KERN_CTL_REG_OPAQUE
/*!
 *       @struct kern_ctl_reg
 *       @discussion This structure defines the properties of a kernel
 *               control being registered.
 *       @field ctl_name A Bundle ID string of up to MAX_KCTL_NAME bytes (including the ending zero).
 *               This string should not be empty.
 *       @field ctl_id The control ID may be dynamically assigned or it can be a
 *               32-bit creator code assigned by DTS.
 *               For a DTS assigned creator code the CTL_FLAG_REG_ID_UNIT flag must be set.
 *               For a dynamically assigned control ID, do not set the CTL_FLAG_REG_ID_UNIT flag.
 *               The  value of the dynamically assigned control ID is set to this field
 *               when the registration succeeds.
 *       @field ctl_unit A separate unit number to register multiple units that
 *               share the same control ID with DTS assigned creator code when
 *               the CTL_FLAG_REG_ID_UNIT flag is set.
 *               This field is ignored for a dynamically assigned control ID.
 *       @field ctl_flags CTL_FLAG_PRIVILEGED and/or CTL_FLAG_REG_ID_UNIT.
 *       @field ctl_sendsize Override the default send size. If set to zero,
 *               the default send size will be used, and this default value
 *               is set to this field to be retrieved by the caller.
 *       @field ctl_recvsize Override the default receive size. If set to
 *               zero, the default receive size will be used, and this default value
 *               is set to this field to be retrieved by the caller.
 *       @field ctl_connect Specify the  function to be called whenever a client
 *               connects to the kernel control. This field must be specified.
 *       @field ctl_disconnect Specify a function to be called whenever a
 *               client disconnects from the kernel control.
 *       @field ctl_send Specify a function to handle data send from the
 *               client to the kernel control.
 *       @field ctl_setopt Specify a function to handle set socket option
 *               operations for the kernel control.
 *       @field ctl_getopt Specify a function to handle get socket option
 *               operations for the kernel control.
 */
struct kern_ctl_reg {
	/* control information */
	char            ctl_name[MAX_KCTL_NAME];
	u_int32_t       ctl_id;
	u_int32_t       ctl_unit;

	/* control settings */
	u_int32_t   ctl_flags;
	u_int32_t   ctl_sendsize;
	u_int32_t   ctl_recvsize;

	/* Dispatch functions */
	ctl_connect_func    ctl_connect;
	ctl_disconnect_func ctl_disconnect;
	ctl_send_func               ctl_send;
	ctl_setopt_func             ctl_setopt;
	ctl_getopt_func             ctl_getopt;
	ctl_rcvd_func               ctl_rcvd;   /* Only valid if CTL_FLAG_REG_EXTENDED is set */
	ctl_send_list_func          ctl_send_list;/* Only valid if CTL_FLAG_REG_EXTENDED is set */
	ctl_bind_func           ctl_bind;
	ctl_setup_func                  ctl_setup;
};
#endif /* KERN_CTL_REG_OPAQUE */

/*!
 *       @function ctl_enqueuembuf_list
 *       @discussion Send data stored in an mbuf packet chain from the kernel
 *               control to the client. The caller is responsible for freeing
 *               the mbuf chain if ctl_enqueuembuf returns an error.
 *               Not valid if ctl_flags contains CTL_FLAG_REG_SOCK_STREAM.
 *       @param kctlref The control reference of the kernel control.
 *       @param unit The unit number of the kernel control instance.
 *       @param m_list An mbuf chain containing the data to send to the client.
 *       @param flags Send flags. CTL_DATA_NOWAKEUP is
 *               the only supported flags.
 *       @param m_remain A pointer to the list of mbuf packets in the chain that
 *               could not be enqueued.
 *       @result 0 - Data was enqueued to be read by the client.
 *               EINVAL - Invalid parameters.
 *               ENOBUFS - The queue is full.
 */
errno_t
ctl_enqueuembuf_list(kern_ctl_ref kctlref, u_int32_t unit, mbuf_t m_list,
    u_int32_t flags, mbuf_t *m_remain);

/*!
 *       @function ctl_getenqueuepacketcount
 *       @discussion Retrieve the number of packets in the socket
 *               receive buffer.
 *       @param kctlref The control reference of the kernel control.
 *       @param unit The unit number of the kernel control instance.
 *       @param pcnt The address where to return the current count.
 *       @result 0 - Success; the packet count is returned to caller.
 *               EINVAL - Invalid parameters.
 */
errno_t
ctl_getenqueuepacketcount(kern_ctl_ref kctlref, u_int32_t unit, u_int32_t *pcnt);

#ifdef KERNEL_PRIVATE

#include <sys/queue.h>
#include <libkern/locks.h>

/*
 * internal structure maintained for each register controller
 */
struct ctl_cb;
struct kctl;
struct socket;
struct socket_info;

void kctl_fill_socketinfo(struct socket *, struct socket_info *);

u_int32_t ctl_id_by_name(const char *);
errno_t ctl_name_by_id(u_int32_t, char *__counted_by(maxsize), size_t maxsize);

extern const u_int32_t ctl_maxunit;
#endif /* KERNEL_PRIVATE */

__END_DECLS
#endif /* KERNEL */

#endif /* KPI_KERN_CONTROL_PRIVATE_H */
