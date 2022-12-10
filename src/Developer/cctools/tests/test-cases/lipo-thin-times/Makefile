# PLATFORM: MACOS
#
# test thining a fat file
#
# Regression tests include:
#
# rdar://49604334 verify lipo -thin doesn't zero out fractional seconds

.PHONY: all clean

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

ARCHS_FAT := -arch x86_64 -arch arm64

all:
	# build a tool for reporting fractional file times
	${CC} -o times ${TESTROOT}/src/times.c
	codesign -s - times

	# compile a fat file, any fat file, yours
	#${CC} $(ARCHS_FAT) -o hello ${TESTROOT}/src/hello.c
	$(CC) $(ARCHS_FAT) -x c -c -o hello /dev/null

	# thin the file
	${LIPO} -thin $(ARCH) -output hello.$(ARCH) hello

	# verify the file fractional seconds
	#
	# as a race condition, there is a sliiiight chance we'll get unlucky
	# and write a file exactly on a microsecond or nanosecond binary. not
	# really sure how to defend against that.
	./times hello.$(ARCH) | ${CHECK} -p TIMES
# TIMES: hello\..*
# TIMES-NOT:    atime: \d+	0
# TIMES-NEXT:   atime: \d+	\d+
# TIMES-NOT:    mtime: \d+	0
# TIMES-NEXT:   mtime: \d+	\d+

	echo PASS

clean:
	rm -rf times hello hello.$(ARCH)
