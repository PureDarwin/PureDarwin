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

PWD = `pwd`

all-check: all check

check:
	./main

all: main 

main : main.c foo.bundle bar.bundle libfoo.dylib libbar.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c

foo.bundle : foo.c libbase.dylib
	${CC} ${CCFLAGS} -bundle foo.c libbase.dylib -o foo.bundle

bar.bundle : bar.c libbase.dylib 
	${CC} ${CCFLAGS} -dynamiclib bar.c libbase.dylib -o bar.bundle

libfoo.dylib : foo.c libbase.dylib
	${CC} ${CCFLAGS} -dynamiclib foo.c libbase.dylib  -o libfoo.dylib

libbar.dylib : bar.c libbase.dylib 
	${CC} ${CCFLAGS} -dynamiclib bar.c libbase.dylib -o libbar.dylib -sub_library libbase

libbase.dylib : base.c
	${CC} ${CCFLAGS} -dynamiclib base.c -o libbase.dylib -install_name ${PWD}/libbase.dylib

clean:
	${RM} ${RMFLAGS} *~ main foo.bundle bar.bundle libfoo.dylib libbar.dylib libbase.dylib
	
	

