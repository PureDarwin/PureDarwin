/*
 * Copyright 2004-2018 Haiku, Inc.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SETJMP_H_
#define _SETJMP_H_


#include <config/HaikuConfig.h>

#include <signal.h>

/* include architecture specific definitions */
#include __HAIKU_ARCH_HEADER(arch_setjmp.h)

typedef struct __jmp_buf_tag {
	__jmp_buf	regs;		/* saved registers, stack & program pointer */
	sigset_t	inverted_signal_mask;
} jmp_buf[1];

typedef jmp_buf sigjmp_buf;


#ifdef __cplusplus
extern "C" {
#endif

extern int	_setjmp(jmp_buf jumpBuffer);
extern int	setjmp(jmp_buf jumpBuffer);
extern int	sigsetjmp(jmp_buf jumpBuffer, int saveMask);

extern void	_longjmp(jmp_buf jumpBuffer, int value)
	__attribute__ ((noreturn));
extern void	longjmp(jmp_buf jumpBuffer, int value)
	__attribute__ ((noreturn));
extern void	siglongjmp(sigjmp_buf jumpBuffer, int value)
	__attribute__ ((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* _SETJMP_H_ */
