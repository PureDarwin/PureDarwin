
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile



all-check: all check

check:
	./main

all:
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c -I../../../include

clean:
	${RM} ${RMFLAGS} main libfoo.dylib
