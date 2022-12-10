
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile


#
# <rdar://problem/5869973> DYLD_ROOT_PATH should apply to LC_RPATH rpaths
#

all-check: all check

check:
	export DYLD_ROOT_PATH=`pwd` && ${PASS_IFF} ./main

all: main

	
hide/libfoo.dylib : foo.c
	mkdir -p hide
	${CC} foo.c -dynamiclib -o hide/libfoo.dylib -install_name @rpath/libfoo.dylib
	

main : main.c hide/libfoo.dylib 
	${CC} -I${TESTROOT}/include main.c -o main hide/libfoo.dylib -Wl,-rpath -Wl,/hide


clean:
	${RM} ${RMFLAGS} *~  main hide
