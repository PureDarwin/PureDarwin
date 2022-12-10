# PLATFORM: MACOS

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

REWRITE = ${TESTROOT}/bin/rewrite-macho.pl

.PHONY: all clean

all:
	# make a series of test binaries.
	${CC} -c -arch x86_64 -o test_x86_64.o $(TESTROOT)/src/foo.c
	${CC} -c -arch arm64  -o test_arm64.o  $(TESTROOT)/src/foo.c
	${CC} -c -arch arm64e -o test_arm64e.o $(TESTROOT)/src/foo.c

	# Force the PAC version to 0.
	#
	# This is for arm64e_2018, used by 1st party userspace and kernel code.
	$(REWRITE) test_arm64e.o -o test_arm64e.o -v -c :$$((0x00000002))
	${LIPO} -create -o test.o test_x86_64.o test_arm64.o test_arm64e.o
	${OTOOLC} -f  test.o | ${CHECK} -p F
	${OTOOLC} -fv test.o | ${CHECK} -p FV

	# Force the PAC version to 15.
	#
	# This is for arm64e_2018 w/ 2nd party capability flags set to disable
	# all pointer authentication keys. This feature was never used, binaries
	# are not known to exist. Tool support was added for this feature
	# "just in case."
	$(REWRITE) test_arm64e.o -o test_arm64e.o -v -c :$$((0x0F000002))
	${LIPO} -create -o test.o test_x86_64.o test_arm64.o test_arm64e.o
	${OTOOLC} -f  test.o | ${CHECK} -p F_PAC
	${OTOOLC} -fv test.o | ${CHECK} -p FV_PAC

	# Set the arm64e versioned ABI flag for userspace.
	#
	# This is for arm64e_2020 and later. The ABI version is not expected
	# to change. But this is another one of those "just in case" things.
	$(REWRITE) test_arm64e.o -o test_arm64e.o -v -c :$$((0xBF000002))
	${LIPO} -create -o test.o test_x86_64.o test_arm64.o test_arm64e.o
	${OTOOLC} -f  test.o | ${CHECK} -p F_USR
	${OTOOLC} -fv test.o | ${CHECK} -p FV_USR

	# Set the arm64e versioned ABI flag for kernelspace.
	#
	# This is for arm64e_2020 and later. The ABI version might change for
	# kexts, assuming the kernel can build a compatibility mechanism they
	# are happy with. Yet another "just in case" file format feature...
	$(REWRITE) test_arm64e.o -o test_arm64e.o -v -c :$$((0xFF000002))
	${LIPO} -create -o test.o test_x86_64.o test_arm64.o test_arm64e.o
	${OTOOLC} -f  test.o | ${CHECK} -p F_KER
	${OTOOLC} -fv test.o | ${CHECK} -p FV_KER

	# Set the arm64e versioned ABI flag to some nonsense junk.
	$(REWRITE) test_arm64e.o -o test_arm64e.o -v -c :$$((0x7F000002))
	${LIPO} -create -o test.o test_x86_64.o test_arm64.o test_arm64e.o
	${OTOOLC} -f  test.o | ${CHECK} -p F_BAD
	${OTOOLC} -fv test.o | ${CHECK} -p FV_BAD

# F: Fat headers
# F: fat_magic 0xcafebabe
# F: nfat_arch 3
# F: architecture 0
# F:     cputype 16777223
# F:     cpusubtype 3
# F:     capabilities 0x0
# F:     offset 4096
# F:     size 608
# F:     align 2^12 (4096)
# F: architecture 1
# F:     cputype 16777228
# F:     cpusubtype 0
# F:     capabilities 0x0
# F:     offset 8192
# F:     size 516
# F:     align 2^12 (4096)
# F: architecture 2
# F:     cputype 16777228
# F:     cpusubtype 2
# F:     capabilities 0x0
# F:     offset 12288
# F:     size 516
# F:     align 2^12 (4096)

# FV: Fat headers
# FV: fat_magic FAT_MAGIC
# FV: nfat_arch 3
# FV: architecture x86_64
# FV:     cputype CPU_TYPE_X86_64
# FV:     cpusubtype CPU_SUBTYPE_X86_64_ALL
# FV:     capabilities 0x0
# FV:     offset 4096
# FV:     size 608
# FV:     align 2^12 (4096)
# FV: architecture arm64
# FV:     cputype CPU_TYPE_ARM64
# FV:     cpusubtype CPU_SUBTYPE_ARM64_ALL
# FV:     capabilities 0x0
# FV:     offset 8192
# FV:     size 516
# FV:     align 2^12 (4096)
# FV: architecture arm64e
# FV:     cputype CPU_TYPE_ARM64
# FV:     cpusubtype CPU_SUBTYPE_ARM64E
# FV:     capabilities 0x0
# FV:     offset 12288
# FV:     size 516
# FV:     align 2^12 (4096)

