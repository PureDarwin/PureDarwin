#!/usr/bin/env bash
# libcalls llvm invents on riscv64 darwin carry the libSystem names arm64 darwin uses, and they
# resolve against the real riscv64 libSystem
# usage: check.sh <clang> <ld> <llvm-objdump> <srcdir> <libSystem prefix>
set -e
CC=$1; LD=$2; OD=$3; SRC=$4; SYS=$5
V="-platform_version macos 11.0 11.0"
fail() { echo "FAIL: $*"; exit 1; }
has() { grep -qE "$2" <<<"$1" || fail "$3"; }
hasnt() { ! grep -qE "$2" <<<"$1" || fail "$3"; }
fn() { sed -n "/^_$1:/,/^_[a-z0-9_]*:/p" <<<"$2"; }

for o in -O1 -O2 -Os; do
  asm=$($CC -target riscv64-apple-darwin20.4 $o -S $SRC/libcalls.c -o -)
  has "$(fn exp10_d "$asm")" "(tail|call)\s+___exp10$" "pow(10, x) is not ___exp10 at $o"
  has "$(fn exp10_f "$asm")" "(tail|call)\s+___exp10f$" "powf(10, x) is not ___exp10f at $o"
  hasnt "$asm" "\s_exp10f?$" "bare _exp10 reference at $o"
  # the pair comes back in fa0 and fa1, like the c struct __double2 return
  has "$(fn sincos_d "$asm")" "call\s+___sincos_stret$" "sin+cos is not ___sincos_stret at $o"
  has "$(fn sincos_d "$asm")" "fsd\s+fa1" "cos is not read from fa1 at $o"
  has "$(fn sincos_f "$asm")" "call\s+___sincosf_stret$" "sinf+cosf is not ___sincosf_stret at $o"
  has "$(fn sincos_f "$asm")" "fsw\s+fa1" "cosf is not read from fa1 at $o"
  has "$(fn zero "$asm")" "(tail|call)\s+_bzero$" "memset 0 is not bzero at $o"
done

# without a hard double abi the struct return does not map onto two fprs, sin and cos stay apart
asm=$($CC -target riscv64-apple-darwin20.4 -O2 -mabi=lp64f -S $SRC/libcalls.c -o -)
hasnt "$(fn sincos_d "$asm")" "___sincos_stret" "__sincos_stret used under lp64f"
has "$(fn sincos_f "$asm")" "___sincosf_stret" "__sincosf_stret missing under lp64f"
asm=$($CC -target riscv64-apple-darwin20.4 -O2 -march=rv64imac -mabi=lp64 -S $SRC/libcalls.c -o -)
hasnt "$asm" "_stret" "__sincos_stret used under soft float"
has "$(fn exp10_d "$asm")" "___exp10$" "soft float pow(10, x) is not ___exp10"

# riscv64 macos starts where arm64 does, an unversioned triple is macos 11
has "$($CC -target riscv64-apple-macos -S $SRC/libcalls.c -o -)" "build_version macos, 11, 0" "riscv64 macos floor is not 11.0"

# every one of them resolves in the riscv64 libSystem
$CC -target riscv64-apple-darwin20.4 -O2 -c $SRC/libcalls.c -o libcalls.o
$LD -arch riscv64 $V -dylib -o liblibcalls.dylib libcalls.o \
  -dylib_file /usr/lib/system/libdyld.dylib:$SYS/usr/lib/system/libdyld.dylib \
  -L$SYS/usr/lib -lSystem
binds=$($OD --macho --dyld-info liblibcalls.dylib)
for s in ___exp10 ___exp10f ___sincos_stret ___sincosf_stret _bzero; do
  has "$binds" "libSystem\s+$s$" "$s does not bind to libSystem"
done
echo "riscv64 darwin libcall checks passed"
