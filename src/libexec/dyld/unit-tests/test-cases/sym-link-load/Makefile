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

### 
### This test case is to verify that dyld cannot be tricked into loading 
### the "same" library twice. 
### 
### The tricky scenario is:
###  1) a library dyld is to load is specified with a symlink path
###  2) the library the that is the target of the symlink is already loaded 
###  via a different path.  In this case becuase of DYLD_LIBRARY_PATH.
### 
### 
### 
### 

PWD = $(shell pwd)

all-check: all check

check:
	./main
	export DYLD_LIBRARY_PATH="${PWD}/fake" && ./main
	

all: main real/liblink.dylib  real/libtest.dylib fake/libtest.dylib


main: main.c stub/libtest.dylib stub/liblink.dylib real/libbase.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c  stub/libtest.dylib stub/liblink.dylib real/libbase.dylib

stub/libtest.dylib: test.c
	mkdir -p stub
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib test.c -DDO_NOTHING -o stub/libtest.dylib -install_name "${PWD}/real/libtest.dylib"
	
stub/liblink.dylib: link.c
	mkdir -p stub
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib link.c -o stub/liblink.dylib -install_name "${PWD}/real/liblink.dylib"
	

real/libbase.dylib: base.c
	mkdir -p real
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib base.c -o "${PWD}/real/libbase.dylib"
	
real/libtest.dylib: test.c real/libbase.dylib
	mkdir -p real
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib test.c real/libbase.dylib -o "${PWD}/real/libtest.dylib"
	
real/liblink.dylib: link.c
	mkdir -p real
	cd real && ln -s libtest.dylib liblink.dylib
	

fake/libtest.dylib: test.c real/libbase.dylib
	mkdir -p fake
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib test.c real/libbase.dylib -o "${PWD}/fake/libtest.dylib" -install_name "${PWD}/real/libtest.dylib"
	
	


clean:
	${RM} ${RMFLAGS} *~ main real stub fake
	
