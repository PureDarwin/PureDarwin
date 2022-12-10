all-check: all check

check:##
# Copyright (c) 2007 Apple Inc. All rights reserved.
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


### <rdar://problem/5504633> ADOBE: Premiere Pro crashes on quit
### 
### libfoo depends on libfoo1 and libfoo2.  main dlopens(libfoo).
### libfoo1 has an initializer that calls dlopen(libbar).
### libbar depends on libfoo2
###

all-check: all check

check:
	./main 

all: main 

main : main.cxx  libfoo.dylib 
	${CXX} ${CCXXFLAGS} -I${TESTROOT}/include -o main main.cxx


libfoo.dylib : foo.c libfoo1.dylib libfoo2.dylib
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib libfoo1.dylib libfoo2.dylib

libfoo1.dylib : foo1.c libbar.dylib 
	${CC} ${CCFLAGS} -dynamiclib foo1.c -o libfoo1.dylib

libfoo2.dylib : foo2.c
	${CC} ${CCFLAGS} -dynamiclib foo2.c -o libfoo2.dylib

libbar.dylib : bar.c libfoo2.dylib
	${CC} ${CCFLAGS} -dynamiclib bar.c -o libbar.dylib libfoo2.dylib


clean:
	${RM} ${RMFLAGS} *~ main libbar.dylib libfoo.dylib libfoo1.dylib libfoo2.dylib

