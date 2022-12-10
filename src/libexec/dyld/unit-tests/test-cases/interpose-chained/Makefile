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
#

#
# This unit test verifies that multiple interposing libraries can all
# interpose the same function and the result is that they chain together.
# That is, each one calls through to the next.  
#
# On Tiger (10.4.0), this test fails with infinite recursion.  
#
# The function foo() does string appends.  This allows us to check:
# 1) every interposer was called, and 2) they were called in the
# correct order.
#

TESTROOT = ../..
include ${TESTROOT}/include/common.makefile

all-check: all check

check:
	export DYLD_INSERT_LIBRARIES="libfoo1.dylib:libfoo2.dylib:libfoo3.dylib" && ./main

all: main libfoo1.dylib libfoo2.dylib libfoo3.dylib

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c libfoo.dylib -o main 

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib

libfoo1.dylib : foo1.c libfoo.dylib
	${CC} ${CCFLAGS} -dynamiclib foo1.c libfoo.dylib -o libfoo1.dylib

libfoo2.dylib : foo2.c libfoo.dylib
	${CC} ${CCFLAGS} -dynamiclib foo2.c libfoo.dylib -o libfoo2.dylib

libfoo3.dylib : foo3.c libfoo.dylib
	${CC} ${CCFLAGS} -dynamiclib foo3.c libfoo.dylib -o libfoo3.dylib



clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib libfoo1.dylib libfoo2.dylib libfoo3.dylib

