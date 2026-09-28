	.text
	.globl __dyld_start
	.p2align 2
__dyld_start:
	call __dyld_start_c
1:	j 1b
