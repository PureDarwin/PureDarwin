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
/* Copyright (c) 1998, 1999 Apple Computer, Inc. All Rights Reserved */
/*!
 *       @header kern_event.h
 *       This header defines in-kernel functions for generating kernel events as
 *       well as functions for receiving kernel events using a kernel event
 *       socket.
 */

#ifndef SYS_KERN_EVENT_PRIVATE_H
#define SYS_KERN_EVENT_PRIVATE_H

#include <sys/kern_event.h>

struct xkevtpcb {
	u_int32_t       kep_len;
	u_int32_t       kep_kind;
	u_int64_t       kep_evtpcb;
	u_int32_t       kep_vendor_code_filter;
	u_int32_t       kep_class_filter;
	u_int32_t       kep_subclass_filter;
};

struct kevtstat {
	u_int64_t       kes_pcbcount __attribute__((aligned(8)));
	u_int64_t       kes_gencnt __attribute__((aligned(8)));
	u_int64_t       kes_badvendor __attribute__((aligned(8)));
	u_int64_t       kes_toobig __attribute__((aligned(8)));
	u_int64_t       kes_nomem __attribute__((aligned(8)));
	u_int64_t       kes_fullsock __attribute__((aligned(8)));
	u_int64_t       kes_posted __attribute__((aligned(8)));
};

#ifdef KERNEL
/*
 * Internal version of kev_msg_post. Allows posting Apple vendor code kernel
 * events.
 */
int     kev_post_msg(struct kev_msg *event);
int     kev_post_msg_nowait(struct kev_msg *event);

LIST_HEAD(kern_event_head, kern_event_pcb);

struct kern_event_pcb {
	decl_lck_mtx_data(, evp_mtx);   /* per-socket mutex */
	LIST_ENTRY(kern_event_pcb) evp_link;    /* glue on list of all PCBs */
	struct socket *evp_socket;              /* pointer back to socket */
	u_int32_t evp_vendor_code_filter;
	u_int32_t evp_class_filter;
	u_int32_t evp_subclass_filter;
};

#define sotoevpcb(so)   ((struct kern_event_pcb *)((so)->so_pcb))

#endif /* KERNEL */
#endif /* SYS_KERN_EVENT_PRIVATE_H */
