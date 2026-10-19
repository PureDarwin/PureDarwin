/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Berkeley Software Design, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)cdefs.h	8.8 (Berkeley) 1/9/95
 * $FreeBSD$
 */
#ifndef _FBSD_COMPAT_SYS_CDEFS_H_
#define _FBSD_COMPAT_SYS_CDEFS_H_


#include <posix/sys/cdefs.h>


#if 0
#define __FBSDID(str)	static const char __fbsdid[] = str
#else
#define __FBSDID(str)
#endif

/*
 * This code has been put in place to help reduce the addition of
 * compiler specific defines in FreeBSD code.  It helps to aid in
 * having a compiler-agnostic source tree.
 */

#if defined(__GNUC__) || defined(__INTEL_COMPILER)

#if __GNUC__ >= 3 || defined(__INTEL_COMPILER)
#define __GNUCLIKE_ASM 3
#define __GNUCLIKE_MATH_BUILTIN_CONSTANTS
#else
#define __GNUCLIKE_ASM 2
#endif
#define __GNUCLIKE___TYPEOF 1
#define __GNUCLIKE___OFFSETOF 1
#define __GNUCLIKE___SECTION 1

#define __GNUCLIKE_ATTRIBUTE_MODE_DI 1

#ifndef __INTEL_COMPILER
# define __GNUCLIKE_CTOR_SECTION_HANDLING 1
#endif

#define __GNUCLIKE_BUILTIN_CONSTANT_P 1
# if defined(__INTEL_COMPILER) && defined(__cplusplus) \
    && __INTEL_COMPILER < 800
#  undef __GNUCLIKE_BUILTIN_CONSTANT_P
# endif

#if (__GNUC_MINOR__ > 95 || __GNUC__ >= 3) && !defined(__INTEL_COMPILER)
# define __GNUCLIKE_BUILTIN_VARARGS 1
# define __GNUCLIKE_BUILTIN_STDARG 1
# define __GNUCLIKE_BUILTIN_VAALIST 1
#endif

#if defined(__GNUC__)
# define __GNUC_VA_LIST_COMPATIBILITY 1
#endif

/*
 * Compiler memory barriers, specific to gcc and clang.
 */
#if defined(__GNUC__)
#define	__compiler_membar()	__asm __volatile(" " : : : "memory")
#endif

#ifndef __INTEL_COMPILER
# define __GNUCLIKE_BUILTIN_NEXT_ARG 1
# define __GNUCLIKE_MATH_BUILTIN_RELOPS
#endif

#define __GNUCLIKE_BUILTIN_MEMCPY 1

/* XXX: if __GNUC__ >= 2: not tested everywhere originally, where replaced */
#define __CC_SUPPORTS_INLINE 1
#define __CC_SUPPORTS___INLINE 1
#define __CC_SUPPORTS___INLINE__ 1

#define __CC_SUPPORTS___FUNC__ 1
#define __CC_SUPPORTS_WARNING 1

#define __CC_SUPPORTS_VARADIC_XXX 1 /* see varargs.h */

#define __CC_SUPPORTS_DYNAMIC_ARRAY_INIT 1

#endif /* __GNUC__ || __INTEL_COMPILER */


/*
 * Macro to test if we're using a specific version of gcc or later.
 */
#if defined(__GNUC__) && !defined(__INTEL_COMPILER)
#define	__GNUC_PREREQ__(ma, mi)	\
	(__GNUC__ > (ma) || __GNUC__ == (ma) && __GNUC_MINOR__ >= (mi))
#else
#define	__GNUC_PREREQ__(ma, mi)	0
#endif

/*
 * GNU C version 2.96 adds explicit branch prediction so that
 * the CPU back-end can hint the processor and also so that
 * code blocks can be reordered such that the predicted path
 * sees a more linear flow, thus improving cache behavior, etc.
 *
 * The following two macros provide us with a way to utilize this
 * compiler feature.  Use __predict_true() if you expect the expression
 * to evaluate to true, and __predict_false() if you expect the
 * expression to evaluate to false.
 *
 * A few notes about usage:
 *
 *	* Generally, __predict_false() error condition checks (unless
 *	  you have some _strong_ reason to do otherwise, in which case
 *	  document it), and/or __predict_true() `no-error' condition
 *	  checks, assuming you want to optimize for the no-error case.
 *
 *	* Other than that, if you don't know the likelihood of a test
 *	  succeeding from empirical or other `hard' evidence, don't
 *	  make predictions.
 *
 *	* These are meant to be used in places that are run `a lot'.
 *	  It is wasteful to make predictions in code that is run
 *	  seldomly (e.g. at subsystem initialization time) as the
 *	  basic block reordering that this affects can often generate
 *	  larger code.
 */
