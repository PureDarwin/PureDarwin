/*
 * Copyright 2007 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_ALTQ_IF_ALTQ_H_
#define _FBSD_COMPAT_ALTQ_IF_ALTQ_H_


#include <sys/mbuf.h>
#include <sys/mutex.h>


struct ifaltq {
	struct mbuf*	ifq_head;
	struct mbuf*	ifq_tail;

	int				ifq_len;
	int				ifq_maxlen;
	int				ifq_drops;
	struct mtx		ifq_mtx;

	struct mbuf*	ifq_drv_head;
	struct mbuf*	ifq_drv_tail;
	int				ifq_drv_len;
	int				ifq_drv_maxlen;

	int				altq_flags;
};


#define ALTQF_READY	0x1

#define ALTDQ_REMOVE 1

#define ALTQ_IS_ENABLED(ifq)	0
#define ALTQ_ENQUEUE(ifr, m, foo, error) \
	do { m_freem(m); error = -1; } while (0)
#define ALTQ_DEQUEUE(ifr, m)	(m) = NULL
#define ALTQ_PURGE(ifq)

#define TBR_IS_ENABLED(ifq)		0
#define tbr_dequeue_ptr(ifq, v)	NULL

#endif
