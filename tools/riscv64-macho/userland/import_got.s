// through the got, the one way an import can be addressed
	.text
	.globl _qux
	.p2align 2
_qux:
	lga t1, _foo_counter
	sw zero, 0(t1)
	ret
