/*
 * Copyright 2007-2009 Haiku Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _FBSD_COMPAT_SYS_SOCKIO_H_
#define _FBSD_COMPAT_SYS_SOCKIO_H_


#include <posix/sys/sockio.h>

#include <sys/ioccom.h>


#define SIOCSIFCAP	SIOCSPACKETCAP

#define SIOCSIFLLADDR			8950	/* set linklevel addr */

#endif
