# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a library function
	${CC} -arch $(ARCH) -o libfoo.o  $(LOCAL_CC_FLAGS) -c libfoo.c

	# build normal libraries, verify they differ (by time)
	${LIBTOOL} -static -o libfoo1.a libfoo.o
	sleep 1
	${LIBTOOL} -static -o libfoo2.a libfoo.o
	$(FAIL_IF_SUCCESS) cmp libfoo1.a libfoo2.a

	# build a deterministic library, verify its contents
	${LIBTOOL} -static -D -o libfoo1.a libfoo.o
	$(FAIL_IF_SUCCESS) cmp libfoo1.a libfoo2.a
	${OTOOL} -av libfoo1.a | ${CHECK}
#CHECK: Archive : libfoo1.a
# CHECK-NEXT: -rw-r--r--  0/0 \s+\d+ Wed Dec 31 16:00:00 1969 __.SYMDEF SORTED
# CHECK-NEXT: -rw-r--r--  0/0 \s+\d+ Wed Dec 31 16:00:00 1969 libfoo.o

	# build a second deterministic library, verify identity
	sleep 1
	${LIBTOOL} -static -D -o libfoo2.a libfoo.o
	$(PASS_IFF_SUCCESS) cmp libfoo1.a libfoo2.a

clean:
	rm -rf libfoo.o libfoo1.a libfoo2.a libfoo1.txt
