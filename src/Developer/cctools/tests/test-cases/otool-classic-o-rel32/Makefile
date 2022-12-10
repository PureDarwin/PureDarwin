# PLATFORM: WATCHOS
#
# This test uses fixed binaries because ld64 infers this functionality based
# on platform versions, rather than based on some manner of flag.

PLATFORM = WATCHOS
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

.PHONY: all clean hi-all hi-clean-objects hi-clean

all:
	${OTOOLC} -o  hi-abs-64.dylib | ${CHECK} -p ABS-TERSE-64
	${OTOOLC} -ov hi-abs-64.dylib | ${CHECK} -p ABS-VERBOSE-64
	${OTOOLC} -o  hi-rel-64.dylib | ${CHECK} -p REL-TERSE-64
	${OTOOLC} -ov hi-rel-64.dylib | ${CHECK} -p REL-VERBOSE-64
	${OTOOLC} -o  hi-abs-32.dylib | ${CHECK} -p ABS-TERSE-32
	${OTOOLC} -ov hi-abs-32.dylib | ${CHECK} -p ABS-VERBOSE-32
	${OTOOLC} -o  hi-rel-32.dylib | ${CHECK} -p REL-TERSE-32
	${OTOOLC} -ov hi-rel-32.dylib | ${CHECK} -p REL-VERBOSE-32
	${OTOOLC} -o  hi-dir-64.dylib | ${CHECK} -p DIR-TERSE-64
	${OTOOLC} -ov hi-dir-64.dylib | ${CHECK} -p DIR-VERBOSE-64
	${OTOOLC} -o  hi-dir-32.dylib | ${CHECK} -p DIR-TERSE-32
	${OTOOLC} -ov hi-dir-32.dylib | ${CHECK} -p DIR-VERBOSE-32
	echo PASS

# ABS-TERSE-64: hi-abs-64.dylib:
# ABS-TERSE-64-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# ABS-TERSE-64-NEXT: 0000000000008008 0xc128
# ABS-TERSE-64-NEXT:     isa        0xc100
# ABS-TERSE-64-NEXT:     superclass 0x0
# ABS-TERSE-64-NEXT:     cache      0x0
# ABS-TERSE-64-NEXT:     vtable     0x0
# ABS-TERSE-64-NEXT:     data       0xc0b0
# ABS-TERSE-64-NEXT:         flags          0x0
# ABS-TERSE-64-NEXT:         instanceStart  8
# ABS-TERSE-64-NEXT:         instanceSize   12
# ABS-TERSE-64-NEXT:         reserved       0x0
# ABS-TERSE-64-NEXT:         ivarLayout     0x0
# ABS-TERSE-64-NEXT:         name           0x7f89
# ABS-TERSE-64-NEXT:         baseMethods    0xc050
# ABS-TERSE-64-NEXT:             entsize 24
# ABS-TERSE-64-NEXT:             count   2
# ABS-TERSE-64-NEXT:             name    0x7f8c
# ABS-TERSE-64-NEXT:             types   0x7fa5
# ABS-TERSE-64-NEXT:             imp     0x7f04
# ABS-TERSE-64-NEXT:             name    0x7f96
# ABS-TERSE-64-NEXT:             types   0x7fa5
# ABS-TERSE-64-NEXT:             imp     0x7f30
# ABS-TERSE-64-NEXT:         baseProtocols  0x0
# ABS-TERSE-64-NEXT:         ivars          0xc088
# ABS-TERSE-64-NEXT:             entsize   32
# ABS-TERSE-64-NEXT:             count     1
# ABS-TERSE-64-NEXT:             offset    0xc0f8
# ABS-TERSE-64-NEXT:             name      0x7f9d
# ABS-TERSE-64-NEXT:             type      0x7fad
# ABS-TERSE-64-NEXT:             alignment 2
# ABS-TERSE-64-NEXT:             size      4
# ABS-TERSE-64-NEXT:         weakIvarLayout 0x0
# ABS-TERSE-64-NEXT:         baseProperties 0x0
# ABS-TERSE-64-NEXT: Meta Class
# ABS-TERSE-64-NEXT:     isa        0x0
# ABS-TERSE-64-NEXT:     superclass 0x0
# ABS-TERSE-64-NEXT:     cache      0x0
# ABS-TERSE-64-NEXT:     vtable     0x0
# ABS-TERSE-64-NEXT:     data       0xc008
# ABS-TERSE-64-NEXT:         flags          0x1
# ABS-TERSE-64-NEXT:         instanceStart  40
# ABS-TERSE-64-NEXT:         instanceSize   40
# ABS-TERSE-64-NEXT:         reserved       0x0
# ABS-TERSE-64-NEXT:         ivarLayout     0x0
# ABS-TERSE-64-NEXT:         name           0x7f89
# ABS-TERSE-64-NEXT:         baseMethods    0x0
# ABS-TERSE-64-NEXT:         baseProtocols  0x0
# ABS-TERSE-64-NEXT:         ivars          0x0
# ABS-TERSE-64-NEXT:         weakIvarLayout 0x0
# ABS-TERSE-64-NEXT:         baseProperties 0x0
# ABS-TERSE-64-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# ABS-TERSE-64-NEXT:     version 0
# ABS-TERSE-64-NEXT:     flags   0x40

