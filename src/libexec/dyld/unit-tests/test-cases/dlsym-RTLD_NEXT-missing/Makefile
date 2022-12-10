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

PROG = ./main

ifeq "ppc" "$(ARCH)"
	MACHINE =  $(shell arch)
	ifeq "i386" "$(MACHINE)"
		PROG = /usr/bin/true
	endif
endif



all-check: all check

check:
	${TESTROOT}/bin/exit-zero-pass.pl "dlsym-RTLD_NEXT-missing with circular libraries" "dlsym-RTLD_NEXT-missing with circular libraries" ${PROG}

all:  main

main : main.c foo1.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c foo1.dylib


foo1.dylib : foo1.c foo2.dylib
	${CC} ${CCFLAGS} -dynamiclib foo1.c -o foo1.dylib foo2.dylib

foo2.dylib : foo2.c foo3.dylib
	${CC} ${CCFLAGS} -dynamiclib foo2.c -o foo2.dylib foo3.dylib

foo3.dylib : foo3.c 
	${CC} ${CCFLAGS} -dynamiclib foo1.c -o foo1-temp.dylib -install_name foo1.dylib
	${CC} ${CCFLAGS} -dynamiclib foo3.c -o foo3.dylib foo1-temp.dylib
	rm foo1-temp.dylib
	
clean:
	${RM} ${RMFLAGS} *~ main test.bundle foo1.dylib foo2.dylib foo3.dylib

