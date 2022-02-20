TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

PWD = $(shell pwd)

#
# Test that DYLD_FALLBACK_LIBRARY_PATH does not apply to dlopen() of a full path
# <rdar://problem/5951327> DYLD_FALLBACK_LIBRARY_PATH man page misleading
#


all-check: all check

check:
	export DYLD_FALLBACK_LIBRARY_PATH="${PWD}/hide" && ./main 

all: main hide/libfoo.dylib

main : main.c hide/libfoo.dylib 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c  


hide/libfoo.dylib : foo.c
	mkdir -p hide
	${CC} ${CCFLAGS} -dynamiclib foo.c -o "${PWD}/hide/libfoo.dylib" 



clean:
	${RM} -rf  *~  main  hide 

