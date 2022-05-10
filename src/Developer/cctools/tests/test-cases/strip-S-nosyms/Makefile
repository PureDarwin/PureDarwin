# PLATFORM: MACOS
#
# <rdar://problem/63125207>
# Verify stripping a stripped file w/ symbolless strings preserves strings.
# Yeah, I know.
#
# The test file was harvested from a swift project by Nick Kledzik. It is
# not clear how to manually create a test file like this.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

LD := $(shell xcrun -f --toolchain $(TOOLCHAIN) ld)

.PHONY: all clean

all:
	STRIP_LD=$(LD) $(STRIP) -S -o stripped.o nosyms.o 2>stderr.out
	cat stderr.out
	$(OTOOLC) -lv stripped.o | $(CHECK)
# CHECK: Load command 1
# CHECK:      cmd LC_SYMTAB
# CHECK:  cmdsize 24
# CHECK:   symoff 0
# CHECK:    nsyms 0
# CHECK:   stroff 192
# CHECK:  strsize 8

	echo PASS

clean:
	rm -f stripped.o stderr.out
