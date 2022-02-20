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


all-check: all check


all:  main 

main : main.c libfoo.dylib libbar.dylib
	${CC} -I${TESTROOT}/include main.c libfoo.dylib libbar.dylib -o main 

libfoo.dylib : foo.c
	${CC} -I${TESTROOT}/include  foo.c -DFOO=1 -dynamiclib -install_name ${PWD}/libfoo.dylib -o libfoo.dylib

libbar.dylib : bar.c other/libfoo.dylib
	${CC} -I${TESTROOT}/include  bar.c -dynamiclib other/libfoo.dylib -install_name ${PWD}/libbar.dylib -o libbar.dylib
	
other/libfoo.dylib : foo.c
	mkdir -p ${PWD}/other
	${CC} -I${TESTROOT}/include  foo.c -DFOO=2 -dynamiclib  -install_name ${PWD}/other/libfoo.dylib -o other/libfoo.dylib



check: 
	export DYLD_FALLBACK_LIBRARY_PATH=${PWD} && ./main


clean:
	rm -rf main libfoo.dylib libbar.dylib other