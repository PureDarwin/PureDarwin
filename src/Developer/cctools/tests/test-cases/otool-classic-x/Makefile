# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a test program
	${CC} -arch x86_64 -o hello ${TESTROOT}/src/hello_textexec.c

	# disassemble text
	${OTOOLC} -x hello | ${CHECK} -p DIS
# DIS: hello:
# DIS_NEXT: (__TEXT,__text) section
# DIS_NEXT: 0000000100000f60 55 48 89 e5 48 83 ec 10 c7 45 fc 00 00 00 00 e8
# DIS_NEXT: 0000000100000f70 8c 00 00 00 31 c0 48 83 c4 10 5d c3
# DIS_NEXT: (__TEXT_EXEC,__text) section
# DIS_NEXT: 0000000100001000 55 48 89 e5 48 83 ec 10 48 8d 3d 8f ff ff ff b0
# DIS_NEXT: 0000000100001010 00 e8 66 ff ff ff 89 45 fc 48 83 c4 10 5d c3

	# disassemble text verbosely
	${OTOOLC} -xv hello | ${CHECK} -p DISV
# DISV: hello:
# DISV_NEXT: (__TEXT,__text) section
# DISV_NEXT: _main:
# DISV_NEXT: 0000000100000f60	pushq	%rbp
# DISV_NEXT: 0000000100000f61	movq	%rsp, %rbp
# DISV_NEXT: 0000000100000f64	subq	$0x10, %rsp
# DISV_NEXT: 0000000100000f68	movl	$0x0, -0x4(%rbp)
# DISV_NEXT: 0000000100000f6f	callq	0x100001000
# DISV_NEXT: 0000000100000f74	xorl	%eax, %eax
# DISV_NEXT: 0000000100000f76	addq	$0x10, %rsp
# DISV_NEXT: 0000000100000f7a	popq	%rbp
# DISV_NEXT: 0000000100000f7b	retq
# DISV_NEXT: (__TEXT_EXEC,__text) section
# DISV_NEXT: _hello:
# DISV_NEXT: 0000000100001000	pushq	%rbp
# DISV_NEXT: 0000000100001001	movq	%rsp, %rbp
# DISV_NEXT: 0000000100001004	subq	$0x10, %rsp
# DISV_NEXT: 0000000100001008	leaq	-0x71(%rip), %rdi
# DISV_NEXT: 000000010000100f	movb	$0x0, %al
# DISV_NEXT: 0000000100001011	callq	0x100000f7c
# DISV_NEXT: 0000000100001016	movl	%eax, -0x4(%rbp)
# DISV_NEXT: 0000000100001019	addq	$0x10, %rsp
# DISV_NEXT: 000000010000101d	popq	%rbp
# DISV_NEXT: 000000010000101e	retq

	# disassemble text Verbosely
	${OTOOLC} -xV hello | ${CHECK} -p DISVV
# DISVV: hello:
# DISVV_NEXT: (__TEXT,__text) section
# DISVV_NEXT: _main:
# DISVV_NEXT: 0000000100000f60	pushq	%rbp
# DISVV_NEXT: 0000000100000f61	movq	%rsp, %rbp
# DISVV_NEXT: 0000000100000f64	subq	$0x10, %rsp
# DISVV_NEXT: 0000000100000f68	movl	$0x0, -0x4(%rbp)
# DISVV_NEXT: 0000000100000f6f	callq	_hello
# DISVV_NEXT: 0000000100000f74	xorl	%eax, %eax
# DISVV_NEXT: 0000000100000f76	addq	$0x10, %rsp
# DISVV_NEXT: 0000000100000f7a	popq	%rbp
# DISVV_NEXT: 0000000100000f7b	retq
# DISVV_NEXT: (__TEXT_EXEC,__text) section
# DISVV_NEXT: _hello:
# DISVV_NEXT: 0000000100001000	pushq	%rbp
# DISVV_NEXT: 0000000100001001	movq	%rsp, %rbp
# DISVV_NEXT: 0000000100001004	subq	$0x10, %rsp
# DISVV_NEXT: 0000000100001008	leaq	-0x71(%rip), %rdi       ## literal pool for: "hello, world!\n"
# DISVV_NEXT: 000000010000100f	movb	$0x0, %al
# DISVV_NEXT: 0000000100001011	callq	0x100000f7c             ## symbol stub for: _printf
# DISVV_NEXT: 0000000100001016	movl	%eax, -0x4(%rbp)
# DISVV_NEXT: 0000000100001019	addq	$0x10, %rsp
# DISVV_NEXT: 000000010000101d	popq	%rbp
# DISVV_NEXT: 000000010000101e	retq

	echo PASS

clean:
	rm -f hello
