
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main 

main : main.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c -Wno-deprecated-declarations



clean:
	${RM} ${RMFLAGS} *~ main

