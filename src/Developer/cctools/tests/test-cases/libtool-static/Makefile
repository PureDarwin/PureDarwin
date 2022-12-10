# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${CC} -arch $(ARCH) -o libfoo.o  $(LOCAL_CC_FLAGS) -c libfoo.c
	${LIBTOOL} -static -o libfoo.a libfoo.o
	${OTOOL} -av libfoo.a | ${CHECK} -p OTOOL
# OTOOL:      Archive : libfoo.a
# OTOOL_NEXT: -rw-r--r--.* __.SYMDEF SORTED
# OTOOL_NEXT: -rw-r--r--.* libfoo.o

	ls -l libfoo.a | ${CHECK} -p LS
# LS: -rw-r--r--  1 .* libfoo.a

	${PASS_IFF_SUCCESS} true

clean:
	rm -rf libfoo.o libfoo.a
