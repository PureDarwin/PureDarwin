# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

PASS_IFF_VALID_PYTHON = python -c					     \
	"import sys,json ; json.loads ( sys.stdin.read ( ) ) ; print 'OK'" | \
	grep "OK" | $(PASS_IFF_STDIN)

all:
	# build some libraries to consume
	${CC} -arch $(ARCH) -o foo.o -c ${TESTROOT}/src/foo.c
	${CC} -arch $(ARCH) -o bar.o -c ${TESTROOT}/src/bar.c
	${LIBTOOL} -static -o libfoo.a foo.o;
	${LIBTOOL} -static -o libbar.a bar.o;

	# verify LD_TRACE_DEPENDENTS takes precedence over LD_TRACE_ARCHIVES
	# by turning both on and building a new library.
	LD_TRACE_DEPENDENTS=1 export LD_TRACE_DEPENDENTS;		     \
	LD_TRACE_ARCHIVES=1 export LD_TRACE_ARCHIVES;			     \
	${LIBTOOL} -static -o libbaz.a libbar.a libfoo.a 2>out.txt
	#$(FAIL_IF_ERROR) cmp out.txt expected.txt
	${CHECK} -i out.txt -p EXPECTED
# EXPECTED: {"archives":[".*/libtool-ldtrace-json-stderr/libbar.a",".*/libtool-ldtrace-json-stderr/libfoo.a"]}

	cat out.txt | $(PASS_IFF_VALID_PYTHON)

clean:
	rm -rf foo.o bar.o libfoo.a libbar.a libbaz.a out.txt
