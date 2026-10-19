/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_IFLIB_SYS__TASK_H_
#define _FBSD_IFLIB_SYS__TASK_H_

/* include the real sys/_task.h */
#include_next <sys/_task.h>


#include <sys/queue.h>


typedef void gtask_fn_t(void *context);

struct gtask {
	STAILQ_ENTRY(gtask) ta_link;	/* (q) link for queue */
	uint16_t ta_flags;		/* (q) state flags */
	u_short	ta_priority;		/* (c) Priority */
	gtask_fn_t *ta_func;		/* (c) task handler */
	void	*ta_context;		/* (c) argument for handler */
};


#endif
