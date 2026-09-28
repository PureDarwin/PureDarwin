	.text
	.globl _start
	.p2align 2
_start:
	lla sp, stack_top
	call _kmain
1:	j 1b
	.bss
	.p2align 4
stack:
	.space 16384
stack_top:
