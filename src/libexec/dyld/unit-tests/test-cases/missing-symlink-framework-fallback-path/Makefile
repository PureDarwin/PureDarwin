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

all-check: all check

check:
	export DYLD_FALLBACK_FRAMEWORK_PATH="dir" && ${TESTROOT}/bin/exit-zero-pass.pl "pass message" "fail message" ./main

all: main dir/Foo.framework/Versions/Current/Foo

Foo.framework/Versions/Current/Foo:
	mkdir -p Foo.framework/Versions/A
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o Foo.framework/Versions/A/Foo Foo.c
	cd Foo.framework/Versions/ && ln -s A Current
	cd Foo.framework/ && ln -s Versions/Current/Foo Foo

dir/Foo.framework/Versions/Current/Foo:
	$(MAKE) Foo.framework/Versions/Current/Foo
	rm -f Foo.framework/Foo
	mkdir dir
	mv Foo.framework dir

main:
	$(MAKE) Foo.framework/Versions/Current/Foo
	${CC} ${CCFLAGS} -I${TESTROOT}/include -F. -framework Foo -o main main.c

clean:
	${RM} ${RMFLAGS} *~ main dir/ Foo.framework/