#if __GNUC_PREREQ__(2, 96)
#define __predict_true(exp)     __builtin_expect((exp), 1)
#define __predict_false(exp)    __builtin_expect((exp), 0)
#else
#define __predict_true(exp)     (exp)
#define __predict_false(exp)    (exp)
#endif

/*
 * We define this here since <stddef.h>, <sys/queue.h>, and <sys/types.h>
 * require it.
 */
#if __GNUC_PREREQ__(4, 1)
#define __offsetof(type, field)	 __builtin_offsetof(type, field)
#else
#ifndef __cplusplus
#define	__offsetof(type, field)	((size_t)(&((type *)0)->field))
#else
#define __offsetof(type, field)					\
  ((reinterpret_cast <size_t>			\
                 (&reinterpret_cast <const volatile char &>	\
                  (static_cast<type *> (0)->field))))
#endif
#endif
#define	__rangeof(type, start, end) \
	(__offsetof(type, end) - __offsetof(type, start))

/*
 * Given the pointer x to the member m of the struct s, return
 * a pointer to the containing structure.  When using GCC, we first
 * assign pointer x to a local variable, to check that its type is
 * compatible with member m.
 */
#if __GNUC_PREREQ__(3, 1)
#define	__containerof(x, s, m) ({					\
	const volatile __typeof(((s *)0)->m) *__x = (x);		\
	__DEQUALIFY(s *, (const volatile char *)__x - __offsetof(s, m));\
})
#else
#define	__containerof(x, s, m)						\
	__DEQUALIFY(s *, (const volatile char *)(x) - __offsetof(s, m))
#endif

/*
 * Compiler-dependent macros to help declare dead (non-returning) and
 * pure (no side effects) functions, and unused variables.  They are
 * null except for versions of gcc that are known to support the features
 * properly (old versions of gcc-2 supported the dead and pure features
 * in a different (wrong) way).  If we do not provide an implementation
 * for a given compiler, let the compile fail if it is told to use
 * a feature that we cannot live without.
 */
/*
 * Compiler-dependent macros to help declare dead (non-returning) and
 * pure (no side effects) functions, and unused variables.  They are
 * null except for versions of gcc that are known to support the features
 * properly (old versions of gcc-2 supported the dead and pure features
 * in a different (wrong) way).  If we do not provide an implementation
 * for a given compiler, let the compile fail if it is told to use
 * a feature that we cannot live without.
 */
#define	__weak_symbol	__attribute__((__weak__))
#if !__GNUC_PREREQ__(2, 5) && !defined(__INTEL_COMPILER)
#define	__dead2
#define	__pure2
#define	__unused
#endif
#if __GNUC__ == 2 && __GNUC_MINOR__ >= 5 && __GNUC_MINOR__ < 7 && !defined(__INTEL_COMPILER)
#define	__dead2		__attribute__((__noreturn__))
#define	__pure2		__attribute__((__const__))
#define	__unused
/* XXX Find out what to do for __packed, __aligned and __section */
#endif
#if __GNUC_PREREQ__(2, 7) || defined(__INTEL_COMPILER)
#undef __dead2
#define	__dead2		__attribute__((__noreturn__))
#define	__pure2		__attribute__((__const__))
#define	__unused	__attribute__((__unused__))
#define	__used		__attribute__((__used__))
#define	__packed	__attribute__((__packed__))
#define	__aligned(x)	__attribute__((__aligned__(x)))
#define	__section(x)	__attribute__((__section__(x)))
#endif
#if __GNUC_PREREQ__(4, 3)
#define	__alloc_size(x)	__attribute__((__alloc_size__(x)))
#define	__alloc_size2(n, x)	__attribute__((__alloc_size__(n, x)))
#else
#define	__alloc_size(x)
#define	__alloc_size2(n, x)
#endif
#if __GNUC_PREREQ__(4, 9)
#define	__alloc_align(x)	__attribute__((__alloc_align__(x)))
#else
#define	__alloc_align(x)
#endif

/*
 * Compiler-dependent macros to declare that functions take printf-like
 * or scanf-like arguments.  They are null except for versions of gcc
 * that are known to support the features properly (old versions of gcc-2
 * didn't permit keeping the keywords out of the application namespace).
 */
