# PLATFORM: MACOS
#
# build a preload file with __TEXT at VMA 0x0 and __DATA at VMA 0x8000.
# write the file such that __TEXT LMA is 0x0 and __DATA LMA is 0x1000, and
# the __DATA LMA and size is poked into __TEXT via const variables.

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a file and verify ld produced a Mach-O PRELOAD
	${CC} -arch ${ARCH} -nodefaultlibs -e _main			     \
	    -Wl,-preload,-segaddr,__DATA,0x8000				     \
	    -o preload ${TESTROOT}/src/preload.c
	${OTOOLC} -hv preload | ${CHECK}
# CHECK:      Mach header
# CHECK-NEXT:       magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
# CHECK-NEXT: MH_MAGIC_64  .*\s+ALL  0x00     PRELOAD     \d+\s+\d+\s+NOUNDEFS

	# produce the ordinary raw file
	${MTOR} -n -o preload.out preload | ${CHECK} -p PRELOAD
	${MTOR} -v -o preload.out preload | ${CHECK} -p PRELOAD
# PRELOAD: writing __TEXT at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+
# PRELOAD: writing __DATA at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+

	# verify mtor is deterministic
	${MTOR} -o preload2.out preload
	${FAIL_IF_ERROR} cmp preload.out preload2.out

	# verify mtor output is not a Mach-O
	${OTOOLC} -hv preload.out | ${CHECK} -p PRELOADOUT
# PRELOADOUT: preload.out: is not an object file

	# produce a new, packed raw file
	${FAIL_IF_ERROR} ${MTOR} -v -packdata _data_addr _data_size -o preload2.out preload | \
	    ${CHECK} -p PRELOAD
	${FAIL_IF_SUCCESS} cmp preload.out preload2.out 2>&1

	# produce a new, packed raw file without bss.
	${FAIL_IF_ERROR} ${MTOR} -v -no_bss -packdata _data_addr _data_size		     \
	    -o preload3.out preload |					     \
	    ${CHECK} -p PRELOAD
	${FAIL_IF_SUCCESS} cmp preload.out preload3.out 2>&1
	${FAIL_IF_SUCCESS} cmp preload2.out preload3.out 2>&1

	echo PASS

clean:
	rm -f preload preload.out preload2.out preload3.out