# ABS-VERBOSE-64: hi-abs-64.dylib:
# ABS-VERBOSE-64-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# ABS-VERBOSE-64-NEXT: 0000000000008008 0xc128 _OBJC_CLASS_$_Hi
# ABS-VERBOSE-64-NEXT:     isa        0xc100 _OBJC_METACLASS_$_Hi
# ABS-VERBOSE-64-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# ABS-VERBOSE-64-NEXT:     cache      0x0 __objc_empty_cache
# ABS-VERBOSE-64-NEXT:     vtable     0x0
# ABS-VERBOSE-64-NEXT:     data       0xc0b0 __OBJC_CLASS_RO_$_Hi
# ABS-VERBOSE-64-NEXT:         flags          0x0
# ABS-VERBOSE-64-NEXT:         instanceStart  8
# ABS-VERBOSE-64-NEXT:         instanceSize   12
# ABS-VERBOSE-64-NEXT:         reserved       0x0
# ABS-VERBOSE-64-NEXT:         ivarLayout     0x0
# ABS-VERBOSE-64-NEXT:         name           0x7f89 Hi
# ABS-VERBOSE-64-NEXT:         baseMethods    0xc050 __OBJC_$_INSTANCE_METHODS_Hi
# ABS-VERBOSE-64-NEXT:             entsize 24
# ABS-VERBOSE-64-NEXT:             count   2
# ABS-VERBOSE-64-NEXT:             name    0x7f8c greetings
# ABS-VERBOSE-64-NEXT:             types   0x7fa5 v16@0:8
# ABS-VERBOSE-64-NEXT:             imp     0x7f04 -[Hi greetings]
# ABS-VERBOSE-64-NEXT:             name    0x7f96 unused
# ABS-VERBOSE-64-NEXT:             types   0x7fa5 v16@0:8
# ABS-VERBOSE-64-NEXT:             imp     0x7f30 -[Hi unused]
# ABS-VERBOSE-64-NEXT:         baseProtocols  0x0
# ABS-VERBOSE-64-NEXT:         ivars          0xc088 __OBJC_$_INSTANCE_VARIABLES_Hi
# ABS-VERBOSE-64-NEXT:             entsize   32
# ABS-VERBOSE-64-NEXT:             count     1
# ABS-VERBOSE-64-NEXT:             offset    0xc0f8 _OBJC_IVAR_$_Hi.payload 8
# ABS-VERBOSE-64-NEXT:             name      0x7f9d payload
# ABS-VERBOSE-64-NEXT:             type      0x7fad i
# ABS-VERBOSE-64-NEXT:             alignment 2
# ABS-VERBOSE-64-NEXT:             size      4
# ABS-VERBOSE-64-NEXT:         weakIvarLayout 0x0
# ABS-VERBOSE-64-NEXT:         baseProperties 0x0
# ABS-VERBOSE-64-NEXT: Meta Class
# ABS-VERBOSE-64-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# ABS-VERBOSE-64-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# ABS-VERBOSE-64-NEXT:     cache      0x0 __objc_empty_cache
# ABS-VERBOSE-64-NEXT:     vtable     0x0
# ABS-VERBOSE-64-NEXT:     data       0xc008 __OBJC_METACLASS_RO_$_Hi
# ABS-VERBOSE-64-NEXT:         flags          0x1 RO_META
# ABS-VERBOSE-64-NEXT:         instanceStart  40
# ABS-VERBOSE-64-NEXT:         instanceSize   40
# ABS-VERBOSE-64-NEXT:         reserved       0x0
# ABS-VERBOSE-64-NEXT:         ivarLayout     0x0
# ABS-VERBOSE-64-NEXT:         name           0x7f89 Hi
# ABS-VERBOSE-64-NEXT:         baseMethods    0x0
# ABS-VERBOSE-64-NEXT:         baseProtocols  0x0
# ABS-VERBOSE-64-NEXT:         ivars          0x0
# ABS-VERBOSE-64-NEXT:         weakIvarLayout 0x0
# ABS-VERBOSE-64-NEXT:         baseProperties 0x0
# ABS-VERBOSE-64-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# ABS-VERBOSE-64-NEXT:     version 0
# ABS-VERBOSE-64-NEXT:     flags   0x40

# REL-TERSE-64: hi-rel-64.dylib:
# REL-TERSE-64-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# REL-TERSE-64-NEXT: 0000000000008008 0xc100
# REL-TERSE-64-NEXT:     isa        0xc0d8
# REL-TERSE-64-NEXT:     superclass 0x0
# REL-TERSE-64-NEXT:     cache      0x0
# REL-TERSE-64-NEXT:     vtable     0x0
# REL-TERSE-64-NEXT:     data       0xc078
# REL-TERSE-64-NEXT:         flags          0x0
# REL-TERSE-64-NEXT:         instanceStart  8
# REL-TERSE-64-NEXT:         instanceSize   12
# REL-TERSE-64-NEXT:         reserved       0x0
# REL-TERSE-64-NEXT:         ivarLayout     0x0
# REL-TERSE-64-NEXT:         name           0x7f85
# REL-TERSE-64-NEXT:         baseMethods    0x7f50
# REL-TERSE-64-NEXT:             entsize 12 (relative)
# REL-TERSE-64-NEXT:             count   2
# REL-TERSE-64-NEXT:             name    0x4168 (0xc0c0)
# REL-TERSE-64-NEXT:             types   0x45 (0x7fa1)
# REL-TERSE-64-NEXT:             imp     0xffffff80 (0x7ee0)
# REL-TERSE-64-NEXT:             name    0x4164 (0xc0c8)
# REL-TERSE-64-NEXT:             types   0x39 (0x7fa1)
# REL-TERSE-64-NEXT:             imp     0xffffffa0 (0x7f0c)
# REL-TERSE-64-NEXT:         baseProtocols  0x0
# REL-TERSE-64-NEXT:         ivars          0xc050
# REL-TERSE-64-NEXT:             entsize   32
# REL-TERSE-64-NEXT:             count     1
# REL-TERSE-64-NEXT:             offset    0xc0d0
# REL-TERSE-64-NEXT:             name      0x7f99
# REL-TERSE-64-NEXT:             type      0x7fa9
# REL-TERSE-64-NEXT:             alignment 2
# REL-TERSE-64-NEXT:             size      4
# REL-TERSE-64-NEXT:         weakIvarLayout 0x0
# REL-TERSE-64-NEXT:         baseProperties 0x0
# REL-TERSE-64-NEXT: Meta Class
# REL-TERSE-64-NEXT:     isa        0x0
# REL-TERSE-64-NEXT:     superclass 0x0
# REL-TERSE-64-NEXT:     cache      0x0
# REL-TERSE-64-NEXT:     vtable     0x0
# REL-TERSE-64-NEXT:     data       0xc008
# REL-TERSE-64-NEXT:         flags          0x1
# REL-TERSE-64-NEXT:         instanceStart  40
# REL-TERSE-64-NEXT:         instanceSize   40
# REL-TERSE-64-NEXT:         reserved       0x0
# REL-TERSE-64-NEXT:         ivarLayout     0x0
# REL-TERSE-64-NEXT:         name           0x7f85
# REL-TERSE-64-NEXT:         baseMethods    0x0
# REL-TERSE-64-NEXT:         baseProtocols  0x0
# REL-TERSE-64-NEXT:         ivars          0x0
# REL-TERSE-64-NEXT:         weakIvarLayout 0x0
# REL-TERSE-64-NEXT:         baseProperties 0x0
# REL-TERSE-64-NEXT: Contents of (__DATA,__objc_selrefs) section
# REL-TERSE-64-NEXT:     0x7f88
# REL-TERSE-64-NEXT:     0x7f92
# REL-TERSE-64-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# REL-TERSE-64-NEXT:     version 0
# REL-TERSE-64-NEXT:     flags   0x40

