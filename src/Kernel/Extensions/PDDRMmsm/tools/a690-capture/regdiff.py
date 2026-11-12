#!/usr/bin/env python3
# regdiff.py - compares a690-capture output from linux with a pdmsmregs=1 PureDarwin boot log
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MPL-2.0 OR BSD-2-Clause OR MIT
#
# usage: regdiff.py linux.txt pd-serial.log
# either file may be a capture or a boot log, only "<window> 0x<offset> <value> <name>" lines count
import re
import sys

LINE = re.compile(r"(?:pd msm: reg )?(\w+) 0x([0-9a-f]+) (0x[0-9a-f]+|unread|skipped) (\S+)")


def load(path):
    regs = {}
    with open(path, errors="replace") as f:
        for line in f:
            m = LINE.search(line)
            if m:
                regs[(m.group(1), int(m.group(2), 16))] = (m.group(3), m.group(4))
    return regs


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: regdiff.py linux.txt pd-serial.log")
    a, b = load(sys.argv[1]), load(sys.argv[2])
    if not a or not b:
        sys.exit("no register lines in %s" % (sys.argv[1] if not a else sys.argv[2]))
    same = differ = unread = 0
    for key in sorted(set(a) | set(b)):
        va, name = a.get(key, ("missing", ""))
        vb, nameb = b.get(key, ("missing", ""))
        name = name or nameb
        if not va.startswith("0x") or not vb.startswith("0x"):
            unread += 1
            continue
        if va == vb:
            same += 1
            continue
        differ += 1
        print("%-9s 0x%05x %-36s %s  %s" % (key[0], key[1], name, va, vb))
    print("%d same, %d differ, %d not read on one side" % (same, differ, unread))
    sys.exit(1 if differ else 0)


main()
