# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${CC} -arch ${ARCH} -o hello -c ${TESTROOT}/src/hello.c
	${MTOR} -output out hello 2>&1 | ${CHECK} -p NOTPRELOAD
	${CC} -arch ${ARCH} -o hello ${TESTROOT}/src/hello.c
	${MTOR} -output out hello 2>&1 | ${CHECK} -p NOTPRELOAD
	${CC} -arch ${ARCH} -dynamiclib -o dylib ${TESTROOT}/src/foo.c
	${MTOR} -output out hello 2>&1 | ${CHECK} -p NOTPRELOAD
# NOTPRELOAD: fatal error: .*mtor: input file: .* must be an MH_PRELOAD file type

	echo PASS

clean:
	rm -rf hello dylib out
