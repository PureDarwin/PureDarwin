##
# Copyright (c) 2007-2009 Apple Inc. All rights reserved.
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
# verifies that the return value from main() makes it to shell
# for both crt1.0 and crt1.10.5.o
#

all-check: all check

check:
	${TESTROOT}/bin/exit-zero-pass.pl "crt-result good" "crt-result good"    ./good
	${TESTROOT}/bin/exit-non-zero-pass.pl "crt-result bad" "crt-result bad"  ./bad


all: good bad 


good: good.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o good good.c 

bad: bad.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o bad bad.c

clean:
	${RM} ${RMFLAGS} *~  good  bad 
 

