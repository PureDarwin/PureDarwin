#!/usr/bin/env bash
# userland mach-o shape for riscv64: two-level dylibs and executables with chained fixups,
# non-lazy stubs through __got, tlv descriptors, dwarf-only unwind and a self-rebasing dyld
# usage: check.sh <clang> <ld> <llvm-objdump> <srcdir>
set -e
CC=$1; LD=$2; OD=$3; SRC=$4
T="-target riscv64-apple-darwin20.4 -O2"
V="-platform_version macos 11.0 11.0"
fail() { echo "FAIL: $*"; exit 1; }
has() { grep -qE "$2" <<<"$1" || fail "$3"; }
hasnt() { ! grep -qE "$2" <<<"$1" || fail "$3"; }

# predefined types and layout match arm64 darwin, in every language, userland and kernel
for lang in c c++ objective-c; do
  for k in "" "-mkernel"; do
    $CC -target riscv64-apple-darwin20.4 -fblocks $k -x $lang -c $SRC/abi_types.c -o abi_types.o || fail "abi types for $lang [$k]"
  done
done
# char_traits<wchar_t>::int_type is wint_t, it has to mangle as int like everywhere on darwin
$CC -target riscv64-apple-darwin20.4 -x c++ -c $SRC/abi_types.c -o abi_types.o
has "$($OD --macho --syms abi_types.o)" "__Z10takes_winti" "wint_t does not mangle as int"

# plain char stays signed for userland and kernel code alike
for k in "" "-mkernel"; do
  $CC $T $k -c $SRC/signed_char.c -o signed_char.o || fail "char is unsigned with [$k]"
done
# non-leaf functions set s0 up as the frame pointer at every level, leaf ones only on request
fn() { sed -n "/^_$1:/,/^_[a-z]*:\|^\s*\.cfi_endproc/p" <<<"$2"; }
for o in -O0 -O2; do
  for k in "" "-mkernel"; do
    asm=$($CC -target riscv64-apple-darwin20.4 $o $k -S $SRC/frame_pointer.c -o -)
    grep -qE "addi\s+s0, sp" <<<"$(fn nonleaf "$asm")" || fail "no frame record in non-leaf at [$o $k]"
    ! grep -qE "addi\s+s0, sp" <<<"$(fn leaf "$asm")" || fail "leaf frame record at [$o $k]"
  done
done
asm=$($CC -target riscv64-apple-darwin20.4 -O2 -mno-omit-leaf-frame-pointer -S $SRC/frame_pointer.c -o -)
grep -qE "addi\s+s0, sp" <<<"$(fn leaf "$asm")" || fail "-mno-omit-leaf-frame-pointer ignored"
asm=$($CC -target riscv64-apple-darwin20.4 -O2 -fomit-frame-pointer -S $SRC/frame_pointer.c -o -)
! grep -qE "addi\s+s0, sp" <<<"$(fn nonleaf "$asm")" || fail "-fomit-frame-pointer ignored"

$CC $T -c $SRC/sys.c -o sys.o
$CC -x c++ $T -fexceptions -frtti -nostdinc++ -c $SRC/foo.cpp -o foo.o
$CC $T -c $SRC/main.c -o main.o
$LD -arch riscv64 $V -dylib -install_name /usr/lib/libSystem.B.dylib -o libSystem.B.dylib sys.o
$LD -arch riscv64 $V -dylib -install_name /usr/lib/libfoo.dylib -o libfoo.dylib foo.o libSystem.B.dylib
$LD -arch riscv64 $V -o main main.o libfoo.dylib libSystem.B.dylib

for f in libfoo.dylib main; do
  hdr=$($OD --macho --private-headers $f)
  has "$hdr" "LC_DYLD_CHAINED_FIXUPS" "$f has no chained fixups"
  hasnt "$hdr" "LC_DYLD_INFO" "$f still has dyld info"
  has "$hdr" "TWOLEVEL" "$f is not two-level"
  has "$($OD --macho --chained-fixups $f)" "pointer_format = 2 \(DYLD_CHAINED_PTR_64\)" "$f pointer format"
  secs=$($OD --macho -h $f)
  hasnt "$secs" "__la_symbol_ptr|__stub_helper|__auth_stubs|__unwind_info" "$f has lazy binding, auth stubs or compact unwind"
  has "$secs" "__eh_frame" "$f lost __eh_frame"
  stubs=$($OD --macho --private-headers $f | grep -A12 "sectname __stubs")
  has "$stubs" "reserved2 12 \(size of stubs\)" "$f stub size"
  has "$($OD --macho -d --section=__TEXT,__stubs $f)" "auipc	t3.*" "$f stub shape"
