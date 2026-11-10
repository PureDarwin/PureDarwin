// SPDX-License-Identifier: MIT
// the few freebsd and netbsd kernel names the vendored tegra210 car and polled i2c code use, over xnu

#ifndef _PD_TEGRA_BSD_SHIM_H_
#define _PD_TEGRA_BSD_SHIM_H_

#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <machine/limits.h>

extern void delay(int usec);
extern int printf(const char *fmt, ...);

#ifndef DELAY
#define DELAY(us)	delay(us)
#endif

#ifndef nitems
#define nitems(x)	(sizeof(x) / sizeof((x)[0]))
#endif
#ifndef UINT_MAX
#define UINT_MAX	0xffffffffU
#endif

#endif
