# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a test program
	${CC} -arch ${ARCH} -o foo.o -c ${TESTROOT}/src/foo.c
	${LIBTOOL} -static -o foo.a foo.o

	${OTOOLC} -S foo.a | ${CHECK} -v
#CHECK:      Archive : foo.a
#CHECK-NEXT: Table of contents from: foo.a(__.SYMDEF SORTED)
#CHECK-NEXT: size of ranlib structures: 8 (number 1)
#CHECK-NEXT: object offset  string index
#CHECK-NEXT: 112            0

	${PASS_IFF} true

clean:
	rm -f foo.o foo.a

