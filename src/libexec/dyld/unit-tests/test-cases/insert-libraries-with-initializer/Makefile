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

#
# verifies that initializers for inserted libraries run before the
# main executables initializers
#


all-check: all check

check:
	export DYLD_INSERT_LIBRARIES=libinsert.dylib && ./main

all: main libinsert.dylib


main: main.c libfoo1.dylib libbase.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c  libfoo1.dylib  libbase.dylib


libfoo1.dylib: foo1.c libbase.dylib libfoo2.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib  -o libfoo1.dylib  foo1.c  libbase.dylib libfoo2.dylib

libfoo2.dylib: foo2.c libbase.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib  -o libfoo2.dylib  foo2.c  libbase.dylib

libinsert.dylib: insert.c libbase.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib  -o libinsert.dylib  insert.c  libbase.dylib

libbase.dylib: base.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib  -o libbase.dylib  base.c


clean:
	${RM} ${RMFLAGS} *~ main  lib*.dylib
	
