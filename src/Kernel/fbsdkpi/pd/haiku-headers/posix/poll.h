/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _POLL_H
#define _POLL_H


#include <signal.h>
#include <sys/time.h>


typedef unsigned long nfds_t;

struct pollfd {
	int		fd;
	short	events;		/* events to look for */
	short	revents;	/* events that occured */
};

/* events & revents - compatible with the B_SELECT_xxx definitions in Drivers.h */
#define	POLLIN		0x0001		/* any readable data available */
#define	POLLOUT		0x0002		/* file descriptor is writeable */
#define	POLLRDNORM	POLLIN
#define	POLLWRNORM	POLLOUT
#define	POLLRDBAND	0x0008		/* priority readable data */
#define	POLLWRBAND	0x0010		/* priority data can be written */
#define	POLLPRI		0x0020		/* high priority readable data */

/* revents only */
#define	POLLERR		0x0004		/* errors pending */
#define	POLLHUP		0x0080		/* disconnected */
#define	POLLNVAL	0x1000		/* invalid file descriptor */


#ifdef __cplusplus
extern "C" {
#endif

extern int poll(struct pollfd *fds, nfds_t numfds, int timeout);
extern int ppoll(struct pollfd *fds, nfds_t numfds,
	const struct timespec *timeout, const sigset_t *sigMask);

#ifdef __cplusplus
}
#endif



#endif /* _POLL_H */
