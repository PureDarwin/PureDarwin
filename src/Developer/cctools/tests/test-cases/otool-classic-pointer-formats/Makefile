# PLATFORM: MACOS IOS WATCHOS TVOS

.PHONY: all clean tool_all tool_clean

PLATFORM = MACOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

# Temporary
OTOOLC = /tmp/cctools.root/usr/bin/otool-classic
# VALID_ARCHS := $(filter-out i386,$(VALID_ARCHS))

# test_preload compiles and runs the test
#   $1 is the arch
#   $2 is the src file
#   $3 is the check prefix
test_preload =								     \
$(CC) -arch $(1) -static -nostdlib -Wl,-e,_start,-fixup_chains_section $(2); \
$(OTOOLC) -dyld_info a.out

SRC	= ../../src/

all:
	#
	# NONE (dyld info)
	#
	${OTOOLC} -dyld_info tool_NONE | $(CHECK) -p NONE
# NONE: tool_NONE:
# NONE: bind information:
# NONE: segment      section address     value              type        addend   dylib     symbol
# NONE: __DATA       __data  0x10000C050                    pointer     0x0      libSystem ___stderrp
# NONE: __DATA_CONST __got   0x100008000                    pointer     0x0      libSystem dyld_stub_binder

	#
	# THREADED (Threaded rebase, binds only)
	#
	${OTOOLC} -dyld_info tool_THREADED | $(CHECK) -p THREADED
# THREADED: tool_THREADED:
# THREADED: dyld information:
# THREADED: segment      section    address     pointer            type   div    addr  key addend   dylib     symbol/vm address
# THREADED: __DATA_CONST __auth_got 0x100008000 0xC009000000000000 pointer 0x0000 true  IA  0x0      libSystem ___assert_rtn
# THREADED: __DATA_CONST __auth_got 0x100008008 0xC009000000000001 pointer 0x0000 true  IA  0x0      libSystem ___snprintf_chk
# THREADED: __DATA_CONST __auth_got 0x100008010 0xC009000000000003 pointer 0x0000 true  IA  0x0      libSystem _free
# THREADED: __DATA_CONST __auth_got 0x100008018 0xC009000000000004 pointer 0x0000 true  IA  0x0      libSystem _malloc
# THREADED: __DATA_CONST __auth_got 0x100008020 0xC009000000000005 pointer 0x0000 true  IA  0x0      libSystem _printf
# THREADED: __DATA_CONST __auth_got 0x100008028 0xC001000000000006 pointer 0x0000 true  IA  0x0      libSystem _strlen
# THREADED: __DATA       __data     0x10000C018 0x4008000000000002 pointer                  0x0      libSystem ___stderrp

	#
	# ARM64E (auth binds, auth rebases, binds, rebases)
	#
	${OTOOLC} -dyld_info tool_ARM64E | $(CHECK) -p ARM64E
# ARM64E: tool_ARM64E:
# ARM64E: dyld information:
# ARM64E: segment      section    address     pointer            type   div    addr  key addend   dylib     symbol/vm address
# ARM64E: __DATA_CONST __auth_got 0x100008000 0xC009000000000000 bind   0x0000 true  IA  0x0      libSystem ___assert_rtn
# ARM64E: __DATA_CONST __auth_got 0x100008008 0xC009000000000001 bind   0x0000 true  IA  0x0      libSystem ___snprintf_chk
# ARM64E: __DATA_CONST __auth_got 0x100008010 0xC009000000000002 bind   0x0000 true  IA  0x0      libSystem _free
# ARM64E: __DATA_CONST __auth_got 0x100008018 0xC009000000000003 bind   0x0000 true  IA  0x0      libSystem _malloc
# ARM64E: __DATA_CONST __auth_got 0x100008020 0xC009000000000004 bind   0x0000 true  IA  0x0      libSystem _printf
# ARM64E: __DATA_CONST __auth_got 0x100008028 0xC001000000000005 bind   0x0000 true  IA  0x0      libSystem _strlen
# ARM64E: __DATA       __data     0x10000C000 0x8008000000007CA8 rebase 0x0000 false IA                     0x7CA8
# ARM64E: __DATA       __data     0x10000C008 0x8008000000007DEC rebase 0x0000 false IA                     0x7DEC
# ARM64E: __DATA       __data     0x10000C010 0x8008000000007D3C rebase 0x0000 false IA                     0x7D3C
# ARM64E: __DATA       __data     0x10000C018 0x4008000000000006 bind                    0x0      libSystem ___stderrp
# ARM64E: __DATA       __data     0x10000C020 0x8008000000007CA8 rebase 0x0000 false IA                     0x7CA8
# ARM64E: __DATA       __data     0x10000C028 0x000000010000C030 rebase                                     0x10000C030

	#
	# PTR_64 (binds, rebases)
	#
	${OTOOLC} -dyld_info tool_PTR_64 | $(CHECK) -p PTR_64
