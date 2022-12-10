# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# build the vers tool, for now
	$(CC) -o verstool ${TESTROOT}/src/verstool.c
	codesign -s - verstool

	# compile a dylib
	$(CC) -dynamiclib -o foo.dylib foo.c

	# verify iosmac load commands
	./verstool foo.dylib write \
	    build-version macos 10.14 10.14 \
	    build-version iosmac 12 12 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	$(PASS_IFF) true

clean:
	rm -rf foo.dylib foo2.dylib foo3.dylib verstool
