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
### rdar://problem/3684168
###
### The process has two different libfoo.dylib.  
### When DYLD_LIBRARY_PATH is used, both resolve to the same library.
### It gets worse.  One of the libraries re-exports from the other. 
### So, without loop detection in dyld, it will infinitely recurse.
###

PWD = `pwd`

all-check: all check

check:
	# verify it runs as-is
	./main
	# verify dyld doesn't hang on the circularity
	DYLD_LIBRARY_PATH=$(PWD) && export DYLD_LIBRARY_PATH && ${TESTROOT}/bin/exit-zero-pass.pl "lib-name-overload" "lib-name-overload" ./main

all: main 

other/libfoo.dylib : foo2.c
	mkdir -p other
	${CC} foo2.c -dynamiclib  -o $(PWD)/other/libfoo.dylib 

libfoo.dylib : foo.c other/libfoo.dylib
	${CC} foo.c -dynamiclib $(PWD)/other/libfoo.dylib -sub_library libfoo -o $(PWD)/libfoo.dylib 

main : main.c libfoo.dylib
	${CC} main.c -I${TESTROOT}/include -o main libfoo.dylib

	
clean:
	rm -rf main other libfoo.dylib