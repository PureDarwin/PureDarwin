/*
 * Copyright (c) 2004, Apple Computer, Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*
 *  Copyright (c) 1994 by Sun Microsystems, Inc
 */

#ifndef	_MACH_SPARC_THREAD_STATUS_H_
#define	_MACH_SPARC_THREAD_STATUS_H_

#include <architecture/sparc/reg.h>

/*
 *	sparc_thread_state_regs
 *		This is the structure that is exported
 *      to user threads for use in set/get status
 *      calls.  This structure should never change.
 *		The "local" and "in" registers of the corresponding 
 *		register window	are saved in the stack frame pointed
 *		to by sp -> %o6.
 *
 *	sparc_thread_state_fpu
 *		This is the structure that is exported
 *      to user threads for use in set/get FPU register 
 *		status calls.
 *
 */

#define	SPARC_THREAD_STATE_REGS	1

struct sparc_thread_state_regs {
	struct regs regs;
};

#define	SPARC_THREAD_STATE_REGS_COUNT \
			(sizeof(struct sparc_thread_state_regs) / sizeof(int))

/*
 *	Floating point unit registers
 */

#define SPARC_THREAD_STATE_FPU	2


struct sparc_thread_state_fpu {
	struct fpu fpu;	/* floating point registers/status */
};

#define	SPARC_THREAD_STATE_FPU_COUNT \
			(sizeof(struct sparc_thread_state_fpu) / sizeof(int))

#define	SPARC_THREAD_STATE_FLAVOR_COUNT  2

#define SPARC_THREAD_STATE_FLAVOR_LIST_COUNT         \
	( SPARC_THREAD_STATE_FLAVOR_COUNT *              \
		(sizeof (struct thread_state_flavor) / sizeof(int)))

#endif	/* _MACH_SPARC_THREAD_STATUS_H_ */
