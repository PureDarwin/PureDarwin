// absolute address of an import, same problem
	.text
	.globl _baz
	.p2align 2
_baz:
	lui t1, %hi(_foo_counter)
	sw zero, %lo(_foo_counter)(t1)
	ret
