/*
 * Copyright 2008-2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _SCHED_H_
#define _SCHED_H_


#ifdef __cplusplus
extern "C" {
#endif


#define SCHED_FIFO		1
#define SCHED_RR		2
#define SCHED_SPORADIC	3
#define SCHED_OTHER		4

struct sched_param {
	int sched_priority;
};


extern int sched_yield(void);
extern int sched_get_priority_min(int);
extern int sched_get_priority_max(int);

#ifdef __cplusplus
}
#endif

#endif  /* _SCHED_H_ */
