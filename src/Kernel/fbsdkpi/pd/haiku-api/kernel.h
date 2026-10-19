// SPDX-License-Identifier: MIT
// haiku's private kernel macros
#ifndef _PD_HAIKU_KERNEL_H
#define _PD_HAIKU_KERNEL_H
#include <OS.h>
#ifndef ROUNDUP
#define ROUNDUP(a, b)	(((a) + ((b) - 1)) / (b) * (b))
#endif
#ifndef ROUNDDOWN
#define ROUNDDOWN(a, b)	(((a) / (b)) * (b))
#endif
#endif
