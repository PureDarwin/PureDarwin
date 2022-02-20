

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main

main: main.c librealmain.dylib 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c librealmain.dylib 

librealmain.dylib: realmain.c  libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o librealmain.dylib realmain.c libfoo.dylib

libfoo.dylib: foo.c  libbar.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libfoo.dylib foo.c  libbar.dylib

libbar.dylib: bar.c  libbaz.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbar.dylib bar.c  libbaz.dylib

libbaz.dylib: baz.c 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbaz.dylib baz.c 

clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib libbar.dylib libbaz.dylib librealmain.dylib

