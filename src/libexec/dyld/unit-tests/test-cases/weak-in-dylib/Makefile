

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main

main: main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libfoo.dylib 

libfoo.dylib: foo.c  
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libfoo.dylib foo.c  

clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib 

