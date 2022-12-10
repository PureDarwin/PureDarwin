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
	./dynamic-test

all: dynamic-test  static-test

static-test: a1.o a2.o main.o
	$(CXX) -I${TESTROOT}/include a1.o a2.o main.o -o static-test

dynamic-test: main.o a1.o liba2.dylib
	$(CXX) -I${TESTROOT}/include  main.o a1.o -L. -la2 -o dynamic-test

liba2.dylib: a2.o
	$(CXX) -I${TESTROOT}/include a2.o -dynamiclib -o liba2.dylib

a1.o: a1.cc a.h
	$(CXX) -I${TESTROOT}/include -c a1.cc

a2.o: a2.cc a.h
	$(CXX) -I${TESTROOT}/include -c a2.cc

main.o: main.cc
	$(CXX) -I${TESTROOT}/include -c main.cc
	
	
clean:
	${RM} ${RMFLAGS} *.o *.dylib static-test dynamic-test 
