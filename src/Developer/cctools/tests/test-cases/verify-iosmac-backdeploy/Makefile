# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# build the vers tool, for now
	$(CC) -o verstool ${TESTROOT}/src/verstool.c
	codesign -s - verstool

	# compile a dylib
	$(CC) -dynamiclib -o foo.dylib foo.c

	# check version-min + build-version
	./verstool foo.dylib write \
	    version-min macos 10.13 10.13 \
	    build-version iosmac 12 12 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# check build-version + version-min
	./verstool foo.dylib write \
	    build-version iosmac 12 12 \
	    version-min macos 10.13 10.13 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# verify you can't mix other things.
	./verstool foo.dylib write \
	    build-version iosmac 12 12 \
	    version-min ios 12 12 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min ios 12 12 \
	    build-version iosmac 12 12 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version iosmac 12 12 \
	    build-version ios 12 12 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	$(PASS_IFF) true

clean:
	rm -rf foo.dylib foo2.dylib foo3.dylib verstool
