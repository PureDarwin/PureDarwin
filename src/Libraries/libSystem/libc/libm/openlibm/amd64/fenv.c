/*-
 * Copyright (c) 2004-2005 David Schultz <das@FreeBSD.ORG>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/msun/amd64/fenv.c,v 1.8 2011/10/21 06:25:31 das Exp $
 */

#include "bsd_fpu.h"
#include "math_private.h"

#ifdef _WIN32
#define __fenv_static OLM_DLLEXPORT
#endif
#include <openlibm_fenv.h>

#ifdef __GNUC_GNU_INLINE__
#error "This file must be compiled with C99 'inline' semantics"
#endif

#ifdef __PUREDARWIN__
// _sjc_ Check whether we need to set this default structure for PureDarwin
#else
const fenv_t __fe_dfl_env = {
	{ 0xffff0000 | __INITIAL_FPUCW__,
	  0xffff0000,
	  0xffffffff,
	  { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff }
	},
	__INITIAL_MXCSR__
};
#endif

#ifdef __PUREDARWIN__
// _sjc_ From Apple's Libm-2026
typedef struct {
    unsigned short __control;
    unsigned short __reserved1;
    unsigned short __status;
    unsigned short __reserved2;
    unsigned int __private3;
    unsigned int __private4;
    unsigned int __private5;
    unsigned int __private6;
    unsigned int __private7;
} __fpustate_t;

#define FE_ALL_RND  ( FE_TONEAREST | FE_TOWARDZERO | FE_UPWARD | FE_DOWNWARD )

static inline int _fesetexceptflag(const fexcept_t *flagp, int excepts ); // _sjc_ ALWAYS_INLINE;
static inline int _fesetexceptflag(const fexcept_t *flagp, int excepts )
{
    int state;
    __fpustate_t currfpu;
    unsigned int mxcsr;
    unsigned int exceptMask = excepts & FE_ALL_EXCEPT;
    unsigned int andMask = ~exceptMask;                     // clear just the bits indicated
    unsigned int orMask =  *flagp & exceptMask;             // latch the specified bits

    //read the state
    mxcsr = _mm_getcsr();                                   //read the MXCSR state
    __asm __volatile ("fnstenv %0" : "=m" (currfpu) );          //read x87 state

    //fix up the MXCSR state
    mxcsr &= andMask;
    mxcsr |= orMask;

    //fix up the x87 state
    state = currfpu.__status;
    state &= andMask;
    state |= orMask;
    currfpu.__status = state;

    //store the state
    __asm __volatile ("ldmxcsr %0 ; fldenv %1" : : "m" (mxcsr), "m" (currfpu));
    return 0;
}

static inline int _fegetexceptflag(fexcept_t *flagp, int excepts);
static inline int _fegetexceptflag(fexcept_t *flagp, int excepts)
{
    fexcept_t fsw = GET_FSW();              //get the x87 status word
    unsigned int mxcsr = _mm_getcsr();      //get the mxcsr
    fexcept_t result = mxcsr | fsw;

    result &= excepts & FE_ALL_EXCEPT;

    *flagp = result;
    return 0;
}
#endif

#ifdef __PUREDARWIN__
int  feclearexcept(int excepts)
{
    fexcept_t zero = 0;
    return _fesetexceptflag( &zero, excepts );
}
#else
extern inline OLM_DLLEXPORT int feclearexcept(int __excepts);
#endif

extern inline OLM_DLLEXPORT int fegetexceptflag(fexcept_t *__flagp, int __excepts);

#ifdef __PUREDARWIN__
int  fesetexceptflag(const fexcept_t *flagp, int excepts )
{
    return _fesetexceptflag( flagp, excepts );
}
#else
OLM_DLLEXPORT int
fesetexceptflag(const fexcept_t *flagp, int excepts)
{
	fenv_t env;

	__fnstenv(&env.__x87);
	env.__x87.__status &= ~excepts;
	env.__x87.__status |= *flagp & excepts;
	__fldenv(env.__x87);

	__stmxcsr(&env.__mxcsr);
	env.__mxcsr &= ~excepts;
	env.__mxcsr |= *flagp & excepts;
	__ldmxcsr(env.__mxcsr);

	return (0);
}
#endif

OLM_DLLEXPORT int
feraiseexcept(int excepts)
{
	fexcept_t ex = excepts;

	fesetexceptflag(&ex, excepts);
	__fwait();
	return (0);
}

