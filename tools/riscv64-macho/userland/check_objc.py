#!/usr/bin/env python3
# the objc dylib must keep __objc_imageinfo, fold its category into the class and point
# every method list entry and selector ref at the right name and implementation
# usage: check_objc.py <dylib>
import struct, sys

d = open(sys.argv[1], 'rb').read()
segs, sects, syms = [], {}, {}
ncmds = struct.unpack_from('<I', d, 16)[0]; off = 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', d, off)
    if cmd == 0x19:
        vm, vs, fo, fs = struct.unpack_from('<QQQQ', d, off + 24)
        segs.append((vm, fo, fs))
        for i in range(struct.unpack_from('<I', d, off + 64)[0]):
            so = off + 72 + 80 * i
            name = d[so:so + 16].split(b'\0')[0].decode()
            addr, sz = struct.unpack_from('<QQ', d, so + 32)
            sects[name] = (addr, sz)
    elif cmd == 0x2:
        symoff, nsyms, stroff, _ = struct.unpack_from('<IIII', d, off + 8)
        for i in range(nsyms):
            strx, ntype, _, _, value = struct.unpack_from('<IBBHQ', d, symoff + 16 * i)
            if (ntype & 0xe) == 0xe:
                syms[value] = d[stroff + strx:d.index(b'\0', stroff + strx)].decode()
    off += size

def rd(a, n):
    for vm, fo, fs in segs:
        if vm <= a < vm + fs:
            return d[fo + a - vm:fo + a - vm + n]
    raise SystemExit('address %#x not in the file' % a)
def ptr(a):
    # a DYLD_CHAINED_PTR_64 rebase, the target is the low 36 bits
    v = struct.unpack('<Q', rd(a, 8))[0]
    assert not v >> 63, 'unexpected bind at %#x' % a
    return v & 0xfffffffff
def cstr(a):
    b = rd(a, 128); return b[:b.index(b'\0')].decode()

def fail(msg):
    print('FAIL:', msg); sys.exit(1)

if sects.get('__objc_imageinfo', (0, 0))[1] != 8:
    fail('__objc_imageinfo missing')
if sects.get('__objc_catlist', (0, 0))[1] != 0:
    fail('category not merged, __objc_catlist still has entries')
cl_addr, cl_size = sects['__objc_classlist']
if cl_size != 8:
    fail('expected one class in __objc_classlist')
cls = ptr(cl_addr)
isa = ptr(cls)

def methods(class_addr):
    ro = ptr(class_addr + 32) & ~7
    lst = ptr(ro + 32)
    flags, count = struct.unpack('<II', rd(lst, 8))
    ent, out = flags & 0xffff, []
    for i in range(count):
        e = lst + 8 + i * ent
        if flags & 0x80000000:
            nameoff, _, impoff = struct.unpack('<iii', rd(e, 12))
            out.append((cstr(ptr(e + nameoff)), e + 8 + impoff))
        else:
            out.append((cstr(ptr(e)), ptr(e + 16)))
    return out

inst, clsm = methods(cls), methods(isa)
print('instance', [(n, syms.get(i)) for n, i in inst])
print('class', [(n, syms.get(i)) for n, i in clsm])
want = {'base': '-[A base]', 'extra': '-[A(Extra) extra]'}
if sorted(n for n, _ in inst) != sorted(want) or any(syms.get(i) != want[n] for n, i in inst):
    fail('instance methods wrong after category merge')
if [(n, syms.get(i)) for n, i in clsm] != [('make', '+[A make]')]:
    fail('class methods wrong')
sr_addr, sr_size = sects['__objc_selrefs']
sels = sorted(cstr(ptr(sr_addr + 8 * i)) for i in range(sr_size // 8))
if sels != ['base', 'extra', 'make']:
    fail('selector refs %s' % sels)
print('objc image checks passed')
