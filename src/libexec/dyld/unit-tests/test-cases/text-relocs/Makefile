##
# Copyright (c) 2005-2007 Apple Inc. All rights reserved.
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
### This test case is to verify __TEXT relocations work in dylibs
### 
### 


TEXT_RELOC_FLAGS = -mdynamic-no-pic -read_only_relocs suppress -Wl,-w
TEXT_STAT_FLAGS = -static

ifeq "iPhoneOS" "$(OS_NAME)"
	# iOS does not support text relocs
	TEXT_RELOC_FLAGS = 
	TEXT_STAT_FLAGS =
endif 


all-check: all check

check:
	./main

all:  
	${CC} ${CCFLAGS} -dynamiclib bar.c space.s -Os -o libbar.dylib  ${TEXT_RELOC_FLAGS}
	${CC} ${CCFLAGS} bind.c $(TEXT_STAT_FLAGS) -Os -c -o bind.o
	${CC}  -dynamiclib bind.o libbar.dylib -o libbind.dylib ${TEXT_RELOC_FLAGS}
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libbar.dylib libbind.dylib
	
clean:
	${RM} ${RMFLAGS} *~ main libbar.dylib libbind.dylib bind.o

