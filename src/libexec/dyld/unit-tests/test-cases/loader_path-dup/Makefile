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

PWD = $(shell pwd)

### 
### This test case is to verify that two different dylibs with the same name  
### and each loaded via the same @loader_path does not confuse dyld
### into just loading one of them.
### 

## Note, until ld understands @loader_path we have to do a funky make


all-check: all check

check:
	./main

all: main

foo/libfoo.dylib : foo.c foo/libbase.dylib
	mkdir -p foo
	${CC} foo.c foo/libbase.dylib -dynamiclib -o "${PWD}/foo/libfoo.dylib" 
	
foo/libbase.dylib : base.c 
	mkdir -p foo
	${CC} base.c -DFOO -dynamiclib -o foo/libbase.dylib  
	

bar/libbar.dylib : bar.c bar/libbase.dylib
	mkdir -p bar
	${CC} bar.c bar/libbase.dylib -dynamiclib -o "${PWD}/bar/libbar.dylib" 
	
bar/libbase.dylib : base.c 
	mkdir -p bar
	${CC} base.c -Dbar -dynamiclib -o bar/libbase.dylib  
	

main : main.c foo/libfoo.dylib bar/libbar.dylib
	${CC} -I${TESTROOT}/include main.c -o main foo/libfoo.dylib bar/libbar.dylib 
	# this breaks partial makes, but ld can't see @loader_path or it freaks
	${INSTALL_NAME_TOOL} -change foo/libbase.dylib  '@loader_path/libbase.dylib'      foo/libfoo.dylib
	${INSTALL_NAME_TOOL} -change bar/libbase.dylib  '@loader_path/libbase.dylib'      bar/libbar.dylib


clean:
	${RM} ${RMFLAGS} *~  main foo bar
