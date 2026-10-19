/*
 * Copyright 2009-2018, Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_PRIORITY_H_
#define _FBSD_COMPAT_SYS_PRIORITY_H_

#include <OS.h>


#define PI_NET			(B_REAL_TIME_DISPLAY_PRIORITY - 9)
#define	PI_SOFT			(B_REAL_TIME_DISPLAY_PRIORITY - 1)

#define PRI_MIN_KERN	(64)
#define PZERO			(PRI_MIN_KERN + 20)
#define	PWAIT			(PRI_MIN_KERN + 28)


#endif /* _FBSD_COMPAT_SYS_PRIORITY_H_ */
