#!/usr/bin/env python3
# the built-in account pictures, drawn here so no artwork has to be licensed
# usage: make-pictures.py OUTDIR

import math
import os
import struct
import sys
import zlib

SIZE = 256


def png(path, rows):
    raw = b"".join(b"\0" + bytes(r) for r in rows)

    def chunk(kind, data):
        c = kind + data
        return struct.pack(">I", len(data)) + c + struct.pack(">I", zlib.crc32(c) & 0xffffffff)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def mix(a, b, t):
    t = max(0.0, min(1.0, t))
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def rgb(h):
    return ((h >> 16) & 255, (h >> 8) & 255, h & 255)


# each picture is a background gradient and a shape: f(x, y) gives the colour at a point in 0..1
def sunset(x, y):
    c = mix(rgb(0xf6b26b), rgb(0xd9536f), y)
    if (x - 0.5) ** 2 + (y - 0.62) ** 2 < 0.2 ** 2 and y < 0.7:
        c = mix(rgb(0xffe08a), rgb(0xffb35c), (y - 0.42) / 0.3)
    if y > 0.7:
        c = mix(rgb(0x8c3b5e), rgb(0x5a2448), (y - 0.7) / 0.3)
    return c


def lagoon(x, y):
    c = mix(rgb(0x9fe3e0), rgb(0x2f8fa8), y)
    for k in range(3):
        band = 0.45 + k * 0.17 + 0.035 * math.sin(x * math.pi * 4 + k)
        if y > band:
            c = mix(c, rgb(0x1f5f86), 0.35)
    return c


def dusk(x, y):
    c = mix(rgb(0x6b4fa3), rgb(0x1f2550), y)
    inside = (x - 0.55) ** 2 + (y - 0.42) ** 2 < 0.22 ** 2
    bite = (x - 0.65) ** 2 + (y - 0.36) ** 2 < 0.19 ** 2
    if inside and not bite:
        c = rgb(0xf3ead2)
    return c


def graphite(x, y):
    c = mix(rgb(0x9a9a9e), rgb(0x45454b), y)
    d = math.hypot(x - 0.5, y - 0.5)
    if int(d * 14) % 2 == 0 and d < 0.42:
        c = mix(c, rgb(0xe6e6e8), 0.28)
    return c


def leaf(x, y):
    c = mix(rgb(0xc8e6a0), rgb(0x4e9a5a), y)
    u, v = (x - 0.5) * 0.707 + (y - 0.5) * 0.707, -(x - 0.5) * 0.707 + (y - 0.5) * 0.707
    if (u / 0.34) ** 2 + (v / 0.17) ** 2 < 1:
        c = rgb(0x2f7a45) if abs(v) < 0.008 else mix(rgb(0x5fbf6a), rgb(0x3a8f4f), u + 0.5)
    return c


def peaks(x, y):
    c = mix(rgb(0xbfe0f7), rgb(0x6aa6dc), y)
    if y > 0.78 - 0.9 * max(0, 0.32 - abs(x - 0.35)) * 2:
        c = mix(rgb(0x5d6f8c), rgb(0x3b4760), y)
    if y > 0.85 - 0.9 * max(0, 0.26 - abs(x - 0.72)) * 2:
        c = mix(rgb(0x7a8aa6), rgb(0x4b5873), y)
    if y > 0.78:
        c = mix(rgb(0x5f8a5a), rgb(0x46684f), (y - 0.78) / 0.22)
    return c


def petals(x, y):
    c = mix(rgb(0xfbd3e0), rgb(0xe48aa8), y)
    for k in range(6):
        a = k * math.pi / 3
        if (x - 0.5 - 0.17 * math.cos(a)) ** 2 + (y - 0.5 - 0.17 * math.sin(a)) ** 2 < 0.13 ** 2:
            c = mix(rgb(0xffffff), rgb(0xf7c1d3), 0.3)
    if (x - 0.5) ** 2 + (y - 0.5) ** 2 < 0.08 ** 2:
        c = rgb(0xf2c14e)
    return c


def stars(x, y):
    c = mix(rgb(0x2a2f5e), rgb(0x0f1330), y)
    for k in range(14):
        sx, sy = (k * 0.6180339) % 1, (k * 0.4142135 + 0.13) % 1
        r = 0.012 + 0.01 * (k % 3)
        if (x - sx) ** 2 + (y - sy) ** 2 < r * r:
            c = rgb(0xfff4c8)
    return c


PICTURES = [("Sunset", sunset), ("Lagoon", lagoon), ("Dusk", dusk), ("Graphite", graphite),
            ("Leaf", leaf), ("Peaks", peaks), ("Petals", petals), ("Stars", stars)]


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    for name, f in PICTURES:
        rows = []
        for py in range(SIZE):
            row = []
            for px in range(SIZE):
                # four samples a pixel smooth the shapes' edges
                acc = [0.0, 0.0, 0.0]
                for sx, sy in ((0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)):
                    c = f((px + sx) / SIZE, (py + sy) / SIZE)
                    for i in range(3):
                        acc[i] += c[i]
                row += [int(acc[i] / 4 + 0.5) for i in range(3)]
            rows.append(row)
        png(os.path.join(out, name + ".png"), rows)


main()
