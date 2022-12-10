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

PWD = `pwd`


#
# Test re-exported frameworks work
#

all-check: all check

check:
	./main

all:  main



Bar.framework/Bar : bar.c
	mkdir -p Bar.framework
	${CC} bar.c -dynamiclib -install_name $(PWD)/Bar.framework/Bar -o Bar.framework/Bar 

Foo.framework/Foo : foo.c Bar.framework/Bar
	mkdir -p Foo.framework
	${CC} foo.c -dynamiclib Bar.framework/Bar -sub_umbrella Bar -install_name $(PWD)/Foo.framework/Foo -o Foo.framework/Foo 

main : main.c Foo.framework/Foo
	${CC} main.c -I${TESTROOT}/include -o main -framework Foo -F.



	
clean:
	rm -rf  main Foo.framework Bar.framework 
	