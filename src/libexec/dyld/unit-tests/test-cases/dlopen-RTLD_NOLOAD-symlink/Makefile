##
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

PWD = $(shell pwd)

###
### Test that RTLD_NOLOAD finds existing image 
### even when symlinks are used to obscure it
###

all-check: all check

check:
	./main libfoosym.dylib
	./main2 libbar.dylib


all: main main2 libfoosym.dylib

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c libfoo.dylib -o main 
	
libfoosym.dylib : libfoo.dylib
	ln -sf libfoo.dylib libfoosym.dylib

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} -dynamiclib foo.c -o ${PWD}/libfoo.dylib 

main2 : main.c libbarsym.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c libbarsym.dylib -o main2 

libbarsym.dylib : libbar.dylib
	ln -sf libbar.dylib libbarsym.dylib

libbar.dylib : bar.c
	${CC} ${CCFLAGS} -dynamiclib bar.c -o libbar.dylib -install_name ${PWD}/libbarsym.dylib 

clean:
	${RM} ${RMFLAGS} *~ main main2 libfoo.dylib libfoosym.dylib libbar.dylib libbarsym.dylib
	
