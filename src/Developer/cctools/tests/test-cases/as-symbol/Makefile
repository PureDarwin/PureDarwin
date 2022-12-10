# PLATFORM: MACOS
#
# <rdar://problem/58253172> SEED: Web: nlist entry values are truncated to 32-bits
# Verify negative constants are properly sign-extended in the 64-bit assembler.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	$(AS) -Q -arch x86_64 -o x.o x.s 2>as.stderr
	$(OTOOLC) -tV x.o | $(CHECK)
# CHECK: 0000000000000000	movq	%rax, -0x5(%rsi)
# CHECK: 0000000000000004	movq	%rax, -0x5(%rsi)
# CHECK: 0000000000000008	movq	%rax, -0x5(%rsi)
# CHECK: 000000000000000c	movq	%rax, -0x5(%rsi)
# CHECK: 0000000000000010	movq	%rax, -0x5(%rsi)
# CHECK: 0000000000000014	movq	%rax, -0x5(%rsi)
# CHECK: 0000000000000018	retq

	# filter the gas deprecated warning out of stderr
	cat as.stderr | grep -v deprecated 1>&2 | true

	echo PASS

clean:
	rm -f x.o as.stderr
