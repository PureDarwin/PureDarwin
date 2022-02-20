

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main

main: main.c libfoo.dylib  libother.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libother.dylib libfoo.dylib 

libother.dylib: foo.c  
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libother.dylib other.c

libfoo.dylib: foo.c  libbar.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libfoo.dylib foo.c  libbar.dylib

libbar.dylib: bar.c libbar3.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbar.dylib bar.c  libbar3.dylib

libbar3.dylib: bar3.c 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbar3.dylib bar3.c 


clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib libbar.dylib libbar3.dylib libother.dylib