# REL-VERBOSE-64: hi-rel-64.dylib:
# REL-VERBOSE-64-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# REL-VERBOSE-64-NEXT: 0000000000008008 0xc100 _OBJC_CLASS_$_Hi
# REL-VERBOSE-64-NEXT:     isa        0xc0d8 _OBJC_METACLASS_$_Hi
# REL-VERBOSE-64-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# REL-VERBOSE-64-NEXT:     cache      0x0 __objc_empty_cache
# REL-VERBOSE-64-NEXT:     vtable     0x0
# REL-VERBOSE-64-NEXT:     data       0xc078 __OBJC_CLASS_RO_$_Hi
# REL-VERBOSE-64-NEXT:         flags          0x0
# REL-VERBOSE-64-NEXT:         instanceStart  8
# REL-VERBOSE-64-NEXT:         instanceSize   12
# REL-VERBOSE-64-NEXT:         reserved       0x0
# REL-VERBOSE-64-NEXT:         ivarLayout     0x0
# REL-VERBOSE-64-NEXT:         name           0x7f85 Hi
# REL-VERBOSE-64-NEXT:         baseMethods    0x7f50 __OBJC_$_INSTANCE_METHODS_Hi
# REL-VERBOSE-64-NEXT:             entsize 12 (relative)
# REL-VERBOSE-64-NEXT:             count   2
# REL-VERBOSE-64-NEXT:             name    0x4168 (0xc0c0) greetings
# REL-VERBOSE-64-NEXT:             types   0x45 (0x7fa1) v16@0:8
# REL-VERBOSE-64-NEXT:             imp     0xffffff80 (0x7ee0) -[Hi greetings]
# REL-VERBOSE-64-NEXT:             name    0x4164 (0xc0c8) unused
# REL-VERBOSE-64-NEXT:             types   0x39 (0x7fa1) v16@0:8
# REL-VERBOSE-64-NEXT:             imp     0xffffffa0 (0x7f0c) -[Hi unused]
# REL-VERBOSE-64-NEXT:         baseProtocols  0x0
# REL-VERBOSE-64-NEXT:         ivars          0xc050 __OBJC_$_INSTANCE_VARIABLES_Hi
# REL-VERBOSE-64-NEXT:             entsize   32
# REL-VERBOSE-64-NEXT:             count     1
# REL-VERBOSE-64-NEXT:             offset    0xc0d0 _OBJC_IVAR_$_Hi.payload 8
# REL-VERBOSE-64-NEXT:             name      0x7f99 payload
# REL-VERBOSE-64-NEXT:             type      0x7fa9 i
# REL-VERBOSE-64-NEXT:             alignment 2
# REL-VERBOSE-64-NEXT:             size      4
# REL-VERBOSE-64-NEXT:         weakIvarLayout 0x0
# REL-VERBOSE-64-NEXT:         baseProperties 0x0
# REL-VERBOSE-64-NEXT: Meta Class
# REL-VERBOSE-64-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# REL-VERBOSE-64-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# REL-VERBOSE-64-NEXT:     cache      0x0 __objc_empty_cache
# REL-VERBOSE-64-NEXT:     vtable     0x0
# REL-VERBOSE-64-NEXT:     data       0xc008 __OBJC_METACLASS_RO_$_Hi
# REL-VERBOSE-64-NEXT:         flags          0x1 RO_META
# REL-VERBOSE-64-NEXT:         instanceStart  40
# REL-VERBOSE-64-NEXT:         instanceSize   40
# REL-VERBOSE-64-NEXT:         reserved       0x0
# REL-VERBOSE-64-NEXT:         ivarLayout     0x0
# REL-VERBOSE-64-NEXT:         name           0x7f85 Hi
# REL-VERBOSE-64-NEXT:         baseMethods    0x0
# REL-VERBOSE-64-NEXT:         baseProtocols  0x0
# REL-VERBOSE-64-NEXT:         ivars          0x0
# REL-VERBOSE-64-NEXT:         weakIvarLayout 0x0
# REL-VERBOSE-64-NEXT:         baseProperties 0x0
# REL-VERBOSE-64-NEXT: Contents of (__DATA,__objc_selrefs) section
# REL-VERBOSE-64-NEXT:     0x7f88 greetings
# REL-VERBOSE-64-NEXT:     0x7f92 unused
# REL-VERBOSE-64-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# REL-VERBOSE-64-NEXT:     version 0
# REL-VERBOSE-64-NEXT:     flags   0x40

# ABS-TERSE-32: hi-abs-32.dylib:
# ABS-TERSE-32-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# ABS-TERSE-32-NEXT: 00008004 0xc0a8
# ABS-TERSE-32-NEXT:     isa        0xc094
# ABS-TERSE-32-NEXT:     superclass 0x0
# ABS-TERSE-32-NEXT:     cache      0x0
# ABS-TERSE-32-NEXT:     vtable     0x0
# ABS-TERSE-32-NEXT:     data       0xc068
# ABS-TERSE-32-NEXT:         flags          0x0
# ABS-TERSE-32-NEXT:         instanceStart  4
# ABS-TERSE-32-NEXT:         instanceSize   8
# ABS-TERSE-32-NEXT:         ivarLayout     0x0
# ABS-TERSE-32-NEXT:         name           0x7f89
# ABS-TERSE-32-NEXT:         baseMethods    0xc02c
# ABS-TERSE-32-NEXT:             entsize 12
# ABS-TERSE-32-NEXT:             count   2
# ABS-TERSE-32-NEXT:             name    0x7f8c
# ABS-TERSE-32-NEXT:             types   0x7fa5
# ABS-TERSE-32-NEXT:             imp     0x7f04
# ABS-TERSE-32-NEXT:             name    0x7f96
# ABS-TERSE-32-NEXT:             types   0x7fa5
# ABS-TERSE-32-NEXT:             imp     0x7f30
# ABS-TERSE-32-NEXT:         baseProtocols  0x0
# ABS-TERSE-32-NEXT:         ivars          0xc04c
# ABS-TERSE-32-NEXT:             entsize   20
# ABS-TERSE-32-NEXT:             count     1
# ABS-TERSE-32-NEXT:             offset    0xc090
# ABS-TERSE-32-NEXT:             name      0x7f9d
# ABS-TERSE-32-NEXT:             type      0x7fac
# ABS-TERSE-32-NEXT:             alignment 2
# ABS-TERSE-32-NEXT:             size      4
# ABS-TERSE-32-NEXT:         weakIvarLayout 0x0
# ABS-TERSE-32-NEXT:         baseProperties 0x0
# ABS-TERSE-32-NEXT: Meta Class
# ABS-TERSE-32-NEXT:     isa        0x0
# ABS-TERSE-32-NEXT:     superclass 0x0
# ABS-TERSE-32-NEXT:     cache      0x0
# ABS-TERSE-32-NEXT:     vtable     0x0
# ABS-TERSE-32-NEXT:     data       0xc004
# ABS-TERSE-32-NEXT:         flags          0x1
# ABS-TERSE-32-NEXT:         instanceStart  20
# ABS-TERSE-32-NEXT:         instanceSize   20
# ABS-TERSE-32-NEXT:         ivarLayout     0x0
# ABS-TERSE-32-NEXT:         name           0x7f89
# ABS-TERSE-32-NEXT:         baseMethods    0x0
# ABS-TERSE-32-NEXT:         baseProtocols  0x0
# ABS-TERSE-32-NEXT:         ivars          0x0
# ABS-TERSE-32-NEXT:         weakIvarLayout 0x0
# ABS-TERSE-32-NEXT:         baseProperties 0x0
# ABS-TERSE-32-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# ABS-TERSE-32-NEXT:     version 0
# ABS-TERSE-32-NEXT:     flags   0x40

