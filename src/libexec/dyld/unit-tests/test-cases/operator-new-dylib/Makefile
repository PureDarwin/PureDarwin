



#
# test that if the main executable overrides operator new
# that a dylib using operator new is redirected to it.
#
#

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile


all-check: all check

check:
	./main

all: main


main: main.cxx libfoo.dylib
	${CXX} ${CXXFLAGS} -I${TESTROOT}/include -o main main.cxx libfoo.dylib 

libfoo.dylib: foo.cxx
	${CXX} ${CXXFLAGS} -dynamiclib foo.cxx -o libfoo.dylib



clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib

