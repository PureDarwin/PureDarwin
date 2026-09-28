// two functions with a gap the linker pads, which must be nops for riscv
	.text
	.globl _pad_a
	.p2align 1
_pad_a:
	c.nop
	ret
	.globl _pad_b
	.p2align 5
_pad_b:
	ret
