#ifndef	_FENV_H_
#define	_FENV_H_

#include <stdint.h>

#include "cdefs-compat.h"

#ifndef	__fenv_static
#define	__fenv_static	static
#endif

// the whole environment is the fcsr, fflags in bits 4:0 and frm in bits 7:5
typedef	uint32_t	fenv_t;
typedef	uint32_t	fexcept_t;

// exception flags, the fflags bit positions
#define	FE_INEXACT	0x0001
#define	FE_UNDERFLOW	0x0002
#define	FE_OVERFLOW	0x0004
#define	FE_DIVBYZERO	0x0008
#define	FE_INVALID	0x0010
#define	FE_ALL_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | \
			 FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

// rounding modes, the frm encodings
#define	FE_TONEAREST	0x0000
#define	FE_TOWARDZERO	0x0001
#define	FE_DOWNWARD	0x0002
#define	FE_UPWARD	0x0003
#define	_ROUND_MASK	(FE_TONEAREST | FE_DOWNWARD | \
			 FE_UPWARD | FE_TOWARDZERO)

__BEGIN_DECLS

// default floating-point environment
extern const fenv_t	__fe_dfl_env;
#define	FE_DFL_ENV	(&__fe_dfl_env)

#define	__rfcsr(__r)	__asm __volatile("frcsr %0" : "=r" (__r))
#define	__wfcsr(__r)	__asm __volatile("fscsr %0" : : "r" (__r))
#define	__rflags(__r)	__asm __volatile("frflags %0" : "=r" (__r))
#define	__clrflags(__m)	__asm __volatile("csrc fflags, %0" : : "r" (__m))
#define	__setflags(__m)	__asm __volatile("csrs fflags, %0" : : "r" (__m))
#define	__rround(__r)	__asm __volatile("frrm %0" : "=r" (__r))
#define	__wround(__r)	__asm __volatile("fsrm %0" : : "r" (__r))

__fenv_static inline int
feclearexcept(int __excepts)
{
	__clrflags(__excepts & FE_ALL_EXCEPT);
	return (0);
}

__fenv_static inline int
fegetexceptflag(fexcept_t *__flagp, int __excepts)
{
	fexcept_t __flags;

	__rflags(__flags);
	*__flagp = __flags & __excepts;
	return (0);
}

__fenv_static inline int
fesetexceptflag(const fexcept_t *__flagp, int __excepts)
{
	__excepts &= FE_ALL_EXCEPT;
	__clrflags(__excepts);
	__setflags(*__flagp & __excepts);
	return (0);
}

// riscv never traps on fp exceptions, raising one only sets its flag
__fenv_static inline int
feraiseexcept(int __excepts)
{
	__setflags(__excepts & FE_ALL_EXCEPT);
	return (0);
}

__fenv_static inline int
fetestexcept(int __excepts)
{
	fexcept_t __flags;

	__rflags(__flags);
	return (__flags & __excepts);
}

__fenv_static inline int
fegetround(void)
{
	uint32_t __round;

	__rround(__round);
	return (__round & _ROUND_MASK);
}

__fenv_static inline int
fesetround(int __round)
{
	if (__round & ~_ROUND_MASK)
		return (-1);
	__wround(__round);
	return (0);
}

__fenv_static inline int
fegetenv(fenv_t *__envp)
{
	__rfcsr(*__envp);
	return (0);
}

// without traps the non-stop mode is always on, only the flags need clearing
__fenv_static inline int
feholdexcept(fenv_t *__envp)
{
	__rfcsr(*__envp);
	__clrflags(FE_ALL_EXCEPT);
	return (0);
}

__fenv_static inline int
fesetenv(const fenv_t *__envp)
{
	__wfcsr(*__envp);
	return (0);
}

__fenv_static inline int
feupdateenv(const fenv_t *__envp)
{
	fexcept_t __flags;

	__rflags(__flags);
	__wfcsr(*__envp);
	__setflags(__flags & FE_ALL_EXCEPT);
	return (0);
}

#if __BSD_VISIBLE

// there are no exception traps to enable
static inline int
feenableexcept(int __mask)
{
	return ((__mask & FE_ALL_EXCEPT) ? -1 : 0);
}

static inline int
fedisableexcept(int __mask)
{
	(void)__mask;
	return (0);
}

static inline int
fegetexcept(void)
{
	return (0);
}

#endif /* __BSD_VISIBLE */

__END_DECLS

#endif	/* !_FENV_H_ */
