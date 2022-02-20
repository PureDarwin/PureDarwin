##
# Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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
# a main executable linked with -rpath that uses @executable_path.
# The @executable_path in the rpath should expand at runtime to the directory
# of the main executable.
#

all-check: all check

check:
	./main

all: main

hide/hole/libfoo.dylib : foo.c
	mkdir -p hide/hole
	${CC} foo.c -dynamiclib -o hide/hole/libfoo.dylib -install_name @rpath/libfoo.dylib
	
libbar.dylib : bar.c hide/hole/libfoo.dylib
	${CC} bar.c -dynamiclib -o libbar.dylib hide/hole/libfoo.dylib

main : main.c libbar.dylib
	${CC} -I${TESTROOT}/include main.c -o main libbar.dylib -Wl,-rpath -Wl,@executable_path/hide/hole

clean:
	${RM} -rf *~  main hide libbar.dylib
