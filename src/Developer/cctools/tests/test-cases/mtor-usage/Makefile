# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${MTOR} 2>&1 | ${CHECK} -p USAGE
	${MTOR} -h 2>&1 | ${CHECK} -p USAGE
	${MTOR} -help 2>&1 | ${CHECK} -p USAGE
# USAGE: usage: mtor [-nv] [-no_bss] [-start <start_address>] [-packdata <lma symbol name> <size symbol name>] -output <out_file> <file>
# USAGE-NEXT:        mtor -help [-version]
# USAGE-NEXT:        mtor -version

	${MTOR} -version 2>&1 | ${CHECK} -p VERSION
# VERSION:     cctools.*
# VERSION-NOT: usage.*

	${MTOR} -start 2>&1 | ${CHECK} -p START
# START:      .*mtor: one start address must be specified for -start
# START-NEXT: usage.*

	${MTOR} -start g 2>&1 | ${CHECK} -p START2
# START2:     .*mtor: g not a proper hexadecimal number
# START2-NOT: usage.*

	${MTOR} -o 2>&1 | ${CHECK} -p OUTPUT
	${MTOR} -output 2>&1 | ${CHECK} -p OUTPUT
# OUTPUT:      .*mtor: one output file must be specified
# OUTPUT-NEXT: usage.*

	${MTOR} -o output1 -o output2 2>&1 | ${CHECK} -p OUTPUT1
# OUTPUT1:      .*mtor: only one output file must be specified
# OUTPUT1-NEXT: usage.*


	${MTOR} -o output input1 input2 2>&1 | ${CHECK} -p INPUT
# INPUT:      .*mtor: only one input file must be specified
# INPUT-NEXT: usage.*


	${MTOR} -packdata 2>&1 | ${CHECK} -p PACKDATA1
# PACKDATA1: .*mtor: data lma symbol name must be specified
# PACKDATA1-NEXT: usage.*

	${MTOR} -packdata lma 2>&1 | ${CHECK} -p PACKDATA2
# PACKDATA2: .*mtor: data size symbol name must be specified
# PACKDATA2-NEXT: usage.*

	echo PASS

clean:

