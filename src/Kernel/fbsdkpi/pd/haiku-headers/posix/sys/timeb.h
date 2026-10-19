/*
 * Copyright 2002-2012 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYS_TIMEB_H
#define _SYS_TIMEB_H


#include <time.h>


struct timeb {
	time_t			time;		/* seconds of current time */
	unsigned short	millitm;	/* milliseconds of current time */
	short			timezone;	/* timezone difference to GMT in minutes */
	short			dstflag;	/* daylight saving flag */
};


extern
#ifdef __cplusplus
"C"
#endif
int ftime(struct timeb *timeb);
	/* legacy */

#endif	/* _SYS_TIMEB_H */
