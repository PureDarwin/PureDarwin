# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${CC} -arch ${ARCH} -Wl,-preload -nodefaultlibs -e _main	     \
	    -o preload ${TESTROOT}/src/preload.c
	${CC} -arch x86_64 -arch x86_64h -Wl,-preload -nodefaultlibs	     \
	    -e _main -o preload.fat ${TESTROOT}/src/preload.c
	${OTOOLC} -hv preload | ${CHECK}
# CHECK:      Mach header
# CHECK-NEXT:       magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
# CHECK-NEXT: MH_MAGIC_64  .*\s+ALL  0x00     PRELOAD     \d+\s+\d+\s+NOUNDEFS

	${MTOR} -n -o preload.bad preload.fat 2>&1 | ${CHECK} -p FAT
# FAT: fatal error: .*mtor: file: preload.fat is a fat file (.*mtor only operates on Mach-O files, use lipo(1) on it to get a Mach-O file)

	${MTOR} -n -o preload.out preload | ${CHECK} -p PRELOAD
	${MTOR} -v -o preload.out preload | ${CHECK} -p PRELOAD
# PRELOAD: writing __TEXT at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+
# PRELOAD: writing __DATA at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+

	${MTOR} -o preload2.out preload
	${FAIL_IF_ERROR} cmp preload.out preload2.out

	${OTOOLC} -hv preload.out | ${CHECK} -p PRELOADOUT
# PRELOADOUT: preload.out: is not an object file

	${MTOR} -v -no_bss -o preload2.out preload | ${CHECK} -p PRELOAD -v
	${FAIL_IF_SUCCESS} cmp preload.out preload2.out 2>&1

	echo PASS

clean:
	rm -f preload preload.out preload2.out preload.fat
