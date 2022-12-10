# PLATFORM: MACOS
#
# test producing an archive file, verifying the times are correct.
#
# Regression tests include:
#
# rdar://49604334 verify libtool doesn't zero out fractional seconds

.PHOHY: all clean

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

DEBUG = echo DEBUG:
# DEBUG = true

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

	# verify the archive has fractional seconds
	#
	# as a race condition, there is a sliiiight chance we'll get unlucky
	# and write a file exactly on a microsecond or nanosecond binary. not
	# really sure how to defend against that.
	stat -f "%Fm" foo.a | ${CHECK} -p STAT
# STAT-NOT: \d+\.000000000
# STAT: \d+\.\d+

	# confirm the archive toc modtime is 5 seconds into the future
	#
	# we can easily get the file mod time in seconds using 'stat -f "%m"'.
	# we can also get the SYMDEF mod time in seconds from 'otool -a'. this
	# saves the trouble of having to parse dates in the Makefile.
	#
	# because ar does not store fractional seconds we can only meaningfully
	# compare whole seconds, and we are vulnerable to a race condition
	# where the comparison isn't exactly 5 seconds.
	otool=${OTOOL};							     \
	filemdt=`stat -f "%m" foo.a`;					     \
	symdeft=`$$otool -a foo.a | head -2 | tail -1 | awk '{print $$4}'`;  \
	delta=`expr $$symdeft - $$filemdt 2>&1`;			     \
	${DEBUG} filemdt=$$filemdt;					     \
	${DEBUG} symdeft=$$symdeft;					     \
	${DEBUG} delta=$$delta;						     \
	if [ "5" != "$$delta" ];					     \
	then								     \
	  echo "error: toc is $$delta seconds in the future (expected 5)";   \
	  echo FAIL;							     \
	  false;							     \
	fi

	# verify ZERO_AR_DATE works.
	#
	# This is very similar to the libtool-deterministic test-case.
	ZERO_AR_DATE=YES ${LIBTOOL} -static -o foo.a foo.o
	${OTOOL} -av foo.a | ${CHECK} -p ZERO
# ZERO: Archive : foo.a
# ZERO: .* Wed Dec 31 \d+:00:00 1969 __.SYMDEF SORTED
# ZERO: .* Wed Dec 31 \d+:00:00 1969 foo.o

	echo PASS

clean:
	rm -rf times foo.o foo.a
