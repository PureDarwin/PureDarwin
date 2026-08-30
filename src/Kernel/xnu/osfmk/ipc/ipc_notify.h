/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	ipc/ipc_notify.h
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Declarations of notification-sending functions.
 */

#ifndef _IPC_IPC_NOTIFY_H_
#define _IPC_IPC_NOTIFY_H_

#include <mach/port.h>

__BEGIN_DECLS __ASSUME_PTR_ABI_SINGLE_BEGIN
#pragma GCC visibility push(hidden)


typedef struct ipc_notify_nsenders {
	ipc_port_t              ns_notify;
	mach_port_mscount_t     ns_mscount;
	boolean_t               ns_is_kobject;
} ipc_notify_nsenders_t;


/*!
 * @abstract
 * Send a dead-name notification.
 *
 * @discussion
 * A dead-name notification is sent when the port being monitored
 * has its receive right destroyed.
 *
 * Conditions:
 * - Nothing locked.
 * - Consumes a ref/soright for @c notify.
 *
 * @param notify        The port receiving the notification.
 * @param name          The name for the port whose receive right has been
 *                      destroyed.
 */
extern void ipc_notify_dead_name(
	ipc_port_t              notify,
	mach_port_name_t        name);

/*!
 * @abstract
 * Send a send-possible notification.
 *
 * @discussion
 * A send-possible notification is sent when the port being monitored
 * has a message queue that becomes non full, and messages can be sent
 * to it without blocking.
 *
 * This consumes the dead-name/send-possible notification slot for this port.
 *
 * Conditions:
 * - Nothing locked.
 * - Consumes a ref/soright for @c notify.
 *
 * @param notify        The port receiving the notification.
 * @param name          The name for the port which can now receive messages
 *                      without blocking.
 */
extern void ipc_notify_send_possible(
	ipc_port_t              notify,
	mach_port_name_t        name);

/*!
 * @abstract
 * Send a port-deleted notification.
 *
 * @discussion
 * A port-deleted notification is sent whenever the last send(-once) right
 * which has an active dead-name/send-possible notification armed is removed
 * from the space.
 *
 * Conditions:
 * - Nothing locked.
 * - Consumes a ref/soright for notify.
 *
 * @param notify        The port receiving the notification.
 * @param name          The name for the port which has been removed from the
 *                      space.
 */
extern void ipc_notify_port_deleted(
	ipc_port_t              notify,
	mach_port_name_t        name);

/*!
 * @abstract
 * Send a port-destroyed notification.
 *
 * @discussion
 * A port-destroyed notification allows for a task to get a receive right
 * back instead of it being destroyed.
 *
 * Conditions:
 * - Nothing locked.
 * - Consumes a ref/soright for @c notify.
 * - Consumes a ref for @c right, which should be a receive right
 *   prepped for placement into a message.  (In-transit, or in-limbo if
 *   a circularity was detected.)
 *
 * @param notify        The port receiving the notification.
 * @param right         The receive right being sent back.
 */
extern void ipc_notify_port_destroyed(
	ipc_port_t              notify,
	ipc_port_t              right);

/*!
 * @abstract
 * Send a no-senders notification to a regular message queue port.
 *
 * @discussion
 * Condition:
 * - Nothing locked.
 * - Consumes a ref/soright for @c notify.
 *
 * @param notify        The port receiving the notification.
 * @param mscount       The make-send count at the time this notification
 *                      was sent (it can be used to synchronize new rights
 *                      being made by the client concurrently).
 */
extern void ipc_notify_no_senders_mqueue(
	ipc_port_t              notify,
	mach_port_mscount_t     mscount);

/*!
 * @abstract
 * Send a no-senders notification to a kobject port.
 *
 * @discussion
 * Condition:
 * - Nothing locked.
 * - Consumes a port reference to @c notify.
 *
 * @param notify        The port receiving the notification,
 *                      which is also the port for which the notification
 *                      is being emitted.
 * @param mscount       The make-send count at the time this notification
 *                      was sent (it can be used to synchronize new rights
 *                      being made by the client concurrently).
 */
extern void ipc_notify_no_senders_kobject(
	ipc_port_t              notify,
	mach_port_mscount_t     mscount);

/*!
 * @abstract
 * Prepare for consuming a no-senders notification
 * when the port send right count just hit 0.
 *
 * @discussion
 * This allows for a two-phase prepare/emit because sending the no-senders
 * notification requires no lock to be held.
 *
 * @c ipc_notify_no_senders_emit() must be called on the value returned
 * by this function.
 *
 * Conditions:
 * - @c port is locked.
 *
 * For kobjects (ns_is_kobject), the `ns_notify` port has a port reference.
 * For regular ports, the `ns_notify` has an outstanding send once right.
 *
 * @returns
 * A token that must be passed to ipc_notify_no_senders_emit.
 */
extern ipc_notify_nsenders_t ipc_notify_no_senders_prepare(
	ipc_port_t              port);

/*!
 * @abstract
 * Emits a no-senders notification that was prepared by
 * @c ipc_notify_no_senders_prepare().
 */
static inline void
ipc_notify_no_senders_emit(ipc_notify_nsenders_t nsrequest)
{
	if (!nsrequest.ns_notify) {
		/* nothing to do */
	} else if (nsrequest.ns_is_kobject) {
		ipc_notify_no_senders_kobject(nsrequest.ns_notify,
		    nsrequest.ns_mscount);
	} else {
		ipc_notify_no_senders_mqueue(nsrequest.ns_notify,
		    nsrequest.ns_mscount);
	}
}

/* Send a send-once notification */
/*!
 * @abstract
 * Send a send-once notification.
 *
 * @discussion
 * A send-once notification is sent when a send-once right to @c port is being
 * destroyed without any message having been sent to it.
 *
 * Conditions:
 * - @c port is locked.
 * - Consumes a ref/soright for @c port.
 */
extern void ipc_notify_send_once_and_unlock(
	ipc_port_t              port);


#pragma GCC visibility pop
__ASSUME_PTR_ABI_SINGLE_END __END_DECLS

#endif  /* _IPC_IPC_NOTIFY_H_ */