# F_PAC: Fat headers
# F_PAC: fat_magic 0xcafebabe
# F_PAC: nfat_arch 3
# F_PAC: architecture 0
# F_PAC:     cputype 16777223
# F_PAC:     cpusubtype 3
# F_PAC:     capabilities 0x0
# F_PAC:     offset 4096
# F_PAC:     size 608
# F_PAC:     align 2^12 (4096)
# F_PAC: architecture 1
# F_PAC:     cputype 16777228
# F_PAC:     cpusubtype 0
# F_PAC:     capabilities 0x0
# F_PAC:     offset 8192
# F_PAC:     size 516
# F_PAC:     align 2^12 (4096)
# F_PAC: architecture 2
# F_PAC:     cputype 16777228
# F_PAC:     cpusubtype 2
# F_PAC:     capabilities 0xf
# F_PAC:     offset 12288
# F_PAC:     size 516
# F_PAC:     align 2^12 (4096)

# FV_PAC: Fat headers
# FV_PAC: fat_magic FAT_MAGIC
# FV_PAC: nfat_arch 3
# FV_PAC: architecture x86_64
# FV_PAC:     cputype CPU_TYPE_X86_64
# FV_PAC:     cpusubtype CPU_SUBTYPE_X86_64_ALL
# FV_PAC:     capabilities 0x0
# FV_PAC:     offset 4096
# FV_PAC:     size 608
# FV_PAC:     align 2^12 (4096)
# FV_PAC: architecture arm64
# FV_PAC:     cputype CPU_TYPE_ARM64
# FV_PAC:     cpusubtype CPU_SUBTYPE_ARM64_ALL
# FV_PAC:     capabilities 0x0
# FV_PAC:     offset 8192
# FV_PAC:     size 516
# FV_PAC:     align 2^12 (4096)
# FV_PAC: architecture arm64e
# FV_PAC:     cputype CPU_TYPE_ARM64
# FV_PAC:     cpusubtype CPU_SUBTYPE_ARM64E
# FV_PAC:     capabilities 0xf
# FV_PAC:     offset 12288
# FV_PAC:     size 516
# FV_PAC:     align 2^12 (4096)

# F_USR: Fat headers
# F_USR: fat_magic 0xcafebabe
# F_USR: nfat_arch 3
# F_USR: architecture 0
# F_USR:     cputype 16777223
# F_USR:     cpusubtype 3
# F_USR:     capabilities 0x0
# F_USR:     offset 4096
# F_USR:     size 608
# F_USR:     align 2^12 (4096)
# F_USR: architecture 1
# F_USR:     cputype 16777228
# F_USR:     cpusubtype 0
# F_USR:     capabilities 0x0
# F_USR:     offset 8192
# F_USR:     size 516
# F_USR:     align 2^12 (4096)
# F_USR: architecture 2
# F_USR:     cputype 16777228
# F_USR:     cpusubtype 2
# F_USR:     capabilities 0xbf
# F_USR:     offset 12288
# F_USR:     size 516
# F_USR:     align 2^12 (4096)

# FV_USR: Fat headers
# FV_USR: fat_magic FAT_MAGIC
# FV_USR: nfat_arch 3
# FV_USR: architecture x86_64
# FV_USR:     cputype CPU_TYPE_X86_64
# FV_USR:     cpusubtype CPU_SUBTYPE_X86_64_ALL
# FV_USR:     capabilities 0x0
# FV_USR:     offset 4096
# FV_USR:     size 608
# FV_USR:     align 2^12 (4096)
# FV_USR: architecture arm64
# FV_USR:     cputype CPU_TYPE_ARM64
# FV_USR:     cpusubtype CPU_SUBTYPE_ARM64_ALL
# FV_USR:     capabilities 0x0
# FV_USR:     offset 8192
# FV_USR:     size 516
# FV_USR:     align 2^12 (4096)
# FV_USR: architecture arm64e
# FV_USR:     cputype CPU_TYPE_ARM64
# FV_USR:     cpusubtype CPU_SUBTYPE_ARM64E
# FV_USR:     capabilities PTR_AUTH_VERSION USERSPACE 0
# FV_USR:     offset 12288
# FV_USR:     size 516
# FV_USR:     align 2^12 (4096)

