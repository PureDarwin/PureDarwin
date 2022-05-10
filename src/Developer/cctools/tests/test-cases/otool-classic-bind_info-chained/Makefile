# PLATFORM: MACOS IOS WATCHOS TVOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

VALID_ARCHS := $(filter-out i386 armv7k,$(VALID_ARCHS))

all:
	for arch in ${VALID_ARCHS};					     \
	do								     \
	  ${CC} -arch $$arch -Wl,-fixup_chains -o verstool		     \
	    ${TESTROOT}/src/verstool.c;					     \
	  ${OTOOLC} -bind_info verstool > output.txt;			     \
	  if [ "$$arch" == "arm64e" ]; then				     \
	    ${CHECK} -p CHECK64E -i output.txt;				     \
	  else								     \
	    ${CHECK} -p CHECK -i output.txt;				     \
	  fi								     \
	done

	echo PASS

################################################################################
#
# CHECK: verstool:
# CHECK-NEXT: dyld information:
# CHECK-NEXT: segment      section address\s* pointer\s* type   addend   dylib     symbol/vm address
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___error
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___memcpy_chk
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___memset_chk
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___snprintf_chk
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___stack_chk_fail
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___stack_chk_guard
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem ___stderrp
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _calloc
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _chmod
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _close
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _exit
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _fprintf
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _free
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _fstat.*
# CHECK: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _mkstemp
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _open
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _perror
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _printf
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _read
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _reallocf
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _rename
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strchr
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strcmp
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strdup
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strerror
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strlen
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strrchr
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _strtoul
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_build_version_command
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_load_command
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_mach_h\[\[:xdigit:\]\]+r_64
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_section
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_section_64
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_segment_command
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_segment_command_64
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _swap_version_min_command
# CHECK-NEXT: __DATA_CONST __got   0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ bind   0x0      libSystem _write
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+
# CHECK-NEXT: __DATA_CONST __const 0x\[\[:xdigit:\]\]+ +0x\[\[:xdigit:\]\]+ rebase                    0x\[\[:xdigit:\]\]+

################################################################################
#
# CHECK64E: verstool:
# CHECK64E-NEXT: dyld information:
# CHECK64E-NEXT: segment      section    address     pointer            type   div    addr  key addend   dylib     symbol/vm address
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem ___error
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem ___memcpy_chk
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem ___memset_chk
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem ___snprintf_chk
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem ___stack_chk_fail
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _calloc
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _chmod
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _close
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _exit
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _fprintf
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _free
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _fstat
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _memcpy
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _mkstemp
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _open
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _perror
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _printf
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _read
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _reallocf
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _rename
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strchr
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strcmp
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strdup
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strerror
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strlen
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strrchr
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _strtoul
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_build_version_command
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_load_command
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_mach_h\[\[:xdigit:\]\]+r_64
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_section
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_section_64
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_segment_command
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_segment_command_64
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _swap_version_min_command
# CHECK64E-NEXT: __DATA_CONST __auth_got 0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind   0x\[\[:xdigit:\]\]+ true  IA  0x0      libSystem _write
# CHECK64E-NEXT: __DATA_CONST __got      0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind                    0x0      libSystem ___stack_chk_guard
# CHECK64E-NEXT: __DATA_CONST __got      0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ bind                    0x0      libSystem ___stderrp
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+
# CHECK64E-NEXT: __DATA_CONST __const    0x\[\[:xdigit:\]\]+ 0x\[\[:xdigit:\]\]+ rebase                                     0x\[\[:xdigit:\]\]+

clean:
	rm -f verstool output.txt
