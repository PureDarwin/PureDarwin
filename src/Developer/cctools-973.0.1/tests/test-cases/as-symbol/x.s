        rightp = 8
        leftp = 13
        leftn = -leftp

        .text
        .align 2
        .p2align 3
	.globl junk
junk:
	movq %rax,-5(%rsi)
	movq %rax,-13+8(%rsi)
	movq %rax,-13+rightp(%rsi)
	movq %rax,-leftp+rightp(%rsi)
	movq %rax,leftn+rightp(%rsi)
	movq %rax,rightp+leftn(%rsi)
	ret
