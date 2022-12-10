TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
# verify addends work
#

all-check: all check

check:
	./main 
	 
all: main

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include libfoo.dylib -o main main.c

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib 

clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib

