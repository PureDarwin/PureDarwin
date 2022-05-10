# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

ARCH := `machine`

all:
	# compile a host executable
	${CC} -arch ${ARCH} -o hello.host ${TESTROOT}/src/hello.c

	# make something that looks like a Rhapsody PPC file. /shrug
	${CC} -arch ${ARCH} -o hello.ppc ${TESTROOT}/src/hello.c	     \
		-Wl,-segalign,0x2000
	${TESTROOT}/bin/rewrite-macho.pl -a ppc -o hello.ppc hello.ppc

	# make a fat binary
	lipo -create -o hello hello.host hello.ppc

	echo PASS
clean:
	rm -f hello hello.host hello.ppc
