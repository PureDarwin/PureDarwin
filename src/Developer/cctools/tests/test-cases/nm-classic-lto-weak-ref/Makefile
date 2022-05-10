# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${CC} -c weak-ref.cpp -O2 -o weak-ref.o
	${CC} -c weak-ref.cpp -O2 -flto=full -o weak-ref.lto.o
	${NMC} -m weak-ref.o | ${CHECK} -p MACHO
	${NMC} -m weak-ref.lto.o | ${CHECK} -p BITCODE
# MACHO: \[\[:xdigit:\]\]+ (__TEXT,__text) external __Z3barv
# MACHO: \[\[:xdigit:\]\]+ (__TEXT,__text) weak external automatically hidden __Z3fooILi0EEvv
# MACHO:                  (undefined) external _printf
# BITCODE: ---------------- (LTO,CODE) external __Z3barv
# BITCODE: ---------------- (LTO,CODE) weak external automatically hidden __Z3fooILi0EEvv
# BITCODE:                  (undefined) external _printf

	echo PASS

clean:
	rm -f weak-ref.o weak-ref.lto.o