#if !__GNUC_PREREQ__(2, 7) && !defined(__INTEL_COMPILER)
#define	__printflike(fmtarg, firstvararg)
#define	__scanflike(fmtarg, firstvararg)
#define	__format_arg(fmtarg)
#else
#define	__printflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#define	__scanflike(fmtarg, firstvararg) \
	    __attribute__((__format__ (__scanf__, fmtarg, firstvararg)))
#define	__format_arg(fmtarg)	__attribute__((__format_arg__ (fmtarg)))
#endif

/*
 * C99 Static array indices in function parameter declarations.  Syntax such as:
 * void bar(int myArray[static 10]);
 * is allowed in C99 but not in C++.  Define __min_size appropriately so
 * headers using it can be compiled in either language.  Use like this:
 * void bar(int myArray[__min_size(10)]);
 */
#if !defined(__cplusplus) && \
    (defined(__clang__) || __GNUC_PREREQ__(4, 6)) && \
    (!defined(__STDC_VERSION__) || (__STDC_VERSION__ >= 199901))
#define __min_size(x)	static (x)
#else
#define __min_size(x)	(x)
#endif

#if __GNUC_PREREQ__(3, 1)
#define	__noinline	__attribute__ ((__noinline__))
#else
#define	__noinline
#endif

#if __GNUC_PREREQ__(3, 3)
#define __nonnull(x)	__attribute__((__nonnull__(x)))
#else
#define __nonnull(x)
#endif

/*
 * The __CONCAT macro is used to concatenate parts of symbol names, e.g.
 * with "#define OLD(foo) __CONCAT(old,foo)", OLD(foo) produces oldfoo.
 * The __CONCAT macro is a bit tricky to use if it must work in non-ANSI
 * mode -- there must be no spaces between its arguments, and for nested
 * __CONCAT's, all the __CONCAT's must be at the left.  __CONCAT can also
 * concatenate double-quoted strings produced by the __STRING macro, but
 * this only works with ANSI C.
 *
 * __XSTRING is like __STRING, but it expands any macros in its argument
 * first.  It is only available with ANSI C.
 */
#if defined(__STDC__) || defined(__cplusplus)
#define	__XSTRING(x)	__STRING(x)	/* expand x, then stringify */

#define	__const		const		/* define reserved names to standard */
#define	__signed	signed
#define	__volatile	volatile
#if defined(__cplusplus)
#define	__inline	inline		/* convert to C++ keyword */
#else
#if !(defined(__CC_SUPPORTS___INLINE))
#define	__inline			/* delete GCC keyword */
#endif /* ! __CC_SUPPORTS___INLINE */
#endif /* !__cplusplus */

#else	/* !(__STDC__ || __cplusplus) */

#if !defined(__CC_SUPPORTS___INLINE)
#define	__const				/* delete pseudo-ANSI C keywords */
#define	__inline
#define	__signed
#define	__volatile
/*
 * In non-ANSI C environments, new programs will want ANSI-only C keywords
 * deleted from the program and old programs will want them left alone.
 * When using a compiler other than gcc, programs using the ANSI C keywords
 * const, inline etc. as normal identifiers should define -DNO_ANSI_KEYWORDS.
 * When using "gcc -traditional", we assume that this is the intent; if
 * __GNUC__ is defined but __STDC__ is not, we leave the new keywords alone.
 */
#ifndef	NO_ANSI_KEYWORDS
#define	const				/* delete ANSI C keywords */
#define	inline
#define	signed
#define	volatile
#endif	/* !NO_ANSI_KEYWORDS */
#endif	/* !__CC_SUPPORTS___INLINE */
#endif	/* !(__STDC__ || __cplusplus) */

#ifndef	__DECONST
#define	__DECONST(type, var)	((type)(uintptr_t)(const void *)(var))
#endif

#ifndef	__UNCONST
#define	__UNCONST(var)	((void*)(uintptr_t)(const void *)(var))
#endif

#ifndef	__DEVOLATILE
#define	__DEVOLATILE(type, var)	((type)(uintptr_t)(volatile void *)(var))
#endif

#ifndef	__DEQUALIFY
#define	__DEQUALIFY(type, var)	((type)(uintptr_t)(const volatile void *)(var))
#endif

#if __GNUC_PREREQ__(4,6) && !defined(__cplusplus)
/* Nothing, gcc 4.6 and higher has _Static_assert built-in */
#elif defined(__COUNTER__)
#define	_Static_assert(x, y)	__Static_assert(x, __COUNTER__)
#define	__Static_assert(x, y)	___Static_assert(x, y)
#define	___Static_assert(x, y)	typedef char __assert_ ## y[(x) ? 1 : -1] \
				__unused
#else
#define	_Static_assert(x, y)
#endif

#endif
