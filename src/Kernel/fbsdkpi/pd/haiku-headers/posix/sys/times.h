/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_TIMES_H
#define _SYS_TIMES_H


#include <time.h>


struct tms {
	clock_t tms_utime;		/* user CPU time */
	clock_t tms_stime;		/* system CPU time */
	clock_t tms_cutime;		/* user CPU time of terminated child processes */
	clock_t tms_cstime;		/* system CPU time of terminated child processes */
};


extern
#ifdef __cplusplus
"C"
#endif
clock_t times(struct tms *tms);

#endif	/* _SYS_TIMES_H */