done

# instructions reach an import only through the got, a direct address must be refused
for s in import_lla import_lui; do
  $CC $T -c $SRC/$s.s -o $s.o
  out=$($LD -arch riscv64 $V -dylib -o $s.dylib $s.o libfoo.dylib 2>&1) && fail "$s linked against an import"
  grep -q "illegal text-relocation to '_foo_counter'" <<<"$out" || fail "$s: $out"
done
$CC $T -c $SRC/import_got.s -o import_got.o
$LD -arch riscv64 $V -dylib -o import_got.dylib import_got.o libfoo.dylib
has "$($OD --macho --dyld-info import_got.dylib)" "__got.*bind.*libfoo.*_foo_counter" "got import not bound"

# objc: image info kept, category merged into its class, method lists and selrefs intact
$CC -x objective-c $T -c $SRC/objc.m -o objc.o
$LD -arch riscv64 $V -dylib -install_name /usr/lib/libobjctest.dylib -o libobjctest.dylib objc.o libSystem.B.dylib
python3 $SRC/check_objc.py libobjctest.dylib || fail "objc image"

# usdt: probe calls become nop and is-enabled checks li a0, 0, the dof itself needs the
# host's libdtrace.dylib, which a linux host does not have for any arch
$CC $T -c $SRC/dtrace.c -o dtrace.o
$LD -arch riscv64 $V -dylib -no_dtrace_dof -o libdtrace.dylib dtrace.o libSystem.B.dylib
dis=$($OD --macho -d libdtrace.dylib)
has "$dis" "li	a0, 0x0" "is-enabled site not cleared"
has "$dis" "	nop" "probe site not a nop"
hasnt "$dis" "jal.*dtrace" "a dtrace call survived"
$LD -arch riscv64 -r -o dtrace_r.o dtrace.o
has "$($OD --macho -r dtrace_r.o)" "___dtrace_probe\\\$pdtest" "ld -r lost the probe relocation"

# padding between code atoms is nops, zero bytes would be an illegal instruction
$CC $T -c $SRC/pad.s -o pad.o
$LD -arch riscv64 $V -dylib -o libpad.dylib pad.o libSystem.B.dylib
pad=$($OD --macho -d libpad.dylib | sed -n '/^_pad_a:/,/^_pad_b:/p')
hasnt "$pad" "unimp|0x0000	" "code padding is not nops"
has "$pad" "nop" "code padding is not nops"

# merged globals stay local, a dylib exports only its own names
$CC $T -c $SRC/merged_globals.c -o merged_globals.o
$LD -arch riscv64 $V -dylib -o libmerged.dylib merged_globals.o libSystem.B.dylib
exports=$($OD --macho --exports-trie libmerged.dylib)
hasnt "$exports" "MergedGlobals" "a merged global is exported"
has "$exports" "_ext_a" "ext_a not exported"
has "$exports" "_ext_b" "ext_b not exported"

mh=$($OD --macho --private-headers main)
has "$mh" "LC_MAIN" "main has no LC_MAIN"
has "$mh" "name /usr/lib/dyld" "main has no LC_LOAD_DYLINKER"
has "$($OD --macho --private-header main)" "PIE" "main is not PIE"
has "$($OD --macho --private-header libfoo.dylib)" "MH_HAS_TLV_DESCRIPTORS" "libfoo has no tlv flag"
has "$($OD --macho --dyld-info libfoo.dylib)" "__thread_vars.*bind.*__tlv_bootstrap" "tlv thunk not bound"
has "$($OD --macho --dyld-info main)" "__got.*bind.*libfoo.*_foo_area" "main does not bind foo_area"
has "$($OD --macho -d main)" "jal" "main has no calls"

$CC $T -c $SRC/dyld.c -o dyld.o
$CC $T -c $SRC/dyld_start.s -o dyld_start.o
$LD -arch riscv64 $V -dylinker -dylinker_install_name /usr/lib/dyld -e __dyld_start -fixup_chains -o dyld dyld_start.o dyld.o
dh=$($OD --macho --private-headers dyld)
has "$($OD --macho --private-header dyld)" "DYLINKER" "dyld is not MH_DYLINKER"
has "$dh" "LC_UNIXTHREAD" "dyld has no thread state"
has "$($OD --macho --dyld-info dyld)" "rebase" "dyld has no rebases"
echo "riscv64 userland mach-o checks passed"
