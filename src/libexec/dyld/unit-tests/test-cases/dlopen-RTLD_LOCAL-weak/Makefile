


TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# verifes that RTLD_LOCAL suppresses weak symbol coalescing
#



all-check: all check

check:
	./main 

all: main libfoo.dylib libbar.dylib

main : main.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib

libbar.dylib  : bar.c
	${CC} ${CCFLAGS} -dynamiclib bar.c -o libbar.dylib 

clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib libbar.dylib

