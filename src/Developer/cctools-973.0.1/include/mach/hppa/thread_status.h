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
 * @HP_COPYRIGHT@
 */
/*
 * HISTORY
 * Revision 1.1.1.1  1997/09/03 20:53:39  roland
 * Initial checkin of SGS release 244
 *
 * Revision 1.4.3.2  1992/01/09  20:05:31  sharpe
 * 	initial 1.1 vers from 1.0
 * 	[1992/01/09  19:29:20  sharpe]
 *
 * Revision 1.4  1991/07/03  17:25:42  osfrcs
 * 	06/19/90 rand       Add THREAD_STATE_FLAVOR_LIST to getstatus
 * 	[91/06/21  17:29:52  brezak]
 * 
 * Revision 1.3.2.2  91/06/21  18:05:17  brezak
 * 	06/19/90 rand       Add THREAD_STATE_FLAVOR_LIST to getstatus
 * 	[91/06/21  17:29:52  brezak]
 * 
 * Revision 1.2.2.2  91/04/30  09:48:00  brezak
 * 	rand         04/19/91 Add options to control reflection of assist/unalign exceptions
 * 	[91/04/29  11:46:12  brezak]
 * 
 * Revision 1.2  91/04/14  20:47:10  osfrcs
 * 	Initial version.
 * 	[91/03/30  09:32:42  brezak]
 * 
 */

#ifndef	_HPPA_THREAD_STATE_ 
#define	_HPPA_THREAD_STATE_

#include <mach/machine/boolean.h>


#define	HPPA_INTEGER_THREAD_STATE     1
#define	HPPA_FRAME_THREAD_STATE     2
#define	HPPA_FP_THREAD_STATE     3

/*
 * Flow control information that can
 * be changed from user state (with
 * some restrictions on psw).
 */
struct hp_pa_frame_thread_state {
	uint32_t	ts_pcsq_front;	/* instruction address space front */
	uint32_t	ts_pcsq_back;	/* instruction address space back */
	uint32_t	ts_pcoq_front;	/* instruction offset space front */
	uint32_t	ts_pcoq_back;	/* instruction offset space back */
	uint32_t	ts_psw;		/* process status word */
	uint32_t	ts_unaligned_faults;	/* number of unaligned data references READ-ONLY */
	uint32_t	ts_fault_address;	/* address of failing page fault READ-ONLY */
/*
 * A step range is a range of address that
 * will be executed with out generating a single
 * step event. If both values are 0 no stepping
 * will occur. Otherwise the program will run while:
 *
 *	if (step_range_start <= step_range_stop)
 *		pcoq0 >= step_range_start && pcoq0 < step_range_stop 
 *	if (step_range_start > step_range_stop)
 *		pcoq0 < step_range_stop && pcoq0 >= step_range_start 
 *
 * notice that setting step_range_start and step_range_stop to the
 * same non-zero value will execute only one instruction due to action
 * of the pc queue. (Yes, nullified instructions count)
 */
	uint32_t	ts_step_range_start;
	uint32_t	ts_step_range_stop;

	/* Generate an exception when OS assists with an alignment fault */
	boolean_t	ts_alignment_trap_reflect;

	/* Generate an exception when OS assists with an FP fault */
	boolean_t	ts_execution_trap_reflect;
};

/*
 * Get rid of as soon as all users of frame_thread_state 
 * have been recompiled. XXX
 */
struct hp_pa_old_frame_thread_state {
	uint32_t	ts_pcsq_front;	/* instruction address space front */
	uint32_t	ts_pcsq_back;	/* instruction address space back */
	uint32_t	ts_pcoq_front;	/* instruction offset space front */
	uint32_t	ts_pcoq_back;	/* instruction offset space back */
	uint32_t	ts_psw;		/* process status word */
};

/*
 * The hp_pa_integer_thread_state that may be changed by any
 * process in user space.
 */
typedef struct hp_pa_integer_thread_state {
	uint32_t	ts_gr1;		/* the user's general registers */
	uint32_t	ts_gr2;
	uint32_t	ts_gr3;
	uint32_t	ts_gr4;
	uint32_t	ts_gr5;
	uint32_t	ts_gr6;
	uint32_t	ts_gr7;
	uint32_t	ts_gr8;
	uint32_t	ts_gr9;
	uint32_t	ts_gr10;
	uint32_t	ts_gr11;
	uint32_t	ts_gr12;
	uint32_t	ts_gr13;
	uint32_t	ts_gr14;
	uint32_t	ts_gr15;
	uint32_t	ts_gr16;
	uint32_t	ts_gr17;
	uint32_t	ts_gr18;
	uint32_t	ts_gr19;
	uint32_t	ts_gr20;
	uint32_t	ts_gr21;
	uint32_t	ts_gr22;
	uint32_t	ts_gr23;
	uint32_t	ts_gr24;
	uint32_t	ts_gr25;
	uint32_t	ts_gr26;
	uint32_t	ts_gr27;
	uint32_t	ts_gr28;
	uint32_t	ts_gr29;
	uint32_t	ts_gr30;
	uint32_t	ts_gr31;
	uint32_t	ts_sr0;		/* the user's space registgers */
	uint32_t	ts_sr1;
	uint32_t	ts_sr2;
	uint32_t	ts_sr3;
	uint32_t	ts_sar;		/* the user's shift amount register */
} hp_pa_integer_thread_state_t;

/*
 * The floating point state that may be changed by any
 * process in user space.
 */
typedef struct hp_pa_fp_thread_state {
	double	ts_fp0;		/* all of the execution unit registers */
	double	ts_fp1;
	double	ts_fp2;
	double	ts_fp3;
	double	ts_fp4;
	double	ts_fp5;
	double	ts_fp6;
	double	ts_fp7;
	double	ts_fp8;
	double	ts_fp9;
	double	ts_fp10;
	double	ts_fp11;
	double	ts_fp12;
	double	ts_fp13;
	double	ts_fp14;
	double	ts_fp15;
	double	ts_fp16;
	double	ts_fp17;
	double	ts_fp18;
	double	ts_fp19;
	double	ts_fp20;
	double	ts_fp21;
	double	ts_fp22;
	double	ts_fp23;
	double	ts_fp24;
	double	ts_fp25;
	double	ts_fp26;
	double	ts_fp27;
	double	ts_fp28;
	double	ts_fp29;
	double	ts_fp30;
	double	ts_fp31;
} hp_pa_fp_thread_state_t;

#define	HPPA_INTEGER_THREAD_STATE_COUNT (sizeof(struct hp_pa_integer_thread_state) / sizeof(uint32_t))
#define	HPPA_FRAME_THREAD_STATE_COUNT (sizeof(struct hp_pa_frame_thread_state) / sizeof(uint32_t))
#define	HPPA_FP_THREAD_STATE_COUNT (sizeof(struct hp_pa_fp_thread_state) / sizeof(uint32_t))

/* Get rid of as soon as all users of thread_frame_state have been recompiled XXX */
#define	HPPA_OLD_FRAME_THREAD_STATE_COUNT (sizeof(struct hp_pa_old_frame_thread_state) / sizeof(uint32_t))

#endif	/* _HPPA_THREAD_STATE_ */
