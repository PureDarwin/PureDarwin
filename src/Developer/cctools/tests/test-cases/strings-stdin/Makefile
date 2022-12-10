# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	echo testing > tmp.txt
	${STRINGS} -a -t d tmp.txt | grep "0 testing" | ${FAIL_IF_EMPTY}
	${STRINGS} -a -t d < tmp.txt | grep "0 testing" | ${FAIL_IF_EMPTY}
	echo testing | ${STRINGS} -a -t d | grep "0 testing" | ${FAIL_IF_EMPTY}
	${PASS_IFF_SUCCESS} true
clean:
	rm -rf tmp.txt
