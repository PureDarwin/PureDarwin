#!/bin/sh
# compare.sh OBJ [MIN_ALIGN] - links OBJ with pd_elf_rel and with ld.lld at the same addresses, compares every loaded section
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
set -e
OBJ=$(readlink -f "$1")
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
BASE=$((0x40000000))

cc -O2 -Wall -o "$OUT/test_link" "$HERE/test_link.c"
"$OUT/test_link" "$OBJ" "$OUT" ${2:-1}
# lld is only the reference here: the same object, the same addresses, the same import values
ld.lld -o "$OUT/lld.elf" -T "$OUT/layout.ld" $(cat "$OUT/defsyms") "$OBJ"

fail=0
for name in $(sed -n 's/^  \(\.[a-z.]*\) 0x.*/\1/p' "$OUT/layout.ld"); do
	addr=$(sed -n "s/^  $name \(0x[0-9a-f]*\) .*/\1/p" "$OUT/layout.ld")
	off=$((addr - BASE))
	llvm-objcopy --dump-section "$name=$OUT/lld$name" "$OUT/lld.elf" 2>/dev/null || { echo "skip $name (no bytes)"; continue; }
	size=$(stat -c %s "$OUT/lld$name")
	tail -c +$((off + 1)) "$OUT/image.bin" | head -c "$size" > "$OUT/ours$name"
	if cmp -s "$OUT/lld$name" "$OUT/ours$name"; then
		echo "same $name ($size bytes)"
	else
		echo "DIFF $name: $(cmp -l "$OUT/lld$name" "$OUT/ours$name" | wc -l) bytes differ"
		cmp -l "$OUT/lld$name" "$OUT/ours$name" | head -5
		fail=1
	fi
done
rm -rf "$OUT"
exit $fail