extern inline OLM_DLLEXPORT int fetestexcept(int __excepts);
extern inline OLM_DLLEXPORT int fegetround(void);
extern inline OLM_DLLEXPORT int fesetround(int __round);

#ifdef __PUREDARWIN__
int  fegetenv(fenv_t *envp)
{
    __fpustate_t currfpu;
    int mxcsr = _mm_getcsr();

    __asm __volatile ("fnstenv %0" : "=m" (currfpu) :: "memory");

    envp->__control = currfpu.__control;
    envp->__status = currfpu.__status;
    envp->__mxcsr = mxcsr;
    ((int*) envp->__reserved)[0] = 0;
    ((int*) envp->__reserved)[1] = 0;

    // fnstenv masks floating-point exceptions.  We restore the state here
    // in case any exceptions were originally unmasked.
    __asm __volatile ("fldenv %0" : : "m" (currfpu));

    return 0;
}
#else
OLM_DLLEXPORT int
fegetenv(fenv_t *envp)
{

	__fnstenv(&envp->__x87);
	__stmxcsr(&envp->__mxcsr);
	/*
	 * fnstenv masks all exceptions, so we need to restore the
	 * control word to avoid this side effect.
	 */
	__fldcw(envp->__x87.__control);
	return (0);
}
#endif

#ifdef __PUREDARWIN__
int   feholdexcept(fenv_t *envp)
{
    __fpustate_t currfpu;
    int mxcsr;

    mxcsr = _mm_getcsr();
    __asm __volatile ("fnstenv %0" : "=m" (*&currfpu) :: "memory");

    envp->__control = currfpu.__control;
    envp->__status = currfpu.__status;
    envp->__mxcsr = mxcsr;
    ((int*) envp->__reserved)[0] = 0;
    ((int*) envp->__reserved)[1] = 0;

    currfpu.__control |= FE_ALL_EXCEPT; // FPU shall handle all exceptions
    currfpu.__status &= ~FE_ALL_EXCEPT;
    mxcsr |= FE_ALL_EXCEPT << 7;  // left shifted because control mask is <<7 of the flags
    mxcsr &= ~FE_ALL_EXCEPT;

    __asm __volatile ("ldmxcsr %0 ; fldenv %1" : : "m" (*&mxcsr), "m" (*&currfpu));

    return 0;
}
#else
OLM_DLLEXPORT int
feholdexcept(fenv_t *envp)
{
	uint32_t mxcsr;

	__stmxcsr(&mxcsr);
	__fnstenv(&envp->__x87);
	__fnclex();
	envp->__mxcsr = mxcsr;
	mxcsr &= ~FE_ALL_EXCEPT;
	mxcsr |= FE_ALL_EXCEPT << _SSE_EMASK_SHIFT;
	__ldmxcsr(mxcsr);
	return (0);
}
#endif

#ifdef __PUREDARWIN__
int  fesetenv(const fenv_t *envp)
{
    __fpustate_t currfpu;
    __asm __volatile ("fnstenv %0" : "=m" (currfpu));

    currfpu.__control = envp->__control;
    currfpu.__status = envp->__status;

    __asm __volatile ("ldmxcsr %0 ; fldenv %1" : : "m" (envp->__mxcsr), "m" (currfpu));
    return 0;
}
#else
extern inline OLM_DLLEXPORT int fesetenv(const fenv_t *__envp);
#endif

OLM_DLLEXPORT int
feupdateenv(const fenv_t *envp)
{
	uint32_t mxcsr;
	uint16_t status;

	__fnstsw(&status);
	__stmxcsr(&mxcsr);
	fesetenv(envp);
	feraiseexcept((mxcsr | status) & FE_ALL_EXCEPT);
	return (0);
}

int
feenableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	mask &= FE_ALL_EXCEPT;
	__fnstcw(&control);
	__stmxcsr(&mxcsr);
	omask = ~(control | mxcsr >> _SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control &= ~mask;
	__fldcw(control);
	mxcsr &= ~(mask << _SSE_EMASK_SHIFT);
	__ldmxcsr(mxcsr);
	return (omask);
}

int
fedisableexcept(int mask)
{
	uint32_t mxcsr, omask;
	uint16_t control;

	mask &= FE_ALL_EXCEPT;
	__fnstcw(&control);
	__stmxcsr(&mxcsr);
	omask = ~(control | mxcsr >> _SSE_EMASK_SHIFT) & FE_ALL_EXCEPT;
	control |= mask;
	__fldcw(control);
	mxcsr |= mask << _SSE_EMASK_SHIFT;
	__ldmxcsr(mxcsr);
	return (omask);
}
