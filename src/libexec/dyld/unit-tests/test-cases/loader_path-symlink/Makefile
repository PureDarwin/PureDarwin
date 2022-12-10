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
# <rdar://problem/10879179> a dylib is found via symlink and must find another dylib relative to the symlink
#

all-check: all check

check:
	./bin/main

all: 
	mkdir -p Frameworks/Foo.framework/Versions/A
	mkdir -p Frameworks/Bar.framework/Versions/A
	${CC} bar.c -dynamiclib -o Frameworks/Bar.framework/Versions/A/Bar -install_name @loader_path/../Bar.framework/Bar
	(cd Frameworks/Foo.framework; ln -fs Versions/A/Foo)
	(cd Frameworks/Bar.framework; ln -fs Versions/A/Bar)
	${CC} foo.c -dynamiclib -o Frameworks/Foo.framework/Versions/A/Foo -install_name @loader_path/../Frameworks/Foo.framework/Foo Frameworks/Bar.framework/Versions/A/Bar
	mkdir -p bin
	${CC} -I${TESTROOT}/include main.c -o bin/main Frameworks/Foo.framework/Versions/A/Foo
	

clean:
	${RM} -rf *~  Frameworks bin
