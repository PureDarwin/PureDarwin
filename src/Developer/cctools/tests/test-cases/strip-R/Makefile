# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

LD_PATH := $(shell xcrun --toolchain ${TOOLCHAIN} -f ld 2>/dev/null)

all:
	# compile a test library
	${CC} -arch ${ARCH} -o foo.o -c ${TESTROOT}/src/foo.c
	${CC} -arch ${ARCH} -o bar.o -c ${TESTROOT}/src/bar.c
	${CC} -r -o lib.o foo.o bar.o 2>&1
	STRIP_LD=${LD_PATH} ${STRIP} -R list.txt -o lib2.o lib.o
	nm -m lib2.o  | ${CHECK}
# CHECK:     .*(__TEXT,__text) private external _foo
# CHECK-NOT: .*(__TEXT,__text) external _foo

	echo PASS
clean:
	rm -f foo.o bar.o lib.o lib2.o
