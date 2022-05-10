# PLATFORM: WATCHOS
#
# <rdar://problem/43620195> Unable to load framework in watch app due to dyld mmap error __LINKEDIT in Segment::map()

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	$(CC) -arch arm64_32 -o hello ${TESTROOT}/src/hello.c
	$(CS_ALLOC) -i hello -a arm64_32 19136 -o hello.cstemp
	#$(CS_ALLOC) -i hello -A 33554444 1 19136 -o hello.cstemp
	$(VERIFY_ALIGN_16K) hello.cstemp | $(PASS_IFF_EMPTY)

clean:
	rm -rf hello hello.cstemp
