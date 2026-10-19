/*
 * Copyright 2018-2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT license.
 */
#ifndef _FBSD_COMPAT_MACHINE_BUS_H_
#define _FBSD_COMPAT_MACHINE_BUS_H_

#include <machine/_bus.h>
#include <machine/cpufunc.h>

#if defined(__i386__) || defined(__amd64__)
#  include <machine/x86/bus.h>
#elif (defined(__riscv) && __riscv_xlen == 64) || defined(__aarch64__)
#  include <machine/generic/bus.h>
#else
#  error Need a bus.h for this arch!
#endif

#endif /* _FBSD_COMPAT_MACHINE_BUS_H_ */
