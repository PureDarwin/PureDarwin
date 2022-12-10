# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a library function
	${CC} -o libfoo.o -c ${TESTROOT}/src/foo.c

	# build a deterministic library
	${LIBTOOL} -static -D -o libfoo.a libfoo.o

	# strip the library
	${STRIP} -D libfoo.a

	# verify
	${OTOOL} -av libfoo.a | ${CHECK}
# CHECK: Archive : libfoo.a
# CHECK-NEXT: -rw-r--r--  0/0 \s+\d+ Wed Dec 31 16:00:00 1969 __.SYMDEF SORTED
# CHECK-NEXT: -rw-r--r--  0/0 \s+\d+ Wed Dec 31 16:00:00 1969 libfoo.o

	echo PASS

clean:
	rm -f libfoo.o libfoo.a
