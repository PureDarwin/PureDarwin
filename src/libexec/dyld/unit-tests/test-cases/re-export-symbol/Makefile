##
# Copyright (c) 2010 Apple Inc. All rights reserved.
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
# Test that fine grain re-exports works
#

all-check: all_$(OS_LION_FEATURES) check_$(OS_LION_FEATURES)

all: all_$(OS_LION_FEATURES)

check: check_$(OS_LION_FEATURES)

check_1:
	./main1
	./main2

all_1:
	# build base library
	${CC} ${CCFLAGS} -dynamiclib bar.c -o `pwd`/libbar.dylib

	# build library the re-exports _bar from base library
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo.dylib libbar.dylib -exported_symbols_list foo.exp
	# link against dylib and verify _bar is marked as coming from libfoo
	${CC} ${CCFLAGS} main1.c -I${TESTROOT}/include libfoo.dylib -o main1

	# build library the re-exports _bar from base library as _mybar
	${CC} ${CCFLAGS} -dynamiclib foo.c -o libfoo2.dylib libbar.dylib -Wl,-alias,_bar,_mybar -exported_symbols_list foo2.exp
	# link against dylib and verify _mybar is marked as coming from libfoo
	${CC} ${CCFLAGS} main2.c -I${TESTROOT}/include libfoo2.dylib -o main2

check_:
	${PASS_IFF} true

all_:
	
clean:
	rm -rf libbar.dylib libfoo.dylib libfoo2.dylib  main1 main2
