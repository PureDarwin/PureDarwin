# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${CC} -o file.o -c ${TESTROOT}/src/foo.c
	${AR} -rc file.a file.o
	${RANLIB} -t file.a 2>errs.txt
	cat errs.txt
	${CHECK} -i errs.txt
# CHECK: warning: .*ranlib: touch option (-t) ignored (table of contents rebuilt anyway)
	echo PASS

clean:
	rm -f file.o file.a errs.txt

