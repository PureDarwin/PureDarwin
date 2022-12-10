# PLATFORM: MACOS
#
# smoke test for as, just to exercise some code.
#
# This test uses the llvm assemblers for code generation.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	# make sure as can find clang
	${AS} /dev/null 2>&1 > as.txt | echo
	if grep -q "not installed" as.txt;\
	then\
	  echo XFAIL;\
	  false;\
	fi

	# convert a non-trivial test binary into assembly
	${CC} -arch ${ARCH} ${TESTROOT}/src/verstool.c -S -o verstool.s -O2

	# build the binary
	${FAIL_IF_ERROR} ${AS} -arch ${ARCH} -q -o verstool.o verstool.s 2>&1
	${FAIL_IF_ERROR} ${CC} -o verstool verstool.o -lsystem 2>&1

	# run the binary, if applicable
	if [ "$(ARCH)" == "x64_64" ];					     \
	then								     \
	    ./verstool 2>&1 | ${CHECK};					     \
	fi
# CHECK: usage: verstool <input> list
# CHECK:                 <input> write [(vm|bv) <platform> <minos> <sdk>] ... <output>

	echo PASS
clean:
	rm -rf a.out as.txt verstool.s verstool.o verstool
