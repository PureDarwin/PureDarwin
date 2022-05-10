# PLATFORM: MACOS
#
# This test confirms otool-classic prints unknown cpu subtype flags.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

REWRITE = ${TESTROOT}/bin/rewrite-macho.pl

.PHONY: all clean

all:
	${CC} -o foo.o -c ${TESTROOT}/src/foo.c
	subtype=`otool -h foo.o | tail -1 | awk '{print $$3}'`;		     \
	subtype=$$(($$subtype | 0xFF000000));				     \
	${REWRITE} foo.o -v -c :$$subtype -o foo2.o
	${OTOOLC} -h foo2.o | ${CHECK} -p TERSE
# TERSE:  0xfeedfac.\s+\d+\s+\d+\s+0xff.*
	${OTOOLC} -hv foo2.o | ${CHECK} -p VERBOSE
# VERBOSE:  MH_MAGIC\S*\s+\S+\s+\S+\s+0xff.*

	echo PASS

clean:
	rm -f foo.o foo2.o

