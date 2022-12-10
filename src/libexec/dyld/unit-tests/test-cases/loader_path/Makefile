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
	./main

all: main

hide/hole/libfoo.dylib : foo.c
	mkdir -p hide/hole
	${CC} foo.c -dynamiclib -o hide/hole/libfoo.dylib -install_name libfoo.dylib
	
hide/libbar.dylib : bar.c hide/hole/libfoo.dylib
	${CC} bar.c -dynamiclib -o hide/libbar.dylib -install_name libbar.dylib hide/hole/libfoo.dylib
	install_name_tool -change libfoo.dylib '@loader_path/hole/libfoo.dylib'  hide/libbar.dylib

hide/libfoo3.dylib : foo.c
	${CC} foo.c -dynamiclib -o hide/libfoo3.dylib -install_name libfoo3.dylib 

libfoo2.dylib : foo.c
	${CC} foo.c -dynamiclib -o libfoo2.dylib


main : main.c libfoo2.dylib hide/libbar.dylib hide/libfoo3.dylib
	${CC}   -Wno-deprecated-declarations -I${TESTROOT}/include main.c -o main hide/libbar.dylib libfoo2.dylib 
	${INSTALL_NAME_TOOL} -change libfoo2.dylib '@loader_path/libfoo2.dylib'      main
	${INSTALL_NAME_TOOL} -change libbar.dylib  '@loader_path/hide/libbar.dylib'  main

clean:
	${RM} ${RMFLAGS} *~  main libfoo.dylib hide libfoo2.dylib
