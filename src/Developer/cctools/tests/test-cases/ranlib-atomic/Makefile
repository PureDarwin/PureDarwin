# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a library function
	${CC} -arch $(ARCH) -o libfoo.o  $(LOCAL_CC_FLAGS) -c libfoo.c

	# verify ranlib works, for completeness ...
	${FAIL_IF_ERROR} ar r libfoo.a libfoo.o 2>&1
	${FAIL_IF_ERROR} ${RANLIB} libfoo.a
	${FAIL_IF_ERROR} test -f libfoo.a
	rm -f libfoo.a
	
	# force-fail ranlib mid-write. In this case, the .a file will still
	# exist, but it should be unmodified.
	${FAIL_IF_ERROR} ar r libfoo.a libfoo.o 2>&1
	cp libfoo.a libfoo.a.orig
	RANLIB_FORCE_FAIL=YES export RANLIB_FORCE_FAIL;		     \
	${FAIL_IF_SUCCESS} ${RANLIB} libfoo.a 2>&1
	${PASS_IFF_SUCCESS} cmp libfoo.a libfoo.a.orig

clean:
	rm -rf libfoo.o libfoo.a libfoo1.a libfoo.a.*
