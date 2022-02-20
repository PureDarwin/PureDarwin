TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

#
#  <rdar://problem/6050482> If pie, ignore preferred load address
#
# run a PIE four times and verify libfoo.dylib load address was different every time
#

FOO_ADDRESS = 0x10000000

ifeq "x86_64" "$(ARCH)"
	FOO_ADDRESS = 0x300000000
endif


all-check: all check

check:
	./main > main.out
	./main >> main.out
	./main >> main.out
	./main >> main.out
	if [ `sort main.out -u | wc -l` == 4 ]; \
	then \
		echo "PASS pie-dylib"; \
	else \
		echo "FAIL pie-dylib"; \
	fi; \
	 
all: main

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -Wl,-pie libfoo.dylib -o main main.c

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib -seg1addr ${FOO_ADDRESS}

clean:
	${RM} ${RMFLAGS} *~ main main.out libfoo.dylib

