##
# Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

PWD = $(shell pwd)

#
# The main executable supplies an rpath.  libfoo.dylib supplies an 
# rpath.  libfoo.dylib needs libbar.dylib and libbaz.dylib.  One
# is found from the main executable's rpath and one from libfoo.dylib's
# rpath. 
#

all-check: all check

check:
	./main

all: main

hide1/libbar.dylib : bar.c
	mkdir -p hide1
	${CC} bar.c -dynamiclib -o hide1/libbar.dylib  -install_name @rpath/libbar.dylib
	
hide2/libbaz.dylib : baz.c
	mkdir -p hide2
	${CC} baz.c -dynamiclib -o hide2/libbaz.dylib  -install_name @rpath/libbaz.dylib

libfoo.dylib : foo.c hide1/libbar.dylib hide2/libbaz.dylib
	${CC} foo.c -dynamiclib -o libfoo.dylib hide1/libbar.dylib hide2/libbaz.dylib -Wl,-rpath -Wl,${PWD}/hide2

main : main.c libfoo.dylib
	${CC} -I${TESTROOT}/include main.c -o main libfoo.dylib -Wl,-rpath -Wl,${PWD}/hide1

clean:
	${RM} ${RMFLAGS} *~  main hide1 hide2 libfoo.dylib
