

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	./main

all: main

main: main.cxx librealmain.dylib 
	${CXX} ${CXXFLAGS} -I${TESTROOT}/include -o main main.cxx librealmain.dylib 

librealmain.dylib: realmain.cxx  libfoo.dylib
	${CXX} ${CXXFLAGS} -I${TESTROOT}/include -dynamiclib -o librealmain.dylib realmain.cxx libfoo.dylib

libfoo.dylib: foo.cxx  
	${CXX} ${CXXFLAGS} -I${TESTROOT}/include -dynamiclib -o libfoo.dylib foo.cxx  

clean:
	${RM} ${RMFLAGS} *~ main  libfoo.dylib  librealmain.dylib

