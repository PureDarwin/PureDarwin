# PLATFORM: MACOS 
#
# rdar://43732746

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	CS_ALLOC_DEBUG=1 export CS_ALLOC_DEBUG; \
		${CS_ALLOC} -memento -mori 2>&1 | cat > csa.txt 

	grep -- "-memento -mori" csa.txt | ${PASS_IFF_STDIN}

clean:
	rm -rf csa.txt
