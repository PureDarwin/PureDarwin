# PLATFORM: MACOS
#
# Gather round and listen to my tale of woe:
#
#   <rdar://problem/61021032> libtool / ranlib generates many UBSAN errors due to unaligned ar files
#   <rdar://problem/64999215> SEED: DTK: warning from ranlib when building arm64 libraries
#
# ranlib will print a warning if the contents of an ar file are not aligned
# to architecture pointer width; this warning really only applies to 64-bit
# architectures rounded to 4-byte bundaries.
#
# In the old days, the warning was meant to remind the cctools maintainers to
# ask the gcc backend maintainers if it was safe to dereference unaligned
# pointers on this architecture. And in those old days, it was safe: unaligned
# access might be slower, but it would generate correct results. Known-to-be-
# safe architectures suppressed these checks. When a new 64-bit architecture
# spun up, the warning would re-emerge.
#
# But now we have clang, and clang chooses to exercise its right to make any
# number of correctness assumptions around pointer access. That means suddenly
# 64-bit systems that were known-to-be-safe-but-slow are suddenly not-known-to-
# be-safe. And there's no real value in this check, other than as a reminder
# that there are some old bugs lurking in ranlib.
#
# This test verifies when calling ranlib in a degenerate case, the alignment
# warning does not fire. Currently those warnings are being suppressed. And
# someday the assumptions will be fixed, ranlib will be UBSAN-safe, and this
# test can be removed.
#

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	${CC} -o empty.o -c -x c /dev/null
	${AR} -r empty.a empty.o 2>errs.txt
	cat errs.txt
	${CHECK} -i errs.txt
# CHECK-NOT: .*/ranlib: archive member: empty.a(empty.o) offset in archive not a multiple of 8 (must be since member is an 64-bit object file)

	echo PASS

clean:
	rm -f empty.a empty.o errs.txt

