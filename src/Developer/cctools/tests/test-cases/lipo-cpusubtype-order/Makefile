# PLATFORM: IOS

PLATFORM = IOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	for arch in armv7k arm64e arm64 arm64_32;			      \
	do								      \
	  ofile="foo.$$arch";						      \
	  $(CC) -arch $$arch -o $$ofile -dynamiclib foo.c > /dev/null 2>&1;      \
	  files="$$files $$ofile";					      \
	done;								      \
	${LIPO} -create -output foo.dylib $$files;
	${LIPO} -info foo.dylib | grep -e 'arm64 arm64e\s*$$' 		      \
		| ${FAIL_IF_EMPTY}
	${PASS_IFF_SUCCESS} true

clean:
	rm -rf foo.armv7k foo.arm64 foo.arm64e foo.arm64_32 foo.dylib

