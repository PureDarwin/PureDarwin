// direct pc-relative address of an import, which no dylib can bind
	.text
	.globl _bar
	.p2align 2
_bar:
	lla t1, _foo_counter
	sw zero, 0(t1)
	ret
