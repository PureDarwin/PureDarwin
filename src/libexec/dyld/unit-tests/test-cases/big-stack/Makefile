##
# Copyright (c) 2006-2008 Apple Inc. All rights reserved.
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

# rosetta does not support very large stack sizes
STACK_SIZE = 0x83000000
ifeq "ppc" "$(ARCH)"
	MACHINE =  $(shell arch)
	ifeq "i386" "$(MACHINE)"
		STACK_SIZE = 0x02100000
	endif
endif


ifeq "iPhoneOS" "$(OS_NAME)"
	STACK_SIZE = 0x20000000
endif


all-check: all check

check:
	${TESTROOT}/bin/exit-zero-pass.pl "big stack" "big stack failed" ./main

all:
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c -Wl,-w -Wl,-stack_size -Wl,${STACK_SIZE} -DSTACK_SIZE=${STACK_SIZE}

clean:
	${RM} ${RMFLAGS} main
