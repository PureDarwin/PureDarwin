#!/usr/bin/env bash
# %pcrel_lo reach on mach-o: near users share one auipc, far ones get their own, and all
# of them still read and write the right place when run in qemu
# usage: check.sh <clang> <ld> <srcdir> <hellodir> <macho2bin.py> <qemu-system-riscv64> <llc>
set -e
CC=$1; LD=$2; SRC=$3; H=$4; M2B=$5; QEMU=$6; LLC=$7
T="-target riscv64-apple-darwin20.4 -O2 -ffreestanding -fno-stack-protector -mcmodel=medany"
fail() { echo "FAIL: $*"; exit 1; }
count() { sed -n "/^_$1:/,/-- End function/p" <<<"$2" | grep -c "auipc.*%$3" || true; }

asm=$($CC $T -S $SRC/pcrel.c -o -)
[ "$(count near_load "$asm" pcrel_hi)" = 1 ] || fail "near loads stopped sharing their auipc"
[ "$(count near_store "$asm" pcrel_hi)" = 1 ] || fail "near stores stopped sharing their auipc"
for f in far_load far_store far_fp; do
  [ "$(count $f "$asm" pcrel_hi)" = 2 ] || fail "$f has no second auipc"
done
# 2.8KB of stores through one base needs a fresh auipc part way, not one per store
n=$(count far_code "$asm" pcrel_hi)
[ "$n" -ge 2 ] && [ "$n" -le 3 ] || fail "far_code has $n auipcs for 700 stores"

# a got or tlv descriptor load yields a pointer that is reused, so its pair stays one
# auipc and one load right after it, whatever lies between the uses of the pointer
pair() { sed -n "/^_$1:/,/-- End function/p" <<<"$2" | grep -A1 "auipc.*%$3" | grep -c "pcrel_lo" || true; }
[ "$(count far_ext "$asm" got_pcrel_hi)" = 1 ] && [ "$(pair far_ext "$asm" got_pcrel_hi)" = 1 ] || fail "far_ext got pair"
tasm=$($CC -target riscv64-apple-darwin20.4 -O2 -S $SRC/tlv.c -o -)
[ "$(pair far_tlv "$tasm" tlvp_pcrel_hi)" = 1 ] || fail "far_tlv descriptor pair"
$CC -target riscv64-apple-darwin20.4 -O2 -c $SRC/tlv.c -o tlv.o || fail "far tlv access"

# a far user reading a copy of the auipc result, left alone it cannot be encoded
M="-mtriple=riscv64-apple-macosx11.0.0 -filetype=obj"
! $LLC $M -start-after=riscv-macho-pcrel-lo $SRC/copied-base.mir -o cb.o 2>/dev/null || fail "copied-base.mir is not far enough to test anything"
$LLC $M -start-before=riscv-macho-pcrel-lo $SRC/copied-base.mir -o cb.o || fail "far user of a copied base"

# users that read a copy of the auipc result, as register allocation leaves them in loops with calls
for o in -O1 -O2 -Os -Oz; do
  $CC -target riscv64-apple-darwin20.4 $o -c $SRC/copied.c -o copied.o || fail "copied base at $o"
  $CC -target riscv64-apple-darwin20.4 $o -x objective-c -fobjc-arc -c $SRC/objc_far.m -o objc_far.o || fail "objc far sends at $o"
done

$CC $T -c $SRC/pcrel.c -o pcrel.o
$CC $T -c $SRC/pcrel_ext.c -o pcrel_ext.o
$CC $T -c $SRC/main.c -o main.o
$CC $T -c $H/start.s -o start.o
$CC $T -Dkmain=kmain_unused -c $H/kmain.c -o hk.o
$LD -arch riscv64 -preload -e _start -pagezero_size 0 -segaddr __TEXT 0x80200000 \
  -o pcrel.macho start.o main.o hk.o pcrel.o pcrel_ext.o
python3 $M2B pcrel.macho pcrel.bin >/dev/null
timeout 30 $QEMU -machine virt -nographic -bios default -kernel pcrel.bin -m 256M > qemu.log 2>&1 || true
grep -a "pcrel checks" qemu.log || fail "no result from qemu"
grep -aq "pcrel checks [0-9]* mismatches 0" qemu.log || fail "a far access went to the wrong place"
echo "riscv64 pcrel reach checks passed"
