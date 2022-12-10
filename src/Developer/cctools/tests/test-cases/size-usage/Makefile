# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile
TESTFILE = /bin/ls

.PHONY: all clean

all:
	##
	## test default usage
	##
	${SIZEC} 2>&1 | ${CHECK} -p USAGE
# USAGE: error: .* can't open file: a.out (No such file or directory)
# USAGE-NEXT: __TEXT	__DATA	__OBJC	others	dec	hex

	##
	## No Arguments and other default output
	##
	${SIZEC} ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
	${SIZEC} - ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
	${SIZEC} -arch x86_64 ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
	${SIZEC} -l ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
	${SIZEC} -x ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
	${SIZEC} -lx ${TESTFILE} 2>&1 | ${CHECK} -p DEFAULT
# DEFAULT: __TEXT	__DATA	__OBJC	others	dec	hex

	##
	## Arch errors
	##
	${SIZEC} -arch 2>&1 | ${CHECK} -p NOARCH
# NOARCH: error: .* missing argument(s) to -arch option
	${SIZEC} -arch ppc ${TESTFILE} 2>&1 | ${CHECK} -p MISSINGARCH
# MISSINGARCH: error: .* file: /bin/ls does not contain architecture: ppc
	${SIZEC} -arch MDT ${TESTFILE} 2>&1 | ${CHECK} -p BADARCH
# BADARCH: error: .* unknown architecture specification flag: -arch MDT
# BADARCH-NEXT: .* known architecture flags are: .*

	##
	## m Flag
	##
	${SIZEC} -m ${TESTFILE} 2>&1 | ${CHECK} -p MFLAG
# MFLAG: Segment __PAGEZERO: \[\[:digit:\]\]+
# MFLAG: Segment __TEXT: \[\[:digit:\]\]+
# MFLAG: 	Section __text: \[\[:digit:\]\]+
# MFLAG: 	total \[\[:digit:\]\]+
# MFLAG: Segment __DATA: \[\[:digit:\]\]+
# MFLAG: 	total \[\[:digit:\]\]+
# MFLAG: Segment __LINKEDIT: \[\[:digit:\]\]+
# MFLAG: total \[\[:digit:\]\]+

	${SIZEC} -mx ${TESTFILE} 2>&1 | ${CHECK} -p MXFLAG
# MXFLAG: Segment __PAGEZERO: 0x\[\[:xdigit:\]\]+
# MXFLAG: Segment __TEXT: 0x\[\[:xdigit:\]\]+
# MXFLAG: 	Section __text: 0x\[\[:xdigit:\]\]+
# MXFLAG: 	total 0x\[\[:xdigit:\]\]+
# MXFLAG: Segment __DATA: 0x\[\[:xdigit:\]\]+
# MXFLAG: 	total 0x\[\[:xdigit:\]\]+
# MXFLAG: Segment __LINKEDIT: 0x\[\[:xdigit:\]\]+
# MXFLAG: total 0x\[\[:xdigit:\]\]+

	##
	## l Flag (with m)
	##
	${SIZEC} -ml ${TESTFILE} 2>&1 | ${CHECK} -p MLFLAG
# MLFLAG: Segment __PAGEZERO: \[\[:digit:\]\]+ (vmaddr 0x0 fileoff 0)
# MLFLAG: Segment __TEXT: \[\[:digit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff 0)
# MLFLAG: 	Section __text: \[\[:digit:\]\]+ (addr 0x\[\[:xdigit:\]\]+ offset \[\[:digit:\]\]+)
# MLFLAG: 	total \[\[:digit:\]\]+
# MLFLAG: Segment __DATA: \[\[:digit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff \[\[:digit:\]\]+)
# MLFLAG: 	total \[\[:digit:\]\]+
# MLFLAG: Segment __LINKEDIT: \[\[:digit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff \[\[:digit:\]\]+)
# MLFLAG: total \[\[:digit:\]\]+

	${SIZEC} -mlx ${TESTFILE} 2>&1 | ${CHECK} -p MLXFLAG
# MLXFLAG: Segment __PAGEZERO: 0x\[\[:xdigit:\]\]+ (vmaddr 0x0 fileoff 0)
# MLXFLAG: Segment __TEXT: 0x\[\[:xdigit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff 0)
# MLXFLAG: 	Section __text: 0x\[\[:xdigit:\]\]+ (addr 0x\[\[:xdigit:\]\]+ offset \[\[:digit:\]\]+)
# MLXFLAG: 	total 0x\[\[:xdigit:\]\]+
# MLXFLAG: Segment __DATA: 0x\[\[:xdigit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff \[\[:digit:\]\]+)
# MLXFLAG: 	total 0x\[\[:xdigit:\]\]+
# MLXFLAG: Segment __LINKEDIT: 0x\[\[:xdigit:\]\]+ (vmaddr 0x\[\[:xdigit:\]\]+ fileoff \[\[:digit:\]\]+)
# MLXFLAG: total 0x\[\[:xdigit:\]\]+

	echo PASS

clean:
