# PLATFORM: MACOS
#
# Make sure strip correctly strips binaries with chained-binds load commands:
#
#   strip -x to remove local symbols
#   strip -S to remove debugging symbols
#   strip on MH_EXECUTE, MH_DYLIB, and MH_DYLINKER
#
# one of the errors we've seen, nm would display "bad string index"
#
# stub dylibs are not page aligned.

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# MH_EXECUTE: local symbols
	${CC} -Wl,-fixup_chains -g -o symbols symbols.c
	codesign -s - symbols
	${STRIP} -x -o symbols.st symbols 2>&1
	${FAIL_IF_ERROR} ${CHECKSYMSDT} symbols.st
	${NMC} -a symbols.st | ${CHECK} -p EXEC1
# EXEC1: .* T __mh_execute_header
# EXEC1: .* S _global_sym
# EXEC1: .* T _main

	# MH_EXECUTE: debugging symbols
	${STRIP} -S -o symbols.st symbols 2>&1
	${FAIL_IF_ERROR} ${CHECKSYMSDT} symbols.st
	${NMC} symbols.st | ${CHECK} -p EXEC2 -v
# EXEC2: .* T __mh_execute_header
# EXEC2: .* S _global_sym
# EXEC2: .* T _main
# EXEC2: .* t _private_sym

	# MH_DYLIB: local symbols
	${CC} -Wl,-fixup_chains -g -DDYLIB				     \
	     -dynamiclib -compatibility_version 1 -current_version 1	     \
	    -o symbols.dylib symbols.c
	codesign -s - symbols.dylib
	${STRIP} -x -o symbols.dylib.st symbols.dylib 2>&1
	${FAIL_IF_ERROR} ${CHECKSYMSDT} symbols.dylib.st
	${NMC} -a symbols.dylib.st | ${CHECK} -p DYLIB1
# DYLIB1: .* T _entry

	# MH_DYLIB: debugging symbols
	${STRIP} -S -o symbols.dylib.st symbols.dylib 2>&1
	${FAIL_IF_ERROR} ${CHECKSYMSDT} symbols.dylib.st
	${NMC} symbols.dylib.st | ${CHECK} -p DYLIB2
# DYLIB2: .* T _entry
# DYLIB2: .* t _private_sym

	# MH_DYLIB: stub dylib
	${STRIP} -xc -o symbols.dylib.st symbols.dylib 2>&1
	${FAIL_IF_ERROR} ${CHECKSYMSDT} symbols.dylib.st
	${NMC} symbols.dylib.st | ${CHECK} -p STUB
# STUB: .* T _entry

	${CC} -Wl,-fixup_chains -g -DDYLIB_CLIENT			     \
	    -o symbols symbols.c symbols.dylib.st
	${NMC} symbols | ${CHECK} -p STUBCLIENT
# STUBCLIENT: .* T __mh_execute_header
# STUBCLIENT: .* U _entry
# STUBCLIENT: .* S _global_sym
# STUBCLIENT: .* T _main

	echo PASS

clean:
	rm -rf symbols symbols.st symbols.dSYM				     \
	       symbols.dylib symbols.dylib.st symbols.dylib.dSYM
