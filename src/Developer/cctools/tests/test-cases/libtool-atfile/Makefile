# PLATFORM: MACOS
#
# This is a copy of the libtool-deterministic test rewritten to use "@file"
# command-line options

.PHONY: all clean

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a library function
	${CC} -arch $(ARCH) -o libfoo.o  -c libfoo.c

	# build normal libraries, verify they differ (by time)
	#
	# verify libtool replaces @file references with text substitutions.
	#
	# verify libtool can handle more than one @file references on the
	# command line.
	${LIBTOOL} @static.txt -o libfoo1.a @object.txt
	sleep 1
	${LIBTOOL} @static.txt @object.txt -o libfoo2.a
	$(FAIL_IF_SUCCESS) cmp libfoo1.a libfoo2.a

	# build a deterministic library, verify its contents
	#
	# verify libtool can handle nested @file references
	${LIBTOOL} @deterministic.txt -o libfoo1.a
	$(FAIL_IF_SUCCESS) cmp libfoo1.a libfoo2.a
	${OTOOL} -av libfoo1.a | ${CHECK}
#CHECK: Archive : libfoo1.a
#CHECK: -rw-r--r--  0/0      44 Wed Dec 31 \d+:00:00 1969 __.SYMDEF SORTED
#CHECK: -rw-r--r--  0/0     620 Wed Dec 31 \d+:00:00 1969 libfoo.o

	# build a second deterministic library, verify identity
	#
	# verify libtool can handle nested @file references again
	sleep 1
	${LIBTOOL} @deterministic.txt -o libfoo2.a
	$(PASS_IF_SUCCESS) cmp libfoo1.a libfoo2.a

	# verify libtool fails on looping content
	${PASS_IFF_ERROR} ${LIBTOOL} @deterministic.txt -o libfoo1.a	     \
	 	@loop.txt 2>&1

clean:
	rm -rf libfoo.o libfoo1.a libfoo2.a libfoo1.txt
