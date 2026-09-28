/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
 */
/*
 * Copyright (c) 1997, Apple Computer, Inc. All rights reserved.
 *
 */

#ifndef _BSD_RISCV_PROFILE_H_
#define _BSD_RISCV_PROFILE_H_

#if defined (__riscv)

#include <sys/appleapiopts.h>

#ifdef KERNEL
#ifdef __APPLE_API_UNSTABLE

// Block interrupts during mcount so they can also be counted. splhigh/splx are
// C routines on riscv and can recursively invoke mcount.
#warning MCOUNT_* not implemented yet.

#define MCOUNT_INIT
#define MCOUNT_ENTER    /* s = splhigh(); */ /* XXX TODO */
#define MCOUNT_EXIT     /* (void) splx(s); */ /* XXX TODO */

#endif /* __APPLE_API_UNSTABLE */
#endif /* KERNEL */

#endif /* defined (__riscv) */

#endif /* _BSD_RISCV_PROFILE_H_ */
