# PLATFORM: MACOS
#
# verify lipo -archs prints values for fat binaries without extra whitespace

.PHONY: all clean

PLATFORM = MACOS
TESTROOT = ../../
include ${TESTROOT}/include/common.makefile

all:
	#compile a thin and fat file.
	${CC} -o thin -arch x86_64 -c ${TESTROOT}/src/hello.c
	${CC} -o fat -arch x86_64 -arch x86_64h -c ${TESTROOT}/src/hello.c
	
	${LIPO} -archs thin | ${CHECK} -p THIN
	${LIPO} -archs fat  | ${CHECK} -p FAT
# THIN-NOT: x86_64\[ \] 
# THIN: x86_64
# FAT-NOT: x86_64 x86_64h\[ \]
# FAT: x86_64 x86_64h

	echo PASS

clean:
	rm -f thin fat
