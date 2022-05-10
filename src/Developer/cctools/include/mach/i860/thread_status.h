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
 * Copyright (c) 1987, 1988 NeXT, Inc.
 */ 

#ifndef	_I860_THREAD_STATE_
#define	_I860_THREAD_STATE_

/*
 * I860_thread_state_regs		this is the structure that is exported
 *					to user threads for use in set/get
 *					status calls.  This structure should
 *					never change.
 */

#define	I860_THREAD_STATE_REGS	(4)	/* normal registers */

struct i860_thread_state_regs {
	int	ireg[31];  /* core registers (incl stack pointer, but not r0) */
	int	freg[30];  /* FPU registers, except f0 and f1 */
	int	psr;	   /* user's processor status register */
	int	epsr;	   /* user's extended processor status register */
	int	db;	   /* user's data breakpoint register */
	int	pc;	   /* user's program counter */
	int	_padding_; /* not used */
	/* Pipeline state for FPU */
	double	Mres3;
	double	Ares3;
	double	Mres2;
	double	Ares2;
	double	Mres1;
	double	Ares1;
	double	Ires1;
	double	Lres3m;
	double	Lres2m;
	double	Lres1m;
	double	KR;
	double	KI;
	double	T;
	int	Fsr3;
	int 	Fsr2;
	int	Fsr1;
	int	Mergelo32;
	int	Mergehi32;
};

#define	I860_THREAD_STATE_REGS_COUNT \
	(sizeof (struct i860_thread_state_regs) / sizeof (int))

#endif	/* _I860_THREAD_STATE_ */
