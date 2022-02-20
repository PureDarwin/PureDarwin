# PLATFORM: MACOS IOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

VALID_PYTHON = python -c						     \
	"import sys,json ; json.loads ( sys.stdin.read ( ) ) ; print 'OK'" | \
	grep "OK"

PASS_IFF_VALID_PYTHON		= $(VALID_PYTHON) | $(PASS_IFF_STDIN)
FAIL_UNLESS_VALID_PYTHON	= $(VALID_PYTHON) | $(FAIL_IF_EMPTY)

all:
	# tools are expected to append to LC_TRACE_FILE, so start with a clean
	# out.txt file
	rm -rf out.txt

	# build some libraries to consume
	${CC} -arch $(ARCH) -o foo.o -c ${TESTROOT}/src/foo.c
	${CC} -arch $(ARCH) -o bar.o -c ${TESTROOT}/src/bar.c
	${LIBTOOL} -static -o libfoo.a foo.o;

	# verify LD_TRACE_DEPENDENTS will avoid writing valid empty files or
	# empty JSON arrays
	LD_TRACE_DEPENDENTS=1 export LD_TRACE_DEPENDENTS;		     \
	LD_TRACE_FILE=out.txt export LD_TRACE_FILE;			     \
	${LIBTOOL} -static -o libbar.a bar.o;
	if [ -a out.txt ];						     \
	then								     \
	  $(FAIL_IF_SUCCESS) true;					     \
	fi

	# verify LD_TRACE_DEPENDENTS takes precedence over LD_TRACE_ARCHIVES
	# by turning both on and building a new library.
	LD_TRACE_DEPENDENTS=1 export LD_TRACE_DEPENDENTS;		     \
	LD_TRACE_ARCHIVES=1 export LD_TRACE_ARCHIVES;			     \
	LD_TRACE_FILE=out.txt export LD_TRACE_FILE;			     \
	${LIBTOOL} -static -o libbaz.a libbar.a libfoo.a
	#$(FAIL_IF_ERROR) cmp out.txt expected.txt
	${CHECK} -i out.txt -p EXPECTED1
# EXPECTED1: {"archives":[".*/libtool-ldtrace-json-file/libbar.a",".*/libtool-ldtrace-json-file/libfoo.a"]}

	cat out.txt | $(FAIL_UNLESS_VALID_PYTHON)

	# verify LD_TRACE_DEPENDENTS will append to the target file. This will
	# no longer be valid JSON, as the file will contain multiple root-level
	# objects
	LD_TRACE_DEPENDENTS=1 export LD_TRACE_DEPENDENTS;		     \
	LD_TRACE_FILE=out.txt export LD_TRACE_FILE;			     \
	${LIBTOOL} -static -o libbaz.a libbar.a libfoo.a
	#$(PASS_IFF) cmp out.txt expected2.txt
	$(PASS_IFF) ${CHECK} -i out.txt -p EXPECTED2
# EXPECTED2: {"archives":[".*/test-cases/libtool-ldtrace-json-file/libbar.a",".*/test-cases/libtool-ldtrace-json-file/libfoo.a"]}
# EXPECTED2: {"archives":[".*/test-cases/libtool-ldtrace-json-file/libbar.a",".*/test-cases/libtool-ldtrace-json-file/libfoo.a"]}

clean:
	rm -rf foo.o bar.o libfoo.a libbar.a libbaz.a out.txt
