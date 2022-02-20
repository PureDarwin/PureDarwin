##
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
BASE_ADDRESS = 0x90030000
ifeq "x86_64" "$(ARCH)"
	BASE_ADDRESS = 0x7FFF80100000
endif


ifeq "iPhoneOS" "$(OS_NAME)"
	BASE_ADDRESS = 0x2FF80000
endif


all-check: all check

check:
	${TESTROOT}/bin/exit-zero-pass.pl "shared-region-overlap" "shared-region-overlap" ./main

all:
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c -Wl,-image_base,${BASE_ADDRESS}

clean:
	${RM} ${RMFLAGS} main
