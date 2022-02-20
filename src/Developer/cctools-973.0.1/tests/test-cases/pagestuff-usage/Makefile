# PLATFORM: MACOS
#
# This test exercises pagestuff against a compiled test bianry. Because this
# compiled test binary is "revlocked" to the test script, its source has
# been copied into the test directory. If necessary the test file can be
# regenerated with the following commands:
#
#   make testfile
#   make clean
#
# The test file doesn't need to execute -- it just has to be parsable via
# pagestuff

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean

PAGESTUFF_MACOS := ${PAGESTUFF} -pagesize 0x1000

all:
	# General Usage
	${PAGESTUFF} 2>&1 | ${CHECK} -p USAGE
	${PAGESTUFF} testfile 2>&1 | ${CHECK} -p USAGE
# USAGE: Usage: .*pagestuff mach-o [-arch name] [-p] [-a] pagenumber [pagenumber ...]

	# Missing Arch
	${PAGESTUFF} -arch 2>&1 | ${CHECK} -p NOARCH
	${PAGESTUFF} testfile -arch 2>&1 | ${CHECK} -p NOARCH
# NOARCH: error: .*pagestuff: missing argument to -arch option

	# Unknown arch
	${PAGESTUFF} testfile -arch mdt 2>&1 | ${CHECK} -p BADARCH
# BADARCH: error: .*pagestuff: unknown architecture specification flag: -arch mdt

	# Legal arch, missing page number
	${PAGESTUFF} testfile -arch ppc 2>&1 | ${CHECK} -p USAGE
	${PAGESTUFF} testfile -arch x86_64 2>&1 | ${CHECK} -p USAGE

	# All pages
	${PAGESTUFF_MACOS} testfile -arch x86_64 -a 2>&1 | ${CHECK} -p ALL
	${PAGESTUFF_MACOS} testfile -a 2>&1 | ${CHECK} -p ALL
# ALL: File Page 0 contains Mach-O headers
# ALL-NEXT: File Page 1 contains contents of section (__TEXT,__text)
# ALL-NEXT: Symbols on file page 1 virtual address 0x100001370 to 0x100002000
# ALL-NEXT:   0x0000000100001370 _main
# ALL-NEXT:   0x0000000100001850 _usage
# ALL-NEXT:   0x0000000100001950 _version_type_for_str
# ALL-NEXT:   0x0000000100001a20 _platform_id_for_name
# ALL-NEXT:   0x0000000100001ac0 _parse_version
# ALL-NEXT:   0x0000000100001d10 _read_file
# ALL-NEXT: File Page 2 contains contents of section (__TEXT,__text)
# ALL-NEXT: Symbols on file page 2 virtual address 0x100002000 to 0x100003000
# ALL-NEXT:   0x0000000100002270 _cmd_list
# ALL-NEXT:   0x0000000100002580 _cmd_write
# ALL-NEXT: File Page 3 contains contents of section (__TEXT,__text)
# ALL-NEXT: File Page 3 contains contents of section (__TEXT,__stubs)
# ALL-NEXT: File Page 3 contains contents of section (__TEXT,__stub_helper)
# ALL-NEXT: File Page 3 contains contents of section (__TEXT,__cstring)
# ALL-NEXT: File Page 3 contains contents of section (__TEXT,__unwind_info)
# ALL-NEXT: Symbols on file page 3 virtual address 0x100003000 to 0x100003ff4
# ALL-NEXT:   0x0000000100003480 _NXHostByteOrder
# ALL-NEXT:   0x0000000100003490 _version_name_for_type
# ALL-NEXT:   0x0000000100003510 _platform_name_for_id
# ALL-NEXT:   0x0000000100003590 _platform_id_for_lccmd
# ALL-NEXT:   0x0000000100003610 _print_version
# ALL-NEXT:   0x0000000100003680 _platform_lccmd_for_id
# ALL-NEXT:   0x0000000100003700 _write_handle_error
# ALL-NEXT: File Page 4 contains contents of section (__DATA,__nl_symbol_ptr)
# ALL-NEXT: File Page 4 contains contents of section (__DATA,__got)
# ALL-NEXT: File Page 4 contains contents of section (__DATA,__la_symbol_ptr)
# ALL-NEXT: File Page 4 contains contents of section (__DATA,__const)
# ALL-NEXT: File Page 4 contains contents of section (__DATA,__bss)
# ALL-NEXT: Symbols on file page 4 virtual address 0x100004000 to 0x100004270
# ALL-NEXT:   0x0000000100004140 _kVersionTypes
# ALL-NEXT:   0x0000000100004170 _kPlatforms
# ALL-NEXT:   0x0000000100004248 _gProgramName
# ALL-NEXT:   0x0000000100004250 _gOptions
# ALL-NEXT: File Page 5 contains dyld info for sliding an image
# ALL-NEXT: File Page 5 contains dyld info for binding symbols
# ALL-NEXT: File Page 5 contains dyld info for lazy bound symbols
# ALL-NEXT: File Page 5 contains dyld info for symbols exported by a dylib
# ALL-NEXT: File Page 5 contains data of function starts
# ALL-NEXT: File Page 5 contains symbol table for non-global symbols
# ALL-NEXT: File Page 5 contains symbol table for defined global symbols
# ALL-NEXT: File Page 5 contains symbol table for undefined symbols
# ALL-NEXT: File Page 5 contains indirect symbols table
# ALL-NEXT: File Page 5 contains string table for external symbols
# ALL-NEXT: File Page 5 contains string table for local symbols
# ALL-NEXT: File Page 5 contains data of code signature
# ALL-NEXT: File Page 6 contains data of code signature
# ALL-NEXT: File Page 7 contains data of code signature
# ALL-NEXT: File Page 8 contains data of code signature
# ALL-NEXT: File Page 9 contains data of code signature
# ALL-NEXT: File Page 10 contains data of code signature

	# Bad Page
	${PAGESTUFF_MACOS} testfile -arch x86_64 11 2>&1 | ${CHECK} -p BADPAGE
