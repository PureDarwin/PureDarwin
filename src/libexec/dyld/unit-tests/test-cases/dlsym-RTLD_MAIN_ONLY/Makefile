

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main 

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include libfoo.dylib -o main main.c

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib


clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib

