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
# Verify that a lazy loaded dylib has initializers run in correct order
#

all-check: all check

check:
	./main
	./main2

all:
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib -install_name libfoo.dylib
	${CC} ${CCFLAGS} foo.c -DBAR=1 -dynamiclib -o libfoo.dylib.full -install_name libfoo.dylib
	${CC} ${CCFLAGS} main.c -I${TESTROOT}/include  -Wl,-lazy_library,libfoo.dylib.full -o main
	${CC} ${CCFLAGS} main.c -I${TESTROOT}/include -DLAZY_HANDLER=1 -Wl,-lazy_library,libfoo.dylib.full -o main2


clean:
	rm -f libfoo.dylib libfoo.dylib.full main main2
