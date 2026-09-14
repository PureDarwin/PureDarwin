# libapfsrw

libapfsrw is a small APFS read/write library, written against Apple's public
APFS reference. The same `src/apfsrw.c` builds twice: as a userspace library
used by the image builder and the `apfsrw` CLI, and inside `apfs.kext` behind
the `APFSRW_KERNEL` shim in `include/apfsrw/apfsrw_port.h`. That gives one
implementation of the copy-on-write B-tree write path, validated against
`fsck_apfs`, instead of two copies that would drift apart.

It works on 4096-byte-block, unencrypted, single-volume containers. The write
API covers create, mkdir, mknod, symlink, hard links, rename, unlink, ranged
writes and truncate, whole-file content replacement, attribute changes (mode,
owner, flags, timestamps), and batching many creates into one commit.

```sh
apfsrw info image.apfs
apfsrw ls image.apfs
apfsrw cat image.apfs /hello.txt
apfsrw mkdir image.apfs /new
apfsrw write image.apfs /new/file.txt local-file
```

libapfsrw is MIT licensed (see the SPDX headers). The vendored decoders keep
their own licences: `third_party/lzfse` (BSD-3-Clause, Apple) and
`third_party/libzbitmap` (MIT, Corellium).
