# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${CC} -arch ${ARCH} -Wl,-preload,-segalign,0x1000 -nodefaultlibs     \
	  -e _main -o preload ${TESTROOT}/src/preload_bss.c
	${OTOOLC} -hv preload | ${CHECK}
# CHECK:      Mach header
# CHECK-NEXT:       magic  cputype cpusubtype  caps    filetype ncmds sizeofcmds      flags
# CHECK-NEXT: MH_MAGIC_64  .*\s+ALL  0x00     PRELOAD     \d+\s+\d+\s+NOUNDEFS

	${MTOR} -v -start 0 -o preload.out preload | ${CHECK} -p PRELOAD
# PRELOAD: writing __TEXT at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+
# PRELOAD: writing __DATA at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+

	ls -l preload.out | awk '{print $$5}' | ${CHECK} -p PRELOADSIZE
# PRELOADSIZE: 4128

	${MTOR} -v -no_bss -start 0 -o preload.out preload | ${CHECK} -p NOBSS
# NOBSS: writing __TEXT at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+
# NOBSS: writing __DATA at VMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+ LMA: 0x\[\[:xdigit:\]\]+-0x\[\[:xdigit:\]\]+

	ls -l preload.out | awk '{print $$5}' | ${CHECK} -p NOBSSSIZE
# NOBSSSIZE: 4096

	echo PASS
clean:
	rm -f preload preload.out preload2.out

