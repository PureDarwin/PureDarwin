#!/usr/bin/env python3
# every auipc/addi or auipc/ld in each _ctz function must land on the 64-bit de Bruijn constant
# or on a byte-identical copy of the table that function needs
# usage: check_tables.py <image.macho>
import struct, sys

d = open(sys.argv[1], 'rb').read()
assert struct.unpack_from('<I', d, 0)[0] == 0xfeedfacf, 'not a 64-bit mach-o'
segs, syms = [], {}
ncmds = struct.unpack_from('<I', d, 16)[0]
off = 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', d, off)
    if cmd == 0x19:
        vm, vs, fo, fs = struct.unpack_from('<QQQQ', d, off + 24)
        segs.append((vm, fo, fs))
    elif cmd == 0x2:
        symoff, nsyms, stroff, strsize = struct.unpack_from('<IIII', d, off + 8)
        for i in range(nsyms):
            strx, ntype, nsect, ndesc, value = struct.unpack_from('<IBBHQ', d, symoff + 16 * i)
            if ntype & 0xe0 or (ntype & 0xe) != 0xe:
                continue
            name = d[stroff + strx:d.index(b'\0', stroff + strx)].decode()
            syms[name] = value
    off += size

def rd(a, n):
    for vm, fo, fs in segs:
        if vm <= a and a + n <= vm + fs:
            return d[fo + a - vm:fo + a - vm + n]
    return b''

t64, t32 = bytearray(64), bytearray(32)
for i in range(64):
    t64[((0x0218a392cd3d5dbf << i) & (2**64 - 1)) >> 58] = i
for i in range(32):
    t32[((0x077cb531 << i) & 0xffffffff) >> 27] = i
tables = {bytes(t64): 'table64', bytes(t32): 'table32'}

bad, funcs = 0, sorted(n for n in syms if n.startswith('_ctz'))
if not funcs:
    sys.exit('no _ctz functions found')
for name in funcs:
    a = syms[name]
    end = min(v for v in syms.values() if v > a)
    hi, seen = {}, []
    while a < end:
        if struct.unpack('<H', rd(a, 2))[0] & 3 != 3:
            a += 2
            continue
        i = struct.unpack('<I', rd(a, 4))[0]
        op, r, rs, f3 = i & 0x7f, (i >> 7) & 31, (i >> 15) & 31, (i >> 12) & 7
        if op == 0x17:
            v = i & 0xfffff000
            hi[r] = a + (v - (1 << 32) if v & 0x80000000 else v)
        elif rs in hi and ((op == 0x13 and f3 == 0) or (op == 0x03 and f3 == 3)):
            imm = i >> 20
            t = hi.pop(rs) + (imm - 4096 if imm & 0x800 else imm)
            what = 'const64' if rd(t, 8) == struct.pack('<Q', 0x0218a392cd3d5dbf) else None
            if op == 0x13 and what is None:
                what = tables.get(rd(t, 64)) or tables.get(rd(t, 32))
            want = 'table64' if name.startswith('_ctz64') else 'table32'
            if what not in ('const64', want):
                bad += 1
            seen.append((hex(t), what))
        a += 4
    print(name, seen)
print('bad table references:', bad)
sys.exit(1 if bad else 0)
