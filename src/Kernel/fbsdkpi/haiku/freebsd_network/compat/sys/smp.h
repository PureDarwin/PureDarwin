/*
 * Copyright 2014-2019, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SMP_H_
#define _FBSD_COMPAT_SYS_SMP_H_


extern u_int mp_maxid;
extern int mp_ncpus;


static inline int
cpu_first(void)
{
	return 0;
}

static inline int
cpu_next(int i)
{
	i++;
	if (i > (int)mp_maxid)
		i = 0;
	return i;
}

#define	CPU_FIRST()	cpu_first()
#define	CPU_NEXT(i)	cpu_next((i))

#define MAXCPU 1
#define curcpu 0


#endif
