# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${NMEDIT} -s ex.exp ex.a -o ex.a.edit
	${NMC} ex.a | ${CHECK} -p BEFORE
	${NMC} ex.a.edit | ${CHECK} -p AFTER
# BEFORE: ex.a(ex.o):
# BEFORE: 0000000000000010 T _main
# BEFORE: 0000000000000000 T _my_extern
# BEFORE: 0000000000000008 T _my_local
#  AFTER: ex.a.edit(ex.o):
#  AFTER: 0000000000000010 t _main
#  AFTER: 0000000000000000 T _my_extern
#  AFTER: 0000000000000008 t _my_local

	echo PASS

clean:
	rm -f ex.o ex.a.edit

ex.a: ex.o
	libtool -static -o $@ $^

ex.o: ex.c
	cc -arch arm64 -o $@ -c $^
