# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${CC} -dynamiclib -o thin.dylib ${TESTROOT}/src/foo.c
	${FAIL_IFF} ${LIPO} -output fat.dylib				     \
	    -create -arch_blank arm64 thin.dylib 2>&1

clean:
	rm -f fat.dylib thin.dylib
