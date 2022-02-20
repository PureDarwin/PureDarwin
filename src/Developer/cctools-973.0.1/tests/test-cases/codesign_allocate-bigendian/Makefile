# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

ARCH := `machine`

all:
	# compile an executable
	${CC} -arch ${ARCH} -o hello ${TESTROOT}/src/hello.c

	# test codesigning
	${CS_ALLOC} -i hello -a ${ARCH} 512 -o hello.signed
	${OTOOLC} -lv hello.signed | ${CHECK}

	# modify our file to look like a big endian arch
	${TESTROOT}/bin/rewrite-macho.pl -a ppc -o hello.swap hello

	# test codesigning again
	${CS_ALLOC} -i hello.swap -a ppc 512 -o hello.signed
	${OTOOLC} -lv hello.signed | ${CHECK}
# CHECK:       cmd LC_CODE_SIGNATURE
# CHECK-NEXT:   cmdsize 16
# CHECK-NEXT:   dataoff \d+
# CHECK-NEXT:  datasize \d+

	echo PASS

clean:
	rm -f hello hello.swap hello.signed
