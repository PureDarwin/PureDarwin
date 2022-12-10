# PLATFORM: MACOS
#

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# when replacing an existing code signature with a page-aligned code
	# signature codesign_allocate will rewrite linkedit in order to
	# elminate the padding between the string table and the code signature.
	# If the calculation is wrong, the Mach-O will be corrupt:
	#
	# checksyms: object: hello2 malformed object (code signature data at
	# offset 12288 with a size of 32, overlaps string table at offset 8456
	# with a size of 3840)
	#
	${CC} -Wl,-fixup_chains -o hello ${TESTROOT}/src/hello.c
	codesign -s - hello
	${FAIL_IF_ERROR} ${CHECKSYMS} hello
	${CS_ALLOC} -p -i hello -o hello2 -a x86_64 32
	${FAIL_IF_ERROR} ${CHECKSYMS} hello2

	# <rdar://problem/60559015> codesign_allocate will error "chained
	# fixups out of place" after stripping.
	#
	# Verify we can codesign a binary even if the chained fixups data
	# has been logically stripped away.
	${CC} -o hello_bc -fembed-bitcode -Wl,-fixup_chains		     \
	  ${TESTROOT}/src/hello.c
	${BITCODE_STRIP} -l -o hello_bc hello_bc
	${FAIL_IF_ERROR} ${CS_ALLOC} -i hello_bc -o hello_bc -a ${ARCH} 16384

	echo PASS

clean:
	rm -f hello hello2 hello_bc
