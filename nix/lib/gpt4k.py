# gpt4k.py IMAGE - rewrite a 512-byte-sector GPT for 4096-byte logical blocks, partitions at the same byte ranges
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause
# Apple's NVMe controllers use 4 KiB blocks, so a disk image for them needs its GPT at LBA 1 of 4096 bytes
import struct
import sys
import zlib

OLD, NEW = 512, 4096
RATIO = NEW // OLD
ENTRY = 128


def header(lba, alt, first, last, guid, entries_lba, nentries, entries_crc):
    h = struct.pack("<8sIIIIQQQQ16sQIII", b"EFI PART", 0x00010000, 92, 0, 0, lba, alt, first, last,
                    guid, entries_lba, nentries, ENTRY, entries_crc)
    crc = zlib.crc32(h) & 0xffffffff
    h = h[:16] + struct.pack("<I", crc) + h[20:]
    return h + bytes(NEW - len(h))


def main(path):
    with open(path, "r+b") as f:
        f.seek(0, 2)
        size = f.seek(0, 2)
        f.seek(OLD)
        old = f.read(92)
        if old[:8] != b"EFI PART":
            sys.exit("gpt4k: no 512-byte GPT header in " + path)
        guid = old[56:72]
        entries_lba, nentries, entsize = struct.unpack("<QII", old[72:88])
        f.seek(entries_lba * OLD)
        table = bytearray(f.read(nentries * entsize))

        # every partition must start and end on a 4 KiB boundary to keep its bytes
        out = bytearray()
        for i in range(nentries):
            e = bytearray(table[i * entsize:(i + 1) * entsize][:ENTRY])
            first, last = struct.unpack("<QQ", e[32:48])
            if e[:16] != bytes(16):
                if first % RATIO or (last + 1) % RATIO:
                    sys.exit("gpt4k: partition %d is not 4 KiB aligned" % (i + 1))
                e[32:48] = struct.pack("<QQ", first // RATIO, (last + 1) // RATIO - 1)
            out += e
        table_bytes = bytes(out) + bytes(-len(out) % NEW)
        table_blocks = len(table_bytes) // NEW
        blocks = size // NEW
        crc = zlib.crc32(bytes(out)) & 0xffffffff

        first_usable = 2 + table_blocks
        last_usable = blocks - 2 - table_blocks
        alt = blocks - 1

        # the old header and table lived where block 0 and the new table now sit, clear them first
        f.seek(OLD)
        f.write(bytes((entries_lba * OLD + nentries * entsize) - OLD))
        f.seek(size - 33 * OLD)
        f.write(bytes(33 * OLD))

        # protective MBR, one 0xEE partition over the disk in 4 KiB blocks
        f.seek(446)
        f.write(struct.pack("<B3sB3sII", 0, b"\x00\x02\x00", 0xee, b"\xff\xff\xff", 1,
                            min(blocks - 1, 0xffffffff)))
        f.seek(510)
        f.write(b"\x55\xaa")

        f.seek(1 * NEW)
        f.write(header(1, alt, first_usable, last_usable, guid, 2, nentries, crc))
        f.write(table_bytes)
        f.seek((alt - table_blocks) * NEW)
        f.write(table_bytes)
        f.write(header(alt, 1, first_usable, last_usable, guid, alt - table_blocks, nentries, crc))


main(sys.argv[1])
