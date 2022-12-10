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


#
# a main executable run with DYLD_LIBRARY_PATH will override its rpath
#

all-check: all check

check:
	export DYLD_LIBRARY_PATH=`pwd`/hide1 && ./main

all: main hide2/libfoo.dylib hide1/libfoo.dylib

	
hide1/libfoo.dylib : foo.c
	mkdir -p hide1
	${CC} foo.c -dynamiclib -o hide1/libfoo.dylib -install_name @rpath/libfoo.dylib
	
hide2/libfoo.dylib : foo.c
	mkdir -p hide2
	${CC} foo.c -DBAD -dynamiclib -o hide2/libfoo.dylib -install_name @rpath/libfoo.dylib

main : main.c hide2/libfoo.dylib 
	${CC} -I${TESTROOT}/include main.c -o main hide2/libfoo.dylib -Wl,-rpath -Wl,@loader_path/hide2

clean:
	${RM} ${RMFLAGS} *~  main hide1 hide2
