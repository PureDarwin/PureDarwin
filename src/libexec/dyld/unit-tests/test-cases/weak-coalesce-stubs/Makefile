
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile


all-check: all check

check:
	./main

all: 
	$(CC) $(CCFLAGS) -I${TESTROOT}/include main.c -o main
	$(CC) $(CCFLAGS) bar.c -dynamiclib -o libbar.dylib
	$(CC) $(CCFLAGS) foo.c -dynamiclib -o libfoo.dylib
	$(STRIP) -c -x libfoo.dylib -o libstub.dylib	
	
clean:
	${RM} ${RMFLAGS} libbar.dylib libfoo.dylib libstub.dylib main
