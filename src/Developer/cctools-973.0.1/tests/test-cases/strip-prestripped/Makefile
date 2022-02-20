# PLATFORM: IOS
#
# <rdar://problem/57307056> ER: Allow strip to process dylibs without local symbols.
#
# When strip runs on a Mach-O without symbols, it emits a (vague) warning
# ("input object file stripped") and then skips the Mach-O. The idea is
# because the binary is already stripped, there's no work to do; skipping the
# file is safer. This means, once a binary is stripped you cannot use strip
# to do some additional processing on that binary, such as create a stub
# dylib, strip away unwanted load commands, etc.
#
# strip has been changed such that it prints a (better) warning when run
# against a stripped binary to only modify the symbol table. When strip is
# asked (in addition, or instead) to modify the binary in other ways, the
# warning will not appear. In either case, strip will proceed with processing
# the file even if it doesn't think anything interesting is going to happen.
#
# This test verifies this new behavior.

PLATFORM = IOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	# construct an empty arm64e dylib
	$(LD) -arch arm64e -dylib -lSystem				     \
	-platform_version iOS ${SDKVERS} ${SDKVERS}			     \
	-o liba.dylib
	${NMC} liba.dylib 2>&1 | ${CHECK} -p NOSYMBOLS
# NOSYMBOLS-SKIP: no symbols
# NOSYMBOLS: .* no name list

	#
	# attempt to strip symbols from the dylib. separate check calls onto
	# their own line so sh catches seg faults.
	${STRIP} liba.dylib -o liba.dylib.stripped 2>out 1>out
	${CHECK} -p WARN -i out
	${NMC} liba.dylib.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out
	${OTOOLC} -hv liba.dylib.stripped | ${CHECK} -p DYLIB
# WARN: .* warning: input object file already stripped: .*
# DYLIB: .* DYLIB .*

	#
	# attempt to create a stub dylib from the dylib
	${STRIP} liba.dylib -o liba.dylib_stub -c -x 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} liba.dylib_stub 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out
	${OTOOLC} -hv liba.dylib_stub | ${CHECK} -p DYLIB_STUB
# NOWARN-NOT: .* warning: input object file already stripped: .*
# DYLIB_STUB: .* DYLIB_STUB .*

	#
	# verify other strip options don't raise a warning.
	${STRIP} liba.dylib -o liba.dylib.stripped -no_uuid 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} liba.dylib.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	${STRIP} liba.dylib -o liba.dylib.stripped -no_split_info 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} liba.dylib.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	#
	# construct an empty .o file, and verify strip runs.
	touch empty.c
	${CC} empty.c -o empty.o -c

	${STRIP} empty.o -o empty.o.stripped 2>out 1>out
	${CHECK} -p WARN -i out
	${NMC} empty.o.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	${STRIP} empty.o -o empty.o.stripped -no_uuid 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} empty.o.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	${STRIP} empty.o -o empty.o.stripped -no_split_info 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} empty.o.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	${STRIP} empty.o -o empty.o.stripped -x 2>out 1>out
	${CHECK} -p WARN -i out
	${NMC} empty.o.stripped 2>out 1>out
	${CHECK} -p NOSYMBOLS -i out

	#
	# construct an empty .a file, and verify strip runs.
	${LIBTOOL} -static -o empty.a empty.o 2>&1

	${STRIP} empty.a -o empty.a.stripped 2>out 1>out
	${CHECK} -p WARN -i out
	${NMC} empty.a.stripped 2>out 1>out
# 	${CHECK} -p EMPTYLIB -i out
# EMPTYLIB: empty.a.stripped(empty.o):
	${CHECK} -p NOSYMBOLS -i out

	${STRIP} empty.a -o empty.a.stripped -no_uuid 2>out 1>out
	${CHECK} -p NOWARN -i out
	${NMC} empty.a.stripped 2>out 1>out
# 	${CHECK} -p EMPTYLIB -i out
	${CHECK} -p NOSYMBOLS -i out

	echo PASS

clean:
	rm -f liba.dylib liba.dylib.stripped liba.dylib_stub		     \
	      empty.c empty.o empty.o.stripped empty.a empty.a.stripped	     \
	      out
