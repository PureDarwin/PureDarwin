# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# We don't yet have compiler support for N_COLD_FUNC so we will
	# use a little utility to set the bit
	${CC} -o symtool symtool.c
	codesign -s - symtool

	# compile a cold symbol
	${CC} -o hello.o -c ${TESTROOT}/src/hello.c
	symtool -o hello.o hello.o

	${NMC} -m hello.o | ${CHECK} -v
# CHECK: 0000000000000000 (__TEXT,__text) external [cold func] _main

	echo PASS

clean:
	rm -f hello.o symtool
