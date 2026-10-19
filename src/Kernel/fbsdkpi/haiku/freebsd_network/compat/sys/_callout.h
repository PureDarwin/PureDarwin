/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS__CALLOUT_H_
#define _FBSD_COMPAT_SYS__CALLOUT_H_


#include <util/list.h>
#include <sys/queue.h>
#include <sys/_mutex.h>


struct callout {
	struct list_link	c_link;
	bigtime_t			c_due;

	void *				c_arg;
	void				(*c_func)(void *);
	struct mtx *		c_mtx;
	int					c_flags;
};


#endif /* _FBSD_COMPAT_SYS__CALLOUT_H_ */