# ABS-VERBOSE-32: hi-abs-32.dylib:
# ABS-VERBOSE-32-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# ABS-VERBOSE-32-NEXT: 00008004 0xc0a8 _OBJC_CLASS_$_Hi
# ABS-VERBOSE-32-NEXT:     isa        0xc094 _OBJC_METACLASS_$_Hi
# ABS-VERBOSE-32-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# ABS-VERBOSE-32-NEXT:     cache      0x0 __objc_empty_cache
# ABS-VERBOSE-32-NEXT:     vtable     0x0
# ABS-VERBOSE-32-NEXT:     data       0xc068 __OBJC_CLASS_RO_$_Hi
# ABS-VERBOSE-32-NEXT:         flags          0x0
# ABS-VERBOSE-32-NEXT:         instanceStart  4
# ABS-VERBOSE-32-NEXT:         instanceSize   8
# ABS-VERBOSE-32-NEXT:         ivarLayout     0x0
# ABS-VERBOSE-32-NEXT:         name           0x7f89 Hi
# ABS-VERBOSE-32-NEXT:         baseMethods    0xc02c __OBJC_$_INSTANCE_METHODS_Hi
# ABS-VERBOSE-32-NEXT:             entsize 12
# ABS-VERBOSE-32-NEXT:             count   2
# ABS-VERBOSE-32-NEXT:             name    0x7f8c greetings
# ABS-VERBOSE-32-NEXT:             types   0x7fa5 v8@0:4
# ABS-VERBOSE-32-NEXT:             imp     0x7f04 -[Hi greetings]
# ABS-VERBOSE-32-NEXT:             name    0x7f96 unused
# ABS-VERBOSE-32-NEXT:             types   0x7fa5 v8@0:4
# ABS-VERBOSE-32-NEXT:             imp     0x7f30 -[Hi unused]
# ABS-VERBOSE-32-NEXT:         baseProtocols  0x0
# ABS-VERBOSE-32-NEXT:         ivars          0xc04c __OBJC_$_INSTANCE_VARIABLES_Hi
# ABS-VERBOSE-32-NEXT:             entsize   20
# ABS-VERBOSE-32-NEXT:             count     1
# ABS-VERBOSE-32-NEXT:             offset    0xc090 _OBJC_IVAR_$_Hi.payload 4
# ABS-VERBOSE-32-NEXT:             name      0x7f9d payload
# ABS-VERBOSE-32-NEXT:             type      0x7fac i
# ABS-VERBOSE-32-NEXT:             alignment 2
# ABS-VERBOSE-32-NEXT:             size      4
# ABS-VERBOSE-32-NEXT:         weakIvarLayout 0x0
# ABS-VERBOSE-32-NEXT:         baseProperties 0x0
# ABS-VERBOSE-32-NEXT: Meta Class
# ABS-VERBOSE-32-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# ABS-VERBOSE-32-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# ABS-VERBOSE-32-NEXT:     cache      0x0 __objc_empty_cache
# ABS-VERBOSE-32-NEXT:     vtable     0x0
# ABS-VERBOSE-32-NEXT:     data       0xc004 __OBJC_METACLASS_RO_$_Hi
# ABS-VERBOSE-32-NEXT:         flags          0x1 RO_META
# ABS-VERBOSE-32-NEXT:         instanceStart  20
# ABS-VERBOSE-32-NEXT:         instanceSize   20
# ABS-VERBOSE-32-NEXT:         ivarLayout     0x0
# ABS-VERBOSE-32-NEXT:         name           0x7f89 Hi
# ABS-VERBOSE-32-NEXT:         baseMethods    0x0
# ABS-VERBOSE-32-NEXT:         baseProtocols  0x0
# ABS-VERBOSE-32-NEXT:         ivars          0x0
# ABS-VERBOSE-32-NEXT:         weakIvarLayout 0x0
# ABS-VERBOSE-32-NEXT:         baseProperties 0x0
# ABS-VERBOSE-32-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# ABS-VERBOSE-32-NEXT:     version 0
# ABS-VERBOSE-32-NEXT:     flags   0x40

