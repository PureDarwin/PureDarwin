# PLATFORM: MACOS
#
# <rdar://problem/20910908> ER: Add link type (weak, upward, normal) to otool -L

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${CC} -dynamiclib -DFOO=weak -o weak.dylib ${TESTROOT}/src/foo.c
	${CC} -dynamiclib -DFOO=reexport -o reexport.dylib ${TESTROOT}/src/foo.c
	${CC} -dynamiclib -DFOO=upward -o upward.dylib ${TESTROOT}/src/foo.c
	${CC} -dynamiclib -DFOO=lazy -o lazy.dylib ${TESTROOT}/src/foo.c
	${CC} -dynamiclib -DFOO=foo -o foo.dylib ${TESTROOT}/src/foo.c	     \
		-Wl,-weak_library,weak.dylib				     \
		-Wl,-reexport_library,reexport.dylib			     \
		-Wl,-upward_library,upward.dylib			     \
		-Wl,lazy.dylib 2>&1
	mangle.pl foo.dylib foo.dylib
	${OTOOLC} -L foo.dylib | ${CHECK}
# CHECK: foo.dylib:
# CHECK: 	weak.dylib (compatibility version 0.0.0, current version 0.0.0, weak)
# CHECK: 	reexport.dylib (compatibility version 0.0.0, current version 0.0.0, reexport)
# CHECK: 	upward.dylib (compatibility version 0.0.0, current version 0.0.0, upward)
# CHECK: 	lazy.dylib (compatibility version 0.0.0, current version 0.0.0, lazy)

	echo PASS

clean:
	rm -rf weak.dylib reexport.dylib upward.dylib lazy.dylib foo.dylib
