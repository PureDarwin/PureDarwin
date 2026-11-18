# patch_sendmsg.py - point libobjc2's nil receiver lookups at nil_wasm.c, run in its source tree
# Copyright (c) 2026 The PureDarwin Project, SPDX-License-Identifier: MIT
import re
p = "sendmsg2.c"; s = open(p).read()
decl = "struct objc_slot *__objc_wasm_nil_slot(SEL);\nstruct objc_method *__objc_wasm_nil_method(SEL);\nIMP __objc_wasm_nil_imp(SEL);\n"
s = s.replace("static long long nil_method(", decl + "static long long nil_method(", 1)
head = "\tif (UNLIKELY(*receiver == nil))\n\t{\n"
parts = s.split(head)
assert len(parts) == 4, len(parts)
# the sender lookup hands back a v1 slot, the other two a method
s = parts[0] + head + "\t\treturn __objc_wasm_nil_slot(selector);\n" + parts[1] \
    + head + "\t\treturn (struct objc_slot2*)__objc_wasm_nil_method(selector);\n" + parts[2] \
    + head + "\t\treturn (struct objc_slot2*)__objc_wasm_nil_method(selector);\n" + parts[3]
a = "\tif (nil == receiver) { return (IMP)nil_method; }"
assert s.count(a) == 1
s = s.replace(a, "\tif (nil == receiver) { return __objc_wasm_nil_imp(selector); }")
open(p, "w").write(s)