# F_KER: Fat headers
# F_KER: fat_magic 0xcafebabe
# F_KER: nfat_arch 3
# F_KER: architecture 0
# F_KER:     cputype 16777223
# F_KER:     cpusubtype 3
# F_KER:     capabilities 0x0
# F_KER:     offset 4096
# F_KER:     size 608
# F_KER:     align 2^12 (4096)
# F_KER: architecture 1
# F_KER:     cputype 16777228
# F_KER:     cpusubtype 0
# F_KER:     capabilities 0x0
# F_KER:     offset 8192
# F_KER:     size 516
# F_KER:     align 2^12 (4096)
# F_KER: architecture 2
# F_KER:     cputype 16777228
# F_KER:     cpusubtype 2
# F_KER:     capabilities 0xff
# F_KER:     offset 12288
# F_KER:     size 516
# F_KER:     align 2^12 (4096)

# FV_KER: Fat headers
# FV_KER: fat_magic FAT_MAGIC
# FV_KER: nfat_arch 3
# FV_KER: architecture x86_64
# FV_KER:     cputype CPU_TYPE_X86_64
# FV_KER:     cpusubtype CPU_SUBTYPE_X86_64_ALL
# FV_KER:     capabilities 0x0
# FV_KER:     offset 4096
# FV_KER:     size 608
# FV_KER:     align 2^12 (4096)
# FV_KER: architecture arm64
# FV_KER:     cputype CPU_TYPE_ARM64
# FV_KER:     cpusubtype CPU_SUBTYPE_ARM64_ALL
# FV_KER:     capabilities 0x0
# FV_KER:     offset 8192
# FV_KER:     size 516
# FV_KER:     align 2^12 (4096)
# FV_KER: architecture arm64e
# FV_KER:     cputype CPU_TYPE_ARM64
# FV_KER:     cpusubtype CPU_SUBTYPE_ARM64E
# FV_KER:     capabilities PTR_AUTH_VERSION KERNEL 0
# FV_KER:     offset 12288
# FV_KER:     size 516
# FV_KER:     align 2^12 (4096)

# F_BAD: Fat headers
# F_BAD: fat_magic 0xcafebabe
# F_BAD: nfat_arch 3
# F_BAD: architecture 0
# F_BAD:     cputype 16777223
# F_BAD:     cpusubtype 3
# F_BAD:     capabilities 0x0
# F_BAD:     offset 4096
# F_BAD:     size 608
# F_BAD:     align 2^12 (4096)
# F_BAD: architecture 1
# F_BAD:     cputype 16777228
# F_BAD:     cpusubtype 0
# F_BAD:     capabilities 0x0
# F_BAD:     offset 8192
# F_BAD:     size 516
# F_BAD:     align 2^12 (4096)
# F_BAD: architecture 2
# F_BAD:     cputype 16777228
# F_BAD:     cpusubtype 2
# F_BAD:     capabilities 0x7f
# F_BAD:     offset 12288
# F_BAD:     size 516
# F_BAD:     align 2^12 (4096)

# FV_BAD: Fat headers
# FV_BAD: fat_magic FAT_MAGIC
# FV_BAD: nfat_arch 3
# FV_BAD: architecture x86_64
# FV_BAD:     cputype CPU_TYPE_X86_64
# FV_BAD:     cpusubtype CPU_SUBTYPE_X86_64_ALL
# FV_BAD:     capabilities 0x0
# FV_BAD:     offset 4096
# FV_BAD:     size 608
# FV_BAD:     align 2^12 (4096)
# FV_BAD: architecture arm64
# FV_BAD:     cputype CPU_TYPE_ARM64
# FV_BAD:     cpusubtype CPU_SUBTYPE_ARM64_ALL
# FV_BAD:     capabilities 0x0
# FV_BAD:     offset 8192
# FV_BAD:     size 516
# FV_BAD:     align 2^12 (4096)
# FV_BAD: architecture arm64e
# FV_BAD:     cputype CPU_TYPE_ARM64
# FV_BAD:     cpusubtype CPU_SUBTYPE_ARM64E
# FV_BAD:     capabilities 0x7f
# FV_BAD:     offset 12288
# FV_BAD:     size 516
# FV_BAD:     align 2^12 (4096)

	echo PASS

clean:
	rm -f test.o test_x86_64.o test_arm64.o test_arm64e.o
