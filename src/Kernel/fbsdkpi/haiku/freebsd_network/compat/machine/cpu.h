/*
 * Copyright 2018, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _FBSD_COMPAT_MACHINE_CPU_H_
#define _FBSD_COMPAT_MACHINE_CPU_H_


#include <arch_cpu_defs.h>


#define cpu_spinwait()		SPINLOCK_PAUSE()


#endif /* _FBSD_COMPAT_MACHINE_CPU_H_ */