# PTR_64: tool_PTR_64:
# PTR_64: dyld information:
# PTR_64: segment      section address     pointer            type   addend   dylib     symbol/vm address
# PTR_64: __DATA_CONST __got   0x100008000 0x8010000000000000 bind   0x0      libSystem ___assert_rtn
# PTR_64: __DATA_CONST __got   0x100008008 0x8010000000000001 bind   0x0      libSystem ___snprintf_chk
# PTR_64: __DATA_CONST __got   0x100008010 0x8010000000000002 bind   0x0      libSystem _free
# PTR_64: __DATA_CONST __got   0x100008018 0x8010000000000003 bind   0x0      libSystem _malloc
# PTR_64: __DATA_CONST __got   0x100008020 0x8010000000000004 bind   0x0      libSystem _printf
# PTR_64: __DATA_CONST __got   0x100008028 0x8000000000000005 bind   0x0      libSystem _strlen
# PTR_64: __DATA       __data  0x10000C000 0x0010000100007CD0 rebase                    0x100007CD0
# PTR_64: __DATA       __data  0x10000C008 0x0010000100007E0C rebase                    0x100007E0C
# PTR_64: __DATA       __data  0x10000C010 0x0010000100007D60 rebase                    0x100007D60
# PTR_64: __DATA       __data  0x10000C018 0x8010000000000006 bind   0x0      libSystem ___stderrp
# PTR_64: __DATA       __data  0x10000C020 0x0010000100007CD0 rebase                    0x100007CD0
# PTR_64: __DATA       __data  0x10000C028 0x000000010000C030 rebase                    0x10000C030

	#
	# PTR_32 (standard userspace binds & rebases)
	#
	${OTOOLC} -dyld_info tool_PTR_32 | $(CHECK) -p PTR_32
# PTR_32: tool_PTR_32:
# PTR_32: dyld information:
# PTR_32: segment      section address pointer    type   addend   dylib     symbol/vm address
# PTR_32: __DATA_CONST __got   0xC000  0x84000000 bind   0x0      libSystem ___assert_rtn
# PTR_32: __DATA_CONST __got   0xC004  0x84000001 bind   0x0      libSystem ___snprintf_chk
# PTR_32: __DATA_CONST __got   0xC008  0x84000002 bind   0x0      libSystem _free
# PTR_32: __DATA_CONST __got   0xC00C  0x84000003 bind   0x0      libSystem _malloc
# PTR_32: __DATA_CONST __got   0xC010  0x84000004 bind   0x0      libSystem _printf
# PTR_32: __DATA_CONST __got   0xC014  0x80000005 bind   0x0      libSystem _strlen
# PTR_32: __DATA       __data  0x10000 0x0400BCA8 rebase                    0xBCA8
# PTR_32: __DATA       __data  0x10004 0x0400BDF0 rebase                    0xBDF0
# PTR_32: __DATA       __data  0x10008 0x0400BD34 rebase                    0xBD34
# PTR_32: __DATA       __data  0x1000C 0x84000006 bind   0x0      libSystem ___stderrp
# PTR_32: __DATA       __data  0x10010 0x0400BCA8 rebase                    0xBCA8
# PTR_32: __DATA       __data  0x10014 0x00010018 rebase                    0x10018

	#
	# PTR_32_CACHE (dyld shared library cache rebases)
	#
	echo "No test for DYLD_CHAINED_PTR_32_CACHE"

	#
	# PTR_32_FIRMWARE (firmware rebases)
	#
	${OTOOLC} -dyld_info tool_PTR_32_FIRMWARE | $(CHECK) -p PTR_32_FIRMWARE
