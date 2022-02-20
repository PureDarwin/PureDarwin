# PLATFORM: WATCHOS
#
# Verify bitcode_strip correctly handles scode signatures when writing Mach-Os

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	# compile a bitcode app
	${CC} -arch arm64_32 -fembed-bitcode -o hello ${TESTROOT}/src/hello.c
	codesign -s - hello

	################################################################
	#
	# test various options, and verify the code signature is removed
	#
	# remove the bitcode segment (-r)
	${BITCODE_STRIP} -r -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p NOCS

	# replace the bitcode segment with a marker (-m)
	${BITCODE_STRIP} -m -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p NOCS

	# leave only the bitcode segment (-l)
	${BITCODE_STRIP} -l -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p NOCS

# NOCS-NOT: cmd LC_CODE_SIGNATURE

	################################################################
	#
	# test with -keep_cs and verify the code signature is preserved
	# and the file can be resigned.
	#
	# remove the bitcode segment (-r)
	${BITCODE_STRIP} -r -keep_cs -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS
	${FAIL_IF_ERROR} codesign -s - --preserve-metadata=id,en,re,ru -f \
		hello.st 2>&1
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS

	# replace the bitcode segment with a marker (-m)
	${BITCODE_STRIP} -m -keep_cs -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS
	${FAIL_IF_ERROR} codesign -s - --preserve-metadata=id,en,re,ru -f \
		hello.st 2>&1
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS

	# leave only the bitcode segment (-l)
	${BITCODE_STRIP} -l -keep_cs -o hello.st hello
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS
	${FAIL_IF_ERROR} codesign -s - --preserve-metadata=id,en,re,ru -f \
		hello.st 2>&1
	${OTOOLC} -lv hello.st | ${CHECK} -p HASCS

# HASCS: cmd LC_CODE_SIGNATURE

	################################################################
	#
	# verify alignment, etc. etc. etc.

	${PASS_IFF} ${CHECKSYMS} hello.st

	# done
	#
	################################################################

clean:
	rm -f hello hello.st
