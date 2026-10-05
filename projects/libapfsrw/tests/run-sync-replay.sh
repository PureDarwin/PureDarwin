#!/usr/bin/env bash
# Copyright (c) 2026 PureDarwin contributors. SPDX-License-Identifier: MIT
# Native regression harness, not a standalone build of the PD kernel or userland.
set -euo pipefail
if [[ $# != 1 ]]; then
    echo "usage: $0 APFS-CONTAINER-IMAGE (copied, never modified)" >&2
    exit 2
fi
test_dir=$(cd -- "$(dirname -- "$0")" && pwd)
lib_dir=$(cd -- "$test_dir/.." && pwd)
scratch_dir=$(mktemp -d)
trap 'rm -rf -- "$scratch_dir"' EXIT
mkdir -p "$scratch_dir/shim/kern" "$scratch_dir/shim/sys"
touch "$scratch_dir/shim/kern/clock.h" "$scratch_dir/shim/sys/disk.h"
${CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-parameter \
    ${SYNC_REPLAY_CFLAGS:-} -DAPFSRW_KERNEL -include "$test_dir/sync-replay-env.h" \
    -I"$scratch_dir/shim" -I"$lib_dir/include" -I"$lib_dir/third_party/utf8proc" \
    "$lib_dir/src/apfsrw.c" "$lib_dir/src/apfsrw_kern.c" \
    "$lib_dir/src/apfsrw_name.c" "$test_dir/sync-replay.c" -o "$scratch_dir/replay"
for mode in 0 1; do
    cp --reflink=auto --sparse=always -- "$1" "$scratch_dir/replay-$mode.img"
    "$scratch_dir/replay" "$scratch_dir/replay-$mode.img" "$mode" | tee "$scratch_dir/result-$mode"
done
baseline=$(awk '{split($2,a,"="); print a[2]}' "$scratch_dir/result-0")
fixed=$(awk '{split($2,a,"="); print a[2]}' "$scratch_dir/result-1")
if (( fixed >= baseline )); then
    echo "FAIL: superseded-write cancellation did not reduce disk writes" >&2
    exit 1
fi
echo "PASS: contents, partial-operation rollback, durable reopen, empty batch, barrier count, write reduction, discard guards"
