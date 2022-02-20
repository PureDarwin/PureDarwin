# PLATFORM: MACOS
#
# pagestuff prints basic statistics about how Mach-O structures are laid
# out on a per-page basis.

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all:
	${CC} -Wl,-fixup_chains -o hello ${TESTROOT}/src/hello.c

	${PAGESTUFF} hello -a | ${CHECK} -p OPTA
# OPTA: File Page 0 contains Mach-O headers
# OPTA: File Page \d+ contains dyld info for chained fixups
# OPTA: File Page \d+ contains dyld info for exported symbols (TRIE)

	${PAGESTUFF} hello -p | ${CHECK} -p OPTP
# OPTP:      FP_MACH_O
# OPTP-NEXT:     offset = 0
# OPTP-NEXT:     size = \d+
# OPTP:          MP_DYLD_CHAINED_FIXUPS
# OPTP-NEXT:         offset = \d+
# OPTP-NEXT:         size = \d+
# OPTP-NEXT:     MP_DYLD_EXPORTS_TRIE
# OPTP-NEXT:         offset = \d+
# OPTP-NEXT:         size = \d+

	echo PASS

clean:
	rm -f hello
