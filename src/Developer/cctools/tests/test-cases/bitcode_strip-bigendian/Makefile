# PLATFORM: WATCHOS

.PHONY: all clean

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a bitcode app
	${CC} -arch arm64_32 -fembed-bitcode -o hello ${TESTROOT}/src/hello.c

	# verify bitcode strip works
	${BITCODE_STRIP} -r -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK}

	# modify our file to look like a big endian arch
	${TESTROOT}/bin/rewrite-macho.pl -a ppc -o hello.swap hello

	# verify bitcode strip still works
	${BITCODE_STRIP} -r -o hello.st hello.swap
	${OTOOLC} -lv hello.st | ${CHECK}
# CHECK-NOT: .*(past end of file)

	echo PASS

clean:
	rm -f hello hello.swap hello.st
