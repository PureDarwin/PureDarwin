# PLATFORM: MACOS

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

# force llvm-otool to use llvm-objdump, in case the user overrode this value.
OTOOL_PATH = llvm-objdump

all:
	#
	# Flag: -f
	#

	${LLOTOOL} -show-objdump-command -f / 2>&1 |			     \
	    ${CHECK} -p FLAGf
# FLAGf: \^\+ .*objdump -macho -universal-headers -non-verbose /

	${LLOTOOL} -show-objdump-command -fv / 2>&1 |			     \
	    ${CHECK} -p FLAGfv
	${LLOTOOL} -show-objdump-command -fV / 2>&1 |			     \
	    ${CHECK} -p FLAGfv
# FLAGfv: \^\+ .*objdump -macho -universal-headers /

	#
	# Flag: -a
	#

	${LLOTOOL} -show-objdump-command -a / 2>&1 |			     \
	    ${CHECK} -p FLAGa
# FLAGa: \^\+ .*objdump -macho -archive-headers -non-verbose /

	${LLOTOOL} -show-objdump-command -av / 2>&1 |			     \
	    ${CHECK} -p FLAGav
# FLAGav: \^\+ .*objdump -macho -archive-headers /

	${LLOTOOL} -show-objdump-command -aV / 2>&1 |			     \
	    ${CHECK} -p FLAGaV
# FLAGaV: \^\+ .*objdump -macho -archive-headers -archive-member-offsets /

	#
	# Flag: -h
	#

	${LLOTOOL} -show-objdump-command -h / 2>&1 |			     \
	    ${CHECK} -p FLAGh
# FLAGh: \^\+ .*objdump -macho -private-header -non-verbose /

	${LLOTOOL} -show-objdump-command -hv / 2>&1 |			     \
	    ${CHECK} -p FLAGhv
	${LLOTOOL} -show-objdump-command -hV / 2>&1 |			     \
	    ${CHECK} -p FLAGhv
# FLAGhv: \^\+ .*objdump -macho -private-header /

	#
	# Flag: -l
	#

	${LLOTOOL} -show-objdump-command -l / 2>&1 |			     \
	    ${CHECK} -p FLAGl
# FLAGl: \^\+ .*objdump -macho -private-headers -non-verbose /

	${LLOTOOL} -show-objdump-command -lv / 2>&1 |			     \
	    ${CHECK} -p FLAGlv
	${LLOTOOL} -show-objdump-command -lV / 2>&1 |			     \
	    ${CHECK} -p FLAGlv
# FLAGlv: \^\+ .*objdump -macho -private-headers /

	#
	# Flag: -L
	#

	${LLOTOOL} -show-objdump-command -L / 2>&1 |			     \
	    ${CHECK} -p FLAGL
# FLAGL: \^\+ .*objdump -macho -dylibs-used -non-verbose /

	${LLOTOOL} -show-objdump-command -Lv / 2>&1 |			     \
	    ${CHECK} -p FLAGLv
	${LLOTOOL} -show-objdump-command -LV / 2>&1 |			     \
	    ${CHECK} -p FLAGLv
# FLAGLv: \^\+ .*objdump -macho -dylibs-used /

	#
	# Flag: -D
	#

	${LLOTOOL} -show-objdump-command -D / 2>&1 |			     \
	    ${CHECK} -p FLAGD
# FLAGD: \^\+ .*objdump -macho -dylib-id -non-verbose /

	${LLOTOOL} -show-objdump-command -Dv / 2>&1 |			     \
	    ${CHECK} -p FLAGDv
	${LLOTOOL} -show-objdump-command -DV / 2>&1 |			     \
	    ${CHECK} -p FLAGDv
# FLAGDv: \^\+ .*objdump -macho -dylib-id /

	#
	# Flag: -t
	#

	${LLOTOOL} -show-objdump-command -t / 2>&1 |		     \
	    ${CHECK} -p FLAGt
# FLAGt: \^\+ .*objdump -macho -section __TEXT,__text -full-leading-addr -print-imm-hex -non-verbose -no-symbolic-operands -no-show-raw-insn /

	${LLOTOOL} -show-objdump-command -tv / 2>&1 |		     \
	    ${CHECK} -p FLAGtv
# FLAGtv: \^\+ .*objdump -macho -disassemble -full-leading-addr -print-imm-hex -no-symbolic-operands -no-show-raw-insn /

	${LLOTOOL} -show-objdump-command -tV / 2>&1 |		     \
	    ${CHECK} -p FLAGtV
# FLAGtV: \^\+ .*objdump -macho -disassemble -full-leading-addr -print-imm-hex -no-show-raw-insn /

	#
	# Flag: -x
	#

	${LLOTOOL} -show-objdump-command -x / 2>&1 |		     \
	    ${CHECK} -p FLAGx -v
# FLAGx: \^\+ .*objdump -macho -section ,__text -full-leading-addr -print-imm-hex -non-verbose -no-symbolic-operands -no-show-raw-insn /

	${LLOTOOL} -show-objdump-command -xv / 2>&1 |		     \
	    ${CHECK} -p FLAGxv
# FLAGxv: \^\+ .*objdump -macho -disassemble-all -full-leading-addr -print-imm-hex -no-symbolic-operands -no-show-raw-insn /

	${LLOTOOL} -show-objdump-command -xV / 2>&1 |		     \
	    ${CHECK} -p FLAGxV
# FLAGxV: \^\+ .*objdump -macho -disassemble-all -full-leading-addr -print-imm-hex -no-show-raw-insn /


	echo PASS

clean:
	rm -f hello
