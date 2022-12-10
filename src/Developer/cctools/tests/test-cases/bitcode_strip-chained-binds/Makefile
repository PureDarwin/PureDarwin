# PLATFORM: WATCHOS

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a bitcode app with chained binds
	${CC} -arch arm64_32 -fembed-bitcode -Wl,-fixup_chains -o hello	     \
	  ${TESTROOT}/src/hello.c

	# strip the binary
	${BITCODE_STRIP} -r -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK}
	${BITCODE_STRIP} -r -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK}
# CHECK-NOT: .*(past end of file)

	${PASS_IFF} ${CHECKSYMS} hello.st

clean:
	rm -f hello hello.st
