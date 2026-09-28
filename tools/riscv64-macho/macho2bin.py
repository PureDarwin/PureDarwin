#!/usr/bin/env python3
# flat image of a 64-bit mach-o, every segment's file bytes at vmaddr minus the lowest vmaddr
# usage: macho2bin.py <in> <out>
import struct, sys
d = open(sys.argv[1], 'rb').read()
magic, cpu, sub, ftype, ncmds, sizeofcmds, flags, _ = struct.unpack_from('<IiiIIIII', d, 0)
assert magic == 0xfeedfacf, 'not a 64-bit mach-o'
segs = []
off = 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', d, off)
    if cmd == 0x19:
        name = d[off+8:off+24].split(b'\0')[0].decode()
        vmaddr, vmsize, fileoff, filesize = struct.unpack_from('<QQQQ', d, off+24)
        if filesize and name != '__LINKEDIT':
            segs.append((vmaddr, fileoff, filesize, name))
    off += size
base = min(s[0] for s in segs)
end = max(s[0] + s[2] for s in segs)
img = bytearray(end - base)
for vmaddr, fileoff, filesize, name in segs:
    img[vmaddr-base:vmaddr-base+filesize] = d[fileoff:fileoff+filesize]
    print('%-12s 0x%x +0x%x' % (name, vmaddr, filesize))
open(sys.argv[2], 'wb').write(img)
print('base 0x%x, %d bytes' % (base, len(img)))
