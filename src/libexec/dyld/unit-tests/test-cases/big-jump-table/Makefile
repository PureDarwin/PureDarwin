##
# Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

all-check: all check

check:
	./main

all: main

###
### The point of this test is to check an edge case for i386 architecture
### with "fast stubs".  We want to verify that a fast stub that ends
### near the page boundary for the __IMPORT segment does not cause an
### accidental read beyond the __IMPORT segment
###  rdar://problem/4653725 
###

main: main.c libtest1.dylib libtest2.dylib libtest3.dylib libtest4.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libtest1.dylib libtest2.dylib libtest3.dylib libtest4.dylib

libtest1.dylib: pointers.c funcs.c libfoo.dylib
	${CC} ${CCFLAGS}  -dynamiclib pointers.c funcs.c -DCASE=1 -o libtest1.dylib libfoo.dylib

libtest2.dylib: pointers.c funcs.c libfoo.dylib
	${CC} ${CCFLAGS}  -dynamiclib pointers.c funcs.c -DCASE=2 -o libtest2.dylib libfoo.dylib 

libtest3.dylib: pointers.c funcs.c libfoo.dylib
	${CC} ${CCFLAGS}  -dynamiclib pointers.c funcs.c -DCASE=3 -o libtest3.dylib libfoo.dylib 

libtest4.dylib: pointers.c funcs.c libfoo.dylib
	${CC} ${CCFLAGS}  -dynamiclib pointers.c funcs.c -DCASE=4 -o libtest4.dylib libfoo.dylib 

libfoo.dylib: foo.c 
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib


clean:
	${RM} ${RMFLAGS} *~ main libtest1.dylib libtest2.dylib libtest3.dylib libtest4.dylib libfoo.dylib
	