# PTR_32_FIRMWARE: tool_PTR_32_FIRMWARE:
# PTR_32_FIRMWARE: dyld information:
# PTR_32_FIRMWARE: segment section address pointer    type   addend   dylib symbol/vm address
# PTR_32_FIRMWARE: __DATA  __data  0x8000  0x00008FAC rebase                0x8FAC
# PTR_32_FIRMWARE: __DATA  __data  0x8FA4  0x04008008 rebase                0x8008
# PTR_32_FIRMWARE: __DATA  __data  0x8FA8  0x00008FA0 rebase                0x8FA0

	#
	# DYLD_CHAINED_PTR_ARM64E_USERLAND24 (arm64e w/ > 65535 symbols)
	#
	${OTOOLC} -dyld_info tool_PTR_ARM64E_USERLAND24			     \
	| $(CHECK) -p PTR_ARM64E_USERLAND24
# PTR_ARM64E_USERLAND24: tool_PTR_ARM64E_USERLAND24:
# PTR_ARM64E_USERLAND24: dyld information:
# PTR_ARM64E_USERLAND24: segment      section    address     pointer            type   div    addr  key addend   dylib     symbol/vm address
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228000 0xC009000000000000 bind   0x0000 true  IA  0x0      dylib     _dylib00001
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228008 0xC009000000000001 bind   0x0000 true  IA  0x0      dylib     _dylib00002
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228010 0xC009000000000002 bind   0x0000 true  IA  0x0      dylib     _dylib00003
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228018 0xC009000000000003 bind   0x0000 true  IA  0x0      dylib     _dylib00004
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228020 0xC009000000000004 bind   0x0000 true  IA  0x0      dylib     _dylib00005
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228028 0xC009000000000005 bind   0x0000 true  IA  0x0      dylib     _dylib00006
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x100228030 0xC009000000000006 bind   0x0000 true  IA  0x0      dylib     _dylib00007
# [ cut for extreme length ]
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B48 0xC009000000011169 bind   0x0000 true  IA  0x0      dylib     _dylib69994
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B50 0xC00900000001116A bind   0x0000 true  IA  0x0      dylib     _dylib69995
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B58 0xC00900000001116B bind   0x0000 true  IA  0x0      dylib     _dylib69996
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B60 0xC00900000001116C bind   0x0000 true  IA  0x0      dylib     _dylib69997
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B68 0xC00900000001116D bind   0x0000 true  IA  0x0      dylib     _dylib69998
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B70 0xC00900000001116E bind   0x0000 true  IA  0x0      dylib     _dylib69999
# PTR_ARM64E_USERLAND24: __DATA_CONST __auth_got 0x1002B0B78 0xC00100000001116F bind   0x0000 true  IA  0x0      dylib     _dylib70000
# PTR_ARM64E_USERLAND24: __DATA       __data     0x1002B4000 0x4008000000011170 bind                    0x0      libSystem ___stderrp
# PTR_ARM64E_USERLAND24: __DATA       __data     0x1002B4008 0x8008000000005184 rebase 0x0000 false IA                     0x5184
# PTR_ARM64E_USERLAND24: __DATA       __data     0x1002B4010 0x00000000002B4018 rebase                                     0x1002B4018

	#
	# Other formats
	#
	echo "No test for DYLD_CHAINED_PTR_64_OFFSET"
	echo "No test for DYLD_CHAINED_PTR_ARM64E_KERNEL"
	echo "No test for DYLD_CHAINED_PTR_64_KERNEL_CACHE"
	echo "No test for DYLD_CHAINED_PTR_ARM64E_USERLAND"
	echo "No test for DYLD_CHAINED_PTR_ARM64E_FIRMWARE"
	echo "No test for DYLD_CHAINED_PTR_X86_64_KERNEL_CACHE"

	echo PASS

clean:
	true

tool_clean:
	rm -f tool_*

tool_all: tool_NONE tool_THREADED tool_ARM64E tool_PTR_64 tool_PTR_32

