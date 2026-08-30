/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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
 *	File:	ipc/ipc_notify.c
 *	Author:	Rich Draves
 *	Date:	1989
 *
 *	Notification-sending functions.
 */

#include <mach/port.h>
#include <mach/mach_notify.h>
#include <kern/ipc_kobject.h>
#include <ipc/ipc_notify.h>
#include <ipc/ipc_policy.h>

/*!
 * @abstract
 * Perform a check on whether forming a notification message
 * to the specified notification port can be elided.
 *
 * @discussion
 * This is racy but helps avoiding costly messages to be formed
 * just to be destroyed because the notification port is already
 * dead.
 *
 * This happens quite a lot during ipc_space_terminate(): all
 * receive rights are destroyed first, then other ports.
 * This avoids sending notifications to receive rights in that
 * space reliably.
 */
static inline bool
ipc_notify_should_send(ipc_port_t notification_port)
{
	return ip_active(notification_port);
}

void
ipc_notify_dead_name(ipc_port_t port, mach_port_name_t name)
{
	if (ipc_notify_should_send(port)) {
		(void)mach_notify_dead_name(port, name);
		/* send-once right consumed */
	} else {
		ipc_port_release_sonce(port);
	}
}

void
ipc_notify_send_possible(ipc_port_t port, mach_port_name_t name)
{
	if (ipc_notify_should_send(port)) {
		(void)mach_notify_send_possible(port, name);
		/* send-once right consumed */
	} else {
		ipc_port_release_sonce(port);
	}
}

void
ipc_notify_port_deleted(ipc_port_t port, mach_port_name_t name)
{
	if (ipc_notify_should_send(port)) {
		(void)mach_notify_port_deleted(port, name);
		/* send-once right consumed */
	} else {
		ipc_port_release_sonce(port);
	}
}

void
ipc_notify_port_destroyed(ipc_port_t port, ipc_port_t right)
{
	mach_notify_port_destroyed(port, right);
	/* send-once and receive rights consumed */
}

ipc_notify_nsenders_t
ipc_notify_no_senders_prepare(ipc_port_t port)
{
	ipc_notify_nsenders_t req = { };
	ipc_object_type_t type = ip_type(port);

	ip_mq_lock_held(port);

	if (io_is_kobject_type(type)) {
		if (ip_active(port) && ipc_policy(type)->pol_notif_no_senders) {
			ip_reference(port);
			req.ns_notify = port;
			req.ns_mscount = port->ip_mscount;
			req.ns_is_kobject = true;
		}
	} else if (port->ip_nsrequest) {
		ipc_release_assert(ipc_policy(type)->pol_notif_no_senders);
		req.ns_notify = port->ip_nsrequest;
		req.ns_mscount = port->ip_mscount;
		req.ns_is_kobject = false;

		port->ip_nsrequest = IP_NULL;
	}

	return req;
}

void
ipc_notify_no_senders_mqueue(ipc_port_t port, mach_port_mscount_t mscount)
{
	if (ipc_notify_should_send(port)) {
		(void)mach_notify_no_senders(port, mscount);
		/* send-once right consumed */
	} else {
		ipc_port_release_sonce(port);
	}
}

void
ipc_notify_no_senders_kobject(ipc_port_t port, mach_port_mscount_t mscount)
{
	if (ipc_notify_should_send(port)) {
		ipc_policy(port)->pol_kobject_no_senders(port, mscount);
	}
	ip_release(port);
}

void
ipc_notify_send_once_and_unlock(ipc_port_t port)
{
	/*
	 * clear any reply context:
	 * no one will be sending the response b/c we are destroying
	 * the single, outstanding send once right.
	 */
	port->ip_reply_context = 0;

	if (!ip_active(port)) {
		ipc_port_release_sonce_and_unlock(port);
	} else if (ip_in_space(port, ipc_space_kernel)) {
		ipc_kobject_notify_send_once_and_unlock(port);
	} else if (ip_full_kernel(port)) {
		ipc_port_release_sonce_and_unlock(port);
	} else {
		ip_mq_unlock(port);
		(void)mach_notify_send_once(port);
	}
	/* send-once right consumed */
}