# REL-TERSE-32: hi-rel-32.dylib:
# REL-TERSE-32-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# REL-TERSE-32-NEXT: 00008004 0xc090
# REL-TERSE-32-NEXT:     isa        0xc07c
# REL-TERSE-32-NEXT:     superclass 0x0
# REL-TERSE-32-NEXT:     cache      0x0
# REL-TERSE-32-NEXT:     vtable     0x0
# REL-TERSE-32-NEXT:     data       0xc048
# REL-TERSE-32-NEXT:         flags          0x0
# REL-TERSE-32-NEXT:         instanceStart  4
# REL-TERSE-32-NEXT:         instanceSize   8
# REL-TERSE-32-NEXT:         ivarLayout     0x0
# REL-TERSE-32-NEXT:         name           0x7f85
# REL-TERSE-32-NEXT:         baseMethods    0x7f50
# REL-TERSE-32-NEXT:             entsize 12 (relative)
# REL-TERSE-32-NEXT:             count   2
# REL-TERSE-32-NEXT:             name    0x4118 (0xc070)
# REL-TERSE-32-NEXT:             types   0x45 (0x7fa1)
# REL-TERSE-32-NEXT:             imp     0xffffff80 (0x7ee0)
# REL-TERSE-32-NEXT:             name    0x4110 (0xc074)
# REL-TERSE-32-NEXT:             types   0x39 (0x7fa1)
# REL-TERSE-32-NEXT:             imp     0xffffffa0 (0x7f0c)
# REL-TERSE-32-NEXT:         baseProtocols  0x0
# REL-TERSE-32-NEXT:         ivars          0xc02c
# REL-TERSE-32-NEXT:             entsize   20
# REL-TERSE-32-NEXT:             count     1
# REL-TERSE-32-NEXT:             offset    0xc078
# REL-TERSE-32-NEXT:             name      0x7f99
# REL-TERSE-32-NEXT:             type      0x7fa8
# REL-TERSE-32-NEXT:             alignment 2
# REL-TERSE-32-NEXT:             size      4
# REL-TERSE-32-NEXT:         weakIvarLayout 0x0
# REL-TERSE-32-NEXT:         baseProperties 0x0
# REL-TERSE-32-NEXT: Meta Class
# REL-TERSE-32-NEXT:     isa        0x0
# REL-TERSE-32-NEXT:     superclass 0x0
# REL-TERSE-32-NEXT:     cache      0x0
# REL-TERSE-32-NEXT:     vtable     0x0
# REL-TERSE-32-NEXT:     data       0xc004
# REL-TERSE-32-NEXT:         flags          0x1
# REL-TERSE-32-NEXT:         instanceStart  20
# REL-TERSE-32-NEXT:         instanceSize   20
# REL-TERSE-32-NEXT:         ivarLayout     0x0
# REL-TERSE-32-NEXT:         name           0x7f85
# REL-TERSE-32-NEXT:         baseMethods    0x0
# REL-TERSE-32-NEXT:         baseProtocols  0x0
# REL-TERSE-32-NEXT:         ivars          0x0
# REL-TERSE-32-NEXT:         weakIvarLayout 0x0
# REL-TERSE-32-NEXT:         baseProperties 0x0
# REL-TERSE-32-NEXT: Contents of (__DATA,__objc_selrefs) section
# REL-TERSE-32-NEXT:     0x7f88
# REL-TERSE-32-NEXT:     0x7f92
# REL-TERSE-32-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# REL-TERSE-32-NEXT:     version 0
# REL-TERSE-32-NEXT:     flags   0x40

