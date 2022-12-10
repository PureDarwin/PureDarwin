##
# Copyright (c) 2013 Apple Computer, Inc. All rights reserved.
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

# foo depends on bar depends on baz
# foo links with baz then bar
# verify that baz terminators run before bar terminators


all-check: all check

check:
	./main libfoo.dylib
	./main libfoo2.dylib

all:
	${CC} ${CCFLAGS}   -dynamiclib base.c -o libbase.dylib
	${CXX} ${CXXFLAGS} -dynamiclib baz.cxx libbase.dylib -o libbaz.dylib
	${CXX} ${CXXFLAGS} -dynamiclib bar.cxx libbaz.dylib libbase.dylib -o libbar.dylib
	${CC} ${CCFLAGS}   -dynamiclib foo.c libbar.dylib libbaz.dylib  -o libfoo.dylib
	${CC} ${CCFLAGS}   -dynamiclib foo.c libbaz.dylib libbar.dylib  -o libfoo2.dylib
	${CC} ${CCFLAGS}   main.c libbase.dylib -I${TESTROOT}/include  -o main


clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib libfoo2.dylib libbaz.dylib libbar.dylib libbase.dylib
	

