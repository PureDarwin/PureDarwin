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

all-check: all_$(OS_LION_FEATURES) check_$(OS_LION_FEATURES)

all: all_$(OS_LION_FEATURES) 

check: check_$(OS_LION_FEATURES)

check_:
	${PASS_IFF} true

all_:


check_1:
	./main

all_1: 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib common.c -o libcommon.dylib  
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib u2.c libcommon.dylib -o libu2.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib u.c libcommon.dylib -o libu.dylib -Wl,-upward_library,libu2.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib c.c libcommon.dylib -o libc.dylib -Wl,-upward_library,libu.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib b.c libcommon.dylib -o libb.dylib libc.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c  libb.dylib libcommon.dylib -o main


clean:
	${RM} ${RMFLAGS} *~ main libu.dylib libu2.dylib libb.dylib libc.dylib libcommon.dylib


	
