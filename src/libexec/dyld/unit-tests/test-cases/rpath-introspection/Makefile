##
# Copyright (c) 2012 Apple Inc. All rights reserved.
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

#
# Checks that @rpath that expands to have ../ in it will get realpathed and 
# .. will not be seen when introspecting dylibs
#

all-check: all check

check:
	./main

all:
	mkdir -p hide/hole
	${CC} foo.c -dynamiclib -o hide/hole/libfoo.dylib -install_name @rpath/libfoo.dylib 
	${CC} -I${TESTROOT}/include main.c -o main hide/hole/libfoo.dylib -Wl,-rpath -Wl,${PWD}/hide/../hide/hole

clean:
	${RM} -rf *~  main hide
