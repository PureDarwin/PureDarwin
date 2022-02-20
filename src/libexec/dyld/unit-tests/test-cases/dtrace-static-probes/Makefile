##
# Copyright (c) 2008 Apple Inc. All rights reserved.
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
# Test that dtrace probes fire 
#

ifeq "$(OS_NAME)" "iPhoneOS"
	SUDO = 
else
	SUDO = sudo
endif

all-check: all check

check:
	if [ `${SUDO} /usr/sbin/dtrace -q -n 'Foo$$target:::count { printf("%u\n", arg0); } ' -c ./main | wc -w` == 5 ]; \
	then \
		echo "PASS dtrace-static-probes"; \
	else \
		echo "FAIL dtrace-static-probes"; \
	fi; \

	

all: main 


main: main.c 
	dtrace -h -s foo.d
	${CC} ${CCFLAGS} main.c -o main -dead_strip


clean:
	rm -rf main foo.h
	
