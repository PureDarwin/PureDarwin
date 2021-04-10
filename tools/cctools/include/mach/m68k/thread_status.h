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
 *
 * HISTORY
 * 15-May-91  Gregg Kellogg (gk) at NeXT
 *	Use m68k_saved_state instead of NeXT_saved_state.
 *	Use m68k_thread_state_regs NeXT_regs.
 *	Use m68k_thread_state_68882 NeXT_thread_state_68882.
 *	Use m68k_thread_state_user_reg NeXT_thread_state_user_reg.
 *	Moved m68k_saved_state and USER_REGS to pcb.h.
 *
 */ 

#ifndef	_MACH_M68K_THREAD_STATUS_
#define	_MACH_M68K_THREAD_STATUS_

/*
 *	m68k_thread_state_regs	this is the structure that is exported
 *				to user threads for use in set/get status
 *				calls.  This structure should never
 *				change.
 *
 *	m68k_thread_state_68882	this structure is exported to user threads
 *				to allow the to set/get 68882 floating
 *				pointer register state.
 *
 *	m68k_saved_state	this structure corresponds to the state
 *				of the user registers as saved on the
 *				stack upon kernel entry.  This structure
 *				is used internally only.  Since this
 *				structure may change from version to
 *				version, it is hidden from the user.
 */

#define	M68K_THREAD_STATE_REGS	(1)	/* normal registers */
#define M68K_THREAD_STATE_68882	(2)	/* 68882 registers */
#define M68K_THREAD_STATE_USER_REG (3)	/* additional user register */

#define M68K_THREAD_STATE_MAXFLAVOR (3)

struct m68k_thread_state_regs {
	int	dreg[8];	/* data registers */
	int	areg[8];	/* address registers (incl stack pointer) */
	short	pad0;		/* not used */
	short	sr;		/* user's status register */
	int	pc;		/* user's program counter */
};

#define	M68K_THREAD_STATE_REGS_COUNT \
	(sizeof (struct m68k_thread_state_regs) / sizeof (int))

struct m68k_thread_state_68882 {
	struct {
		int	fp[3];		/* 96-bit extended format */
	} regs[8];
	int	cr;			/* control */
	int	sr;			/* status */
	int	iar;			/* instruction address */
	int	state;			/* execution state */
};

#define	M68K_THREAD_STATE_68882_COUNT \
	(sizeof (struct m68k_thread_state_68882) / sizeof (int))

struct m68k_thread_state_user_reg {
	int	user_reg;		/* user register (used by cthreads) */
};

#define M68K_THREAD_STATE_USER_REG_COUNT \
	(sizeof (struct m68k_thread_state_user_reg) / sizeof (int))

#endif	/* _MACH_M68K_THREAD_STATUS_ */
