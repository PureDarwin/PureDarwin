# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a library function
	${CC} -arch $(ARCH) -o libfoo.o  $(LOCAL_CC_FLAGS) -c libfoo.c

	# verify libtool works, for completeness ... 
	${FAIL_IF_ERROR} ${LIBTOOL} -static -o libfoo.a libfoo.o
	${FAIL_IF_ERROR} test -f libfoo.a
	rm -f libfoo.a
	
	# force-fail libtool mid-write
	LIBTOOL_FORCE_FAIL=YES export LIBTOOL_FORCE_FAIL;		     \
	${FAIL_IF_SUCCESS} ${LIBTOOL} -static -o libfoo.a libfoo.o 2>&1
	${PASS_IFF_ERROR} test -f libfoo.a

clean:
	rm -rf libfoo.o libfoo.a libfoo.a.*
