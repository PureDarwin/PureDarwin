# PLATFORM: MACOS
#
# <rdar://problem/63125207>
# Verify stripping an object-file w/ a non-zero data-in-code payload works.
#
# The test file comes from cctools libstuff's own get_arch_from_host.o.

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

LD := $(shell xcrun -f --toolchain $(TOOLCHAIN) ld)

.PHONY: all clean

all:
	STRIP_LD=$(LD) $(STRIP) -S -o stripped.o dice.o 2>stderr.out
	cat stderr.out
	$(OTOOLC) -lv stripped.o | $(CHECK)
# CHECK: Load command 1
# CHECK:      cmd LC_SYMTAB
# CHECK:  cmdsize 24
# CHECK:   symoff 5432
# CHECK:    nsyms 63
# CHECK:   stroff 6440
# CHECK:  strsize 392

	echo PASS

clean:
	rm -f stripped.o stderr.out
