# PLATFORM: MACOS

.PHONY: all clean

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all: hello
	# verify the stripped file has NLIST_OUTOFSYNC_WITH_DYLDINFO
	${OTOOLC} -hv hello.sym | ${CHECK} -p NOFLAG
	${OTOOLC} -hv hello | ${CHECK} -p HASFLAG
# NOFLAG-NOT: .*NLIST_OUTOFSYNC_WITH_DYLDINFO.*
# HASFLAG:    .*NLIST_OUTOFSYNC_WITH_DYLDINFO.*

	# verify the unstripped binary has a function named "SayHelloTo"
	# and the stripped binary does not
	${NMC} hello.sym | ${CHECK} -p HASHELLO
	${NMC} hello | ${CHECK} -p NOHELLO
# HASHELLO:    .*SayHelloTo.*
# NOHELLO-NOT: .*SayHelloTo.*

	echo PASS

hello:
	# building hello
	swiftc -o hello.sym hello.swift
	${STRIP} -ST -o hello hello.sym

clean:
	rm -rf hello.sym hello