# BADPAGE: File for architecture x86_64 has no page 11 (has only 11 pages)
	${PAGESTUFF_MACOS} testfile 11 2>&1 | ${CHECK} -p BADPAGE2
# BADPAGE2: File has no page 11 (file has only 11 pages)

	# Page 1
	${PAGESTUFF_MACOS} testfile 1 2>&1 | ${CHECK} -p PAGE1
# PAGE1: File Page 1 contains contents of section (__TEXT,__text)
# PAGE1-NEXT: Symbols on file page 1 virtual address 0x100001370 to 0x100002000
# PAGE1-NEXT:   0x0000000100001370 _main
# PAGE1-NEXT:   0x0000000100001850 _usage
# PAGE1-NEXT:   0x0000000100001950 _version_type_for_str
# PAGE1-NEXT:   0x0000000100001a20 _platform_id_for_name
# PAGE1-NEXT:   0x0000000100001ac0 _parse_version
# PAGE1-NEXT:   0x0000000100001d10 _read_file

	# Page 1 3 5
	${PAGESTUFF_MACOS} testfile 1 3 5 2>&1 | ${CHECK} -p PAGE1_3_5
# PAGE1_3_5: File Page 1 contains contents of section (__TEXT,__text)
# PAGE1_3_5-NEXT: Symbols on file page 1 virtual address 0x100001370 to 0x100002000
# PAGE1_3_5-NEXT:   0x0000000100001370 _main
# PAGE1_3_5-NEXT:   0x0000000100001850 _usage
# PAGE1_3_5-NEXT:   0x0000000100001950 _version_type_for_str
# PAGE1_3_5-NEXT:   0x0000000100001a20 _platform_id_for_name
# PAGE1_3_5-NEXT:   0x0000000100001ac0 _parse_version
# PAGE1_3_5-NEXT:   0x0000000100001d10 _read_file
# PAGE1_3_5-NEXT: File Page 3 contains contents of section (__TEXT,__text)
# PAGE1_3_5-NEXT: File Page 3 contains contents of section (__TEXT,__stubs)
# PAGE1_3_5-NEXT: File Page 3 contains contents of section (__TEXT,__stub_helper)
# PAGE1_3_5-NEXT: File Page 3 contains contents of section (__TEXT,__cstring)
# PAGE1_3_5-NEXT: File Page 3 contains contents of section (__TEXT,__unwind_info)
# PAGE1_3_5-NEXT: Symbols on file page 3 virtual address 0x100003000 to 0x100003ff4
# PAGE1_3_5-NEXT:   0x0000000100003480 _NXHostByteOrder
# PAGE1_3_5-NEXT:   0x0000000100003490 _version_name_for_type
# PAGE1_3_5-NEXT:   0x0000000100003510 _platform_name_for_id
# PAGE1_3_5-NEXT:   0x0000000100003590 _platform_id_for_lccmd
# PAGE1_3_5-NEXT:   0x0000000100003610 _print_version
# PAGE1_3_5-NEXT:   0x0000000100003680 _platform_lccmd_for_id
# PAGE1_3_5-NEXT:   0x0000000100003700 _write_handle_error
# PAGE1_3_5-NEXT: File Page 5 contains dyld info for sliding an image
# PAGE1_3_5-NEXT: File Page 5 contains dyld info for binding symbols
# PAGE1_3_5-NEXT: File Page 5 contains dyld info for lazy bound symbols
# PAGE1_3_5-NEXT: File Page 5 contains dyld info for symbols exported by a dylib
# PAGE1_3_5-NEXT: File Page 5 contains data of function starts
# PAGE1_3_5-NEXT: File Page 5 contains symbol table for non-global symbols
# PAGE1_3_5-NEXT: File Page 5 contains symbol table for defined global symbols
# PAGE1_3_5-NEXT: File Page 5 contains symbol table for undefined symbols
# PAGE1_3_5-NEXT: File Page 5 contains indirect symbols table
# PAGE1_3_5-NEXT: File Page 5 contains string table for external symbols
# PAGE1_3_5-NEXT: File Page 5 contains string table for local symbols
# PAGE1_3_5-NEXT: File Page 5 contains data of code signature

	echo PASS

clean:
	rm -rf verstool.o

testfile: verstool.o
	${CC} -o testfile verstool.o
	codesign -s - testfile


