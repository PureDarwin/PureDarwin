/*
 * Copyright 2018-2019 Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_RISCV64_SIGNAL_H_
#define _ARCH_RISCV64_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 */

#if (defined(__riscv) && __riscv_xlen == 64)
struct vregs {
	ulong x[31];
	ulong pc;
	double f[32];
	ulong fcsr;
};
#endif /* (defined(__riscv) && __riscv_xlen == 64) */


#endif /* _ARCH_RISCV64_SIGNAL_H_ */