# NONE is any non-arm64e app using LC_DYLD_INFO. In this case, an arm64 binary
# targetting iOS 13.0.
tool_NONE: tool.c
	xcrun -sdk iphoneos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk iphoneos -version Path 2>/dev/null`	     \
	-arch arm64 -miphoneos-version-min=13.0

# THREADED is an arm64e app using LC_DYLD_INFO's threaded rebase format. This
# largely exercises the ARM64E pointer formats. This test exists to exercise
# otool's tooling.
tool_THREADED: tool.c
	xcrun -sdk iphoneos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk iphoneos -version Path 2>/dev/null`	     \
	-arch arm64e -miphoneos-version-min=13.0

# ARM64E is an arm64e app using chained fixups. This test is an arm64e binary
# targetting iOS 14.0
tool_ARM64E: tool.c
	xcrun -sdk iphoneos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk iphoneos -version Path 2>/dev/null`	     \
	-arch arm64e -miphoneos-version-min=14.0

# PTR_64 is a 64-bit app using chained fixups. This test is an arm64 binary
# targetting iOS 14.0 with -fixup_chains enabled.
tool_PTR_64: tool.c
	xcrun -sdk iphoneos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk iphoneos -version Path 2>/dev/null`	     \
	-arch arm64 -miphoneos-version-min=14.0 -Wl,-fixup_chains

# PTR_32 is a 32-bit app using chained fixups. This test is an arm64_32 binary
# targetting watchOS 7.0 with -fixup_chains enabled.
tool_PTR_32: tool.c
	xcrun -sdk watchos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk watchos -version Path 2>/dev/null`	     \
	-arch arm64_32 -mwatchos-version-min=7.0 -Wl,-fixup_chains

# PTR_32_FIRMWARE is a 32-bit firmware binary using chained fixups. This test
# is an arm64_32 firmware targetting watchOS 7.0 with section-based chained
# fixups.
tool_PTR_32_FIRMWARE: firmware.c
	xcrun -sdk watchos cc -o $@ $^					     \
	-isysroot `xcodebuild -sdk watchos -version Path 2>/dev/null`	     \
	-arch arm64_32 -mwatchos-version-min=7.0 -Wl,-fixup_chains_section   \
	-static -nostdlib -Wl,-e,_start

# PTR_ARM64E_USERLAND24 is a 64-bit arm64e binary that requires more than 65535
# symbols. Such binaries use a 24-bit wide target ordinal (symbol ordinal)
# rather than a 16-bit wide value. This test is an arm64e executable that links
# against a dylib exporting 70,000 symbols.
tool_PTR_ARM64E_USERLAND24:
	rm -rf src lib
	mkdir src lib
	echo "#include <stdio.h>"			>> src/tool_ext
	echo ""						>> src/tool_ext
	echo "void function(void) { }"			>> src/tool_ext
	echo "FILE** unauth_bind = &stderr;" 		>> src/tool_ext
	echo "void (*auth_rebase)(void) = &function;"	>> src/tool_ext
	echo "int x = 0, *unauth_rebase = &x;"		>> src/tool_ext
	echo "" 					>> src/tool_ext
	echo "int main(void) {" 			>> src/tool_main
	echo "  int x = 0;" 				>> src/tool_main

	i=1;								     \
	while [ $$i -le 70000 ]; do					     \
	  s=`printf "%05d" $$i`;					     \
	  echo "int dylib$$s(void) { return 0; }" 	>> src/dylib.c;	     \
	  echo "extern int dylib$$s(void);" 		>> src/tool_ext;     \
	  echo "  x += dylib$$s();" 			>> src/tool_main;    \
	  i=$$((i+1));							     \
	done

	# build a dylib with 70,000 exports.
	xcrun -sdk iphoneos cc -arch arm64e -o lib/dylib.dylib		     \
	  src/dylib.c -dynamiclib;

	# build the tool
	echo "  return x;"				>> src/tool_main
	echo "}"					>> src/tool_main
	cat src/tool_ext src/tool_main > src/tool.c
	xcrun -sdk iphoneos cc -arch arm64e -o $@ src/tool.c lib/dylib.dylib

	# clean up
	rm -rf src lib dylib.dylib
