# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a test program
	${CC} -arch x86_64 -o hello ${TESTROOT}/src/hello.c

	# disassemble text
	${OTOOLC} -t hello | ${CHECK} -p DIS
# DIS: hello:
# DIS_NEXT: (__TEXT,__text) section
# DIS_NEXT:0000000100000f50 55 48 89 e5 48 83 ec 10 48 8d 3d 3b 00 00 00 c7
# DIS_NEXT:0000000100000f60 45 fc 00 00 00 00 b0 00 e8 0d 00 00 00 31 c9 89
# DIS_NEXT:0000000100000f70 45 f8 89 c8 48 83 c4 10 5d c3

	# disassemble text verbosely
	${OTOOLC} -tv hello | ${CHECK} -p DISV
# DISV: hello:
# DISV_NEXT: (__TEXT,__text) section
# DISV_NEXT: _main:
# DISV_NEXT: 0000000100000f50	pushq	%rbp
# DISV_NEXT: 0000000100000f51	movq	%rsp, %rbp
# DISV_NEXT: 0000000100000f54	subq	$0x10, %rsp
# DISV_NEXT: 0000000100000f58	leaq	0x3b(%rip), %rdi
# DISV_NEXT: 0000000100000f5f	movl	$0x0, -0x4(%rbp)
# DISV_NEXT: 0000000100000f66	movb	$0x0, %al
# DISV_NEXT: 0000000100000f68	callq	0x100000f7a
# DISV_NEXT: 0000000100000f6d	xorl	%ecx, %ecx
# DISV_NEXT: 0000000100000f6f	movl	%eax, -0x8(%rbp)
# DISV_NEXT: 0000000100000f72	movl	%ecx, %eax
# DISV_NEXT: 0000000100000f74	addq	$0x10, %rsp
# DISV_NEXT: 0000000100000f78	popq	%rbp
# DISV_NEXT: 0000000100000f79	retq

	# disassemble text Verbosely
	${OTOOLC} -tV hello | ${CHECK} -p DISVV
# DISVV: hello:
# DISVV_NEXT:(__TEXT,__text) section
# DISVV_NEXT:_main:
# DISVV_NEXT:0000000100000f50	pushq	%rbp
# DISVV_NEXT:0000000100000f51	movq	%rsp, %rbp
# DISVV_NEXT:0000000100000f54	subq	$0x10, %rsp
# DISVV_NEXT:0000000100000f58	leaq	0x3b(%rip), %rdi        ## literal pool for: "hello, world!\n"
# DISVV_NEXT:0000000100000f5f	movl	$0x0, -0x4(%rbp)
# DISVV_NEXT:0000000100000f66	movb	$0x0, %al
# DISVV_NEXT:0000000100000f68	callq	0x100000f7a             ## symbol stub for: _printf
# DISVV_NEXT:0000000100000f6d	xorl	%ecx, %ecx
# DISVV_NEXT:0000000100000f6f	movl	%eax, -0x8(%rbp)
# DISVV_NEXT:0000000100000f72	movl	%ecx, %eax
# DISVV_NEXT:0000000100000f74	addq	$0x10, %rsp
# DISVV_NEXT:0000000100000f78	popq	%rbp
# DISVV_NEXT:0000000100000f79	retq

	echo PASS

clean:
	rm -f hello