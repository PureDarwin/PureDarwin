# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# tools are expected to append to LC_TRACE_FILE, so start with a clean
	# out.txt file
	rm -rf out.txt

	# build some libraries to consume
	${CC} -arch $(ARCH) -o foo.o -c ${TESTROOT}/src/foo.c
	${CC} -arch $(ARCH) -o bar.o -c ${TESTROOT}/src/bar.c
	${LIBTOOL} -static -o libfoo.a foo.o;
	${LIBTOOL} -static -o libbar.a bar.o;

	# verify LD_TRACE_ARCHIVES writes to out.txt.
	LD_TRACE_ARCHIVES=1 export LD_TRACE_ARCHIVES;			     \
	LD_TRACE_FILE=out.txt export LD_TRACE_FILE;			     \
	${LIBTOOL} -static -o libbaz.a libbar.a libfoo.a
	#$(FAIL_IF_ERROR) cmp out.txt expected.txt
	${CHECK} -i out.txt -p EXPECTED1
# EXPECTED1: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libbar.a
# EXPECTED1: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libfoo.a

	# verify LD_TRACE_ARCHIVES appends to out.txt.
	LD_TRACE_ARCHIVES=1 export LD_TRACE_ARCHIVES;			     \
	LD_TRACE_FILE=out.txt export LD_TRACE_FILE;			     \
	${LIBTOOL} -static -o libbaz.a libbar.a libfoo.a
	#$(PASS_IFF) cmp out.txt expected2.txt
	$(PASS_IFF) ${CHECK} -i out.txt -p EXPECTED2
# EXPECTED2: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libbar.a
# EXPECTED2: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libfoo.a
# EXPECTED2: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libbar.a
# EXPECTED2: [Logging for XBS] Used static archive: .*/libtool-ldtrace-file/libfoo.a

clean:
	rm -rf foo.o bar.o libfoo.a libbar.a libbaz.a out.txt
