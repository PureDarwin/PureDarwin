

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile


#
# <rdar://problem/6369189> flat_namespace and weak binding conflict
#
# Note that libfoo.dylib is built flat-namespace
#

all-check: all check

check:
	./main

all: main

main: main.c libfoo.dylib 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libfoo.dylib 

libfoo.dylib: foo.c  libbar.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libfoo.dylib foo.c  libbar.dylib -flat_namespace

libbar.dylib: bar.c  libbaz.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbar.dylib bar.c  libbaz.dylib

libbaz.dylib: baz.c 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o libbaz.dylib baz.c 

clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib libbar.dylib libbaz.dylib

