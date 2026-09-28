#ifndef	_ARCH_RISCV_ASM_HELP_H_
#define	_ARCH_RISCV_ASM_HELP_H_

#ifdef	__ASSEMBLER__

#if !defined(__riscv) || __riscv_xlen != 64
#error Unknown architecture.
#endif

// ';' separates riscv statements, arm64's macros use %% for the same thing
#define ALIGN	.align 2

#define TEXT	.text

#define DATA	.data

#define LEAF(name, localvarsize)	\
	.globl	name			; \
	ALIGN				; \
name:

#define P_LEAF(name, localvarsize)	\
	ALIGN				; \
name:

#define LABEL(name)			\
	.globl	name			; \
name:

#define NESTED(name, localvarsize)	\
	.globl	name			; \
	ALIGN				; \
name:

#define P_NESTED(name, localvarsize)	\
	ALIGN				; \
name:

#define END(name)

#define IMPORT(name)			\
	.reference name

#define EXPORT(name)			\
	.globl	name			; \
name:

// la goes through the got when the code is pic, lla is always pc relative
#if defined(__DYNAMIC__)
#define GET_ADDRESS(reg, var)		\
	la	reg, var
#else
#define GET_ADDRESS(reg, var)		\
	lla	reg, var
#endif

// ld64 routes these through stubs or branch islands as needed
#define BRANCH_EXTERNAL(var)		\
	tail	var

#define CALL_EXTERNAL(var)		\
	call	var

#define ENTRY_POINT(name)		\
	.align 2			; \
	.globl name			; \
	.text				; \
name:

#endif	/* __ASSEMBLER__ */

#endif	/* _ARCH_RISCV_ASM_HELP_H_ */
