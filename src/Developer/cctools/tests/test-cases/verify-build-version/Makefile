# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# build the vers tool, for now
	$(CC) -o verstool ${TESTROOT}/src/verstool.c
	codesign -s - verstool

	# compile a dylib
	$(CC) -dynamiclib -o foo.dylib foo.c

	# test two macos versions
	./verstool foo.dylib write \
	    version-min macos 10.13 10.13 \
	    version-min macos 10.13 10.13 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version macos 10.13 10.13 \
	    build-version macos 10.13 10.13 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# test two ios versions
	./verstool foo.dylib write \
	    version-min ios 11 11 \
	    version-min ios 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version ios 11 11 \
	    build-version ios 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# test two watchos versions
	./verstool foo.dylib write \
	    version-min watchos 5 5 \
	    version-min watchos 5 5 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version watchos 5 5 \
	    build-version watchos 5 5 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# test two tvos versions
	./verstool foo.dylib write \
	    version-min tvos 11 11 \
	    version-min tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version tvos 11 11 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

 	# vers min + build vers
	./verstool foo.dylib write \
	    version-min ios 11 11 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min watchos 5 5 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min macos 10.13 10.13 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min tvos 10.13 10.13 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

 	# build vers + vers min
	./verstool foo.dylib write \
	    version-min ios 11 11 \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_SUCCESS) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# one version min
	./verstool foo.dylib write \
	    version-min macos 10.13 10.13 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min ios 11 11 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min watchos 5 5 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    version-min tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# one build version
	./verstool foo.dylib write \
	    build-version macos 10.13 10.13 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version ios 11 11 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version watchos 5 5 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	./verstool foo.dylib write \
	    build-version tvos 11 11 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# iosmac
	./verstool foo.dylib write \
	    build-version macos 10.13 10.13 \
	    build-version iosmac 11 11 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	# driverkit
	./verstool foo.dylib write \
	    build-version driverkit 19 19 \
	    foo2.dylib
	$(FAIL_IF_ERROR) $(CS_ALLOC) -i foo2.dylib -a x86_64 4096 \
		-o foo3.dylib 2>&1

	$(PASS_IFF) true

clean:
	rm -rf foo.dylib foo2.dylib foo3.dylib verstool
