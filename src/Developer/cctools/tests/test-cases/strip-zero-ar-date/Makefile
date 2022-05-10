# PLATFORM: MACOS
#
# test zeroing ar dates when stripping an archive.
#
# <rdar://problem/50683266> With ZERO_AR_DATE, strip will zero the .a time, libtool will not

.PHOHY: all clean

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# build an archive
	${CC} -o foo.o -c ${TESTROOT}/src/foo.c
	${LIBTOOL} -static -o foo.a foo.o

	# verify the archive has two components in it
	${OTOOL} -av foo.a | ${CHECK} -p TOC
# TOC: Archive : foo.a
# TOC: .* __.SYMDEF SORTED
# TOC: .* foo.o

	# verify the archive is newer than the .o file
	ls -1tr foo.o foo.a | ${CHECK} -p LS
# LS: foo.o
# LS: foo.a

	# strip the archive without zeroing dates.
	${STRIP} foo.a 2>&1
	${OTOOL} -av foo.a | ${CHECK} -p NOZERO
# NOZERO: Archive : foo.a
# NOZERO-NOT: .* Wed Dec 31 \d+:00:00 1969 __.SYMDEF SORTED
# NOZERO: .* __.SYMDEF SORTED
# NOZERO-NOT: .* Wed Dec 31 \d+:00:00 1969 foo.o
# NOZERO: .* foo.o

	# verify the archive is still newer than the .o file
	ls -1tr foo.o foo.a | ${CHECK} -p LS

	# strip the archive and zero dates.
	ZERO_AR_DATE=YES ${STRIP} foo.a 2>&1
	${OTOOL} -av foo.a | ${CHECK} -p ZERO
# ZERO: Archive : foo.a
# ZERO: .* Wed Dec 31 \d+:00:00 1969 __.SYMDEF SORTED
# ZERO: .* Wed Dec 31 \d+:00:00 1969 foo.o

	# verify the archive is still newer than the .o file
	ls -1tr foo.o foo.a | ${CHECK} -p LS

	echo PASS

clean:
	rm -rf foo.o foo.a
