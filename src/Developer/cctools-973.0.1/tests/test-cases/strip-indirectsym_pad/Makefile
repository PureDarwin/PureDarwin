# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

all:
	$(STRIP) -x -c -o dylib dylib.sym
	ls -ld dylib | $(CHECK)
# CHECK: .* 4096 .*

	echo PASS

clean:
	rm -f dylib.dylib

LD64_606_3 := /SWE/release/Software/Azul/BuildRecords/ld64/ld64-606.3\#10/install/Root/Applications/Xcode.app/Contents/Developer/Toolchains/iOS15.0.xctoolchain/usr/bin/ld
XCODE_LIB := /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib

dylib.sym: dylib.o
	DYLD_LIBRARY_PATH=$(XCODE_LIB)					     \
	  $(LD64_606_3) -dylib -o $@ $^					     \
	  -framework System -framework CoreFoundation
	rm -f dylib.o
