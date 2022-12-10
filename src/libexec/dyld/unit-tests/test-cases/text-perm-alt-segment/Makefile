##
# Copyright (c) 2013 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
# 
# This file contains Original Code and/or Modifications of Original Code
# as defined in and that are subject to the Apple Public Source License
# Version 2.0 (the 'License'). You may not use this file except in
# compliance with the License. Please obtain a copy of the License at
# http://www.opensource.apple.com/apsl/ and read it before using this
# file.
# 
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
# Please see the License for the specific language governing rights and
# limitations under the License.
# 
# @APPLE_LICENSE_HEADER_END@
##
TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

### 
### This test case is to verify __TEXT relocations work in dylibs
### 
### 


ifeq "i386" "$(ARCH)"
	EXTRA_DEP = libfoo.dylib
else

endif

all-check: all check

check:
	./main

all:  ${EXTRA_DEP}
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c ${EXTRA_DEP}

libfoo.dylib:
	${CC} ${CCFLAGS} foo.c -c
	${LD} -r -arch ${ARCH} foo.o -rename_section __TEXT __text MYTEXT mytext -o foo2.o
	${CC} ${CCFLAGS} -dynamiclib foo2.o -o libfoo.dylib -Wl,-segprot,MYTEXT,rwx,r-x -read_only_relocs suppress


clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib foo.o foo2.o

