##
# Copyright (c) 2005-2008 Apple Inc. All rights reserved.
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
	cd absent/  && ../${TESTROOT}/bin/exit-non-zero-pass.pl "non-weak-library" "non-weak-library" ./main
	cd present/ && ./main

all: foo.c main.c
	${CC} -dynamiclib -prebind -seg1addr 400000 -o libfoo.dylib foo.c
	mkdir -p absent/
	${CC} -prebind -I${TESTROOT}/include -L. -lfoo -DLIB_ABSENT  -o absent/main  main.c
	mkdir -p present/
	${CC} -prebind -I${TESTROOT}/include -L. -lfoo -DLIB_PRESENT -o present/main main.c
	mv libfoo.dylib present/

clean:
	${RM} ${RMFLAGS} *~ libfoo.dylib absent present