# REL-VERBOSE-32: hi-rel-32.dylib:
# REL-VERBOSE-32-NEXT: Contents of (__DATA_CONST,__objc_classlist) section
# REL-VERBOSE-32-NEXT: 00008004 0xc090 _OBJC_CLASS_$_Hi
# REL-VERBOSE-32-NEXT:     isa        0xc07c _OBJC_METACLASS_$_Hi
# REL-VERBOSE-32-NEXT:     superclass 0x0 _OBJC_CLASS_$_NSObject
# REL-VERBOSE-32-NEXT:     cache      0x0 __objc_empty_cache
# REL-VERBOSE-32-NEXT:     vtable     0x0
# REL-VERBOSE-32-NEXT:     data       0xc048 __OBJC_CLASS_RO_$_Hi
# REL-VERBOSE-32-NEXT:         flags          0x0
# REL-VERBOSE-32-NEXT:         instanceStart  4
# REL-VERBOSE-32-NEXT:         instanceSize   8
# REL-VERBOSE-32-NEXT:         ivarLayout     0x0
# REL-VERBOSE-32-NEXT:         name           0x7f85 Hi
# REL-VERBOSE-32-NEXT:         baseMethods    0x7f50 __OBJC_$_INSTANCE_METHODS_Hi
# REL-VERBOSE-32-NEXT:             entsize 12 (relative)
# REL-VERBOSE-32-NEXT:             count   2
# REL-VERBOSE-32-NEXT:             name    0x4118 (0xc070) greetings
# REL-VERBOSE-32-NEXT:             types   0x45 (0x7fa1) v8@0:4
# REL-VERBOSE-32-NEXT:             imp     0xffffff80 (0x7ee0) -[Hi greetings]
# REL-VERBOSE-32-NEXT:             name    0x4110 (0xc074) unused
# REL-VERBOSE-32-NEXT:             types   0x39 (0x7fa1) v8@0:4
# REL-VERBOSE-32-NEXT:             imp     0xffffffa0 (0x7f0c) -[Hi unused]
# REL-VERBOSE-32-NEXT:         baseProtocols  0x0
# REL-VERBOSE-32-NEXT:         ivars          0xc02c __OBJC_$_INSTANCE_VARIABLES_Hi
# REL-VERBOSE-32-NEXT:             entsize   20
# REL-VERBOSE-32-NEXT:             count     1
# REL-VERBOSE-32-NEXT:             offset    0xc078 _OBJC_IVAR_$_Hi.payload 4
# REL-VERBOSE-32-NEXT:             name      0x7f99 payload
# REL-VERBOSE-32-NEXT:             type      0x7fa8 i
# REL-VERBOSE-32-NEXT:             alignment 2
# REL-VERBOSE-32-NEXT:             size      4
# REL-VERBOSE-32-NEXT:         weakIvarLayout 0x0
# REL-VERBOSE-32-NEXT:         baseProperties 0x0
# REL-VERBOSE-32-NEXT: Meta Class
# REL-VERBOSE-32-NEXT:     isa        0x0 _OBJC_METACLASS_$_NSObject
# REL-VERBOSE-32-NEXT:     superclass 0x0 _OBJC_METACLASS_$_NSObject
# REL-VERBOSE-32-NEXT:     cache      0x0 __objc_empty_cache
# REL-VERBOSE-32-NEXT:     vtable     0x0
# REL-VERBOSE-32-NEXT:     data       0xc004 __OBJC_METACLASS_RO_$_Hi
# REL-VERBOSE-32-NEXT:         flags          0x1 RO_META
# REL-VERBOSE-32-NEXT:         instanceStart  20
# REL-VERBOSE-32-NEXT:         instanceSize   20
# REL-VERBOSE-32-NEXT:         ivarLayout     0x0
# REL-VERBOSE-32-NEXT:         name           0x7f85 Hi
# REL-VERBOSE-32-NEXT:         baseMethods    0x0
# REL-VERBOSE-32-NEXT:         baseProtocols  0x0
# REL-VERBOSE-32-NEXT:         ivars          0x0
# REL-VERBOSE-32-NEXT:         weakIvarLayout 0x0
# REL-VERBOSE-32-NEXT:         baseProperties 0x0
# REL-VERBOSE-32-NEXT: Contents of (__DATA,__objc_selrefs) section
# REL-VERBOSE-32-NEXT:     0x7f88 greetings
# REL-VERBOSE-32-NEXT:     0x7f92 unused
# REL-VERBOSE-32-NEXT: Contents of (__DATA_CONST,__objc_imageinfo) section
# REL-VERBOSE-32-NEXT:     version 0
# REL-VERBOSE-32-NEXT:     flags   0x40

# DIR-TERSE-64: hi-dir-64.dylib:
# DIR-TERSE-64:         baseMethods    0x7f50
# DIR-TERSE-64-NEXT:             entsize 12 (relative, direct SEL)
# DIR-TERSE-64-NEXT:             count   2
# DIR-TERSE-64-NEXT:             name    0x30 (0x7f88)
# DIR-TERSE-64-NEXT:             types   0x45 (0x7fa1)
# DIR-TERSE-64-NEXT:             imp     0xffffff80 (0x7ee0)
# DIR-TERSE-64-NEXT:             name    0x2e (0x7f92)
# DIR-TERSE-64-NEXT:             types   0x39 (0x7fa1)
# DIR-TERSE-64-NEXT:             imp     0xffffffa0 (0x7f0c)
# DIR-TERSE-64-NEXT:         baseProtocols  0x0

