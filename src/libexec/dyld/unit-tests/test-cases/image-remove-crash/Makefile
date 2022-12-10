TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all:
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o foo.bundle foo.c -bundle
	


clean:
	${RM} ${RMFLAGS} *~ main foo.bundle 

