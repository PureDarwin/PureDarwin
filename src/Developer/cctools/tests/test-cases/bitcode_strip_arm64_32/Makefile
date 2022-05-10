# PLATFORM: WATCHOS
#
# <rdar://problem/43620195> Unable to load framework in watch app due to dyld mmap error __LINKEDIT in Segment::map()

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	$(CC) -arch arm64_32 -dynamiclib -o empty.dylib -fembed-bitcode empty.c
	$(BITCODE_STRIP) -m -o empty2.dylib empty.dylib
# 	otool -lv empty2.dylib | grep -A4 __LLVM > segment.txt
# 	grep filesize segment.txt | grep 16384 | $(PASS_IFF_STDIN)
	$(VERIFY_ALIGN_16K) empty2.dylib | $(PASS_IFF_EMPTY)

clean:
	rm -rf empty.dylib empty2.dylib