# DIR-VERBOSE-64: hi-dir-64.dylib:
# DIR-VERBOSE-64:         baseMethods    0x7f50 __OBJC_$_INSTANCE_METHODS_Hi
# DIR-VERBOSE-64-NEXT:             entsize 12 (relative, direct SEL)
# DIR-VERBOSE-64-NEXT:             count   2
# DIR-VERBOSE-64-NEXT:             name    0x30 (0x7f88) greetings
# DIR-VERBOSE-64-NEXT:             types   0x45 (0x7fa1) v16@0:8
# DIR-VERBOSE-64-NEXT:             imp     0xffffff80 (0x7ee0) -[Hi greetings]
# DIR-VERBOSE-64-NEXT:             name    0x2e (0x7f92) unused
# DIR-VERBOSE-64-NEXT:             types   0x39 (0x7fa1) v16@0:8
# DIR-VERBOSE-64-NEXT:             imp     0xffffffa0 (0x7f0c) -[Hi unused]
# DIR-VERBOSE-64-NEXT:         baseProtocols  0x0

# DIR-TERSE-32: hi-dir-32.dylib:
# DIR-TERSE-32:         baseMethods    0x7f50
# DIR-TERSE-32-NEXT:             entsize 12 (relative, direct SEL)
# DIR-TERSE-32-NEXT:             count   2
# DIR-TERSE-32-NEXT:             name    0x30 (0x7f88)
# DIR-TERSE-32-NEXT:             types   0x45 (0x7fa1)
# DIR-TERSE-32-NEXT:             imp     0xffffff80 (0x7ee0)
# DIR-TERSE-32-NEXT:             name    0x2e (0x7f92)
# DIR-TERSE-32-NEXT:             types   0x39 (0x7fa1)
# DIR-TERSE-32-NEXT:             imp     0xffffffa0 (0x7f0c)
# DIR-TERSE-32-NEXT:         baseProtocols  0x0

# DIR-VERBOSE-32: hi-dir-32.dylib:
# DIR-VERBOSE-32:         baseMethods    0x7f50 __OBJC_$_INSTANCE_METHODS_Hi
# DIR-VERBOSE-32-NEXT:             entsize 12 (relative, direct SEL)
# DIR-VERBOSE-32-NEXT:             count   2
# DIR-VERBOSE-32-NEXT:             name    0x30 (0x7f88) greetings
# DIR-VERBOSE-32-NEXT:             types   0x45 (0x7fa1) v8@0:4
# DIR-VERBOSE-32-NEXT:             imp     0xffffff80 (0x7ee0) -[Hi greetings]
# DIR-VERBOSE-32-NEXT:             name    0x2e (0x7f92) unused
# DIR-VERBOSE-32-NEXT:             types   0x39 (0x7fa1) v8@0:4
# DIR-VERBOSE-32-NEXT:             imp     0xffffffa0 (0x7f0c) -[Hi unused]
# DIR-VERBOSE-32-NEXT:         baseProtocols  0x0

clean:
	true

# hi: main.o hi.dylib
# 	ld -o hi main.o hi.dylib -platform_version macos 10.16 10.16	     \
# 	-framework Foundation -framework System

hi-all: hi-rel-64.dylib hi-abs-64.dylib hi-rel-32.dylib hi-abs-32.dylib

hi-64.o:
	$(CC) -arch arm64 -c -o hi-64.o hi.m

hi-32.o:
	$(CC) -arch arm64_32 -c -o hi-32.o hi.m

hi-rel-64.dylib: hi-64.o
	$(LD) -arch arm64 -dylib -o $@ hi-64.o				     \
	-platform_version watchOS 7.0 7.0				     \
	  -framework Foundation -framework System

hi-abs-64.dylib: hi-64.o
	$(LD) -arch arm64 -dylib -o $@ hi-64.o				     \
	-platform_version watchOS 6.0 6.0				     \
	  -framework Foundation -framework System

hi-rel-32.dylib: hi-32.o
	$(LD) -arch arm64_32 -dylib -o $@ hi-32.o			     \
	-platform_version watchOS 7.0 7.0				     \
	-framework Foundation -framework System

hi-abs-32.dylib: hi-32.o
	$(LD) -arch arm64_32 -dylib -o $@ hi-32.o			     \
	-platform_version watchOS 6.0 6.0				     \
	-framework Foundation -framework System

hi-clean-objects:
	rm -f hi.o hi-32.o hi-64.o

hi-clean: hi-clean-objects
	rm -f hi-rel-64.dylib hi-abs-64.dylib hi-rel-32.dylib hi-abs-32.dylib
