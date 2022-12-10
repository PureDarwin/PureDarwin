##
# Copyright (c) 2009 Apple Inc. All rights reserved.
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
# check that a loaded dylib with an @rpath install name will 
# be found by a client.
#

all-check: all check

check:
	./main

all: main

libstuff.dylib : stuff.c
	${CC} stuff.c -I${TESTROOT}/include -dynamiclib -o libstuff.dylib -install_name @rpath/libstuff.dylib
	
libstuff_better.dylib : stuff.c
	${CC} stuff.c -DBETTER=1 -I${TESTROOT}/include -dynamiclib -o libstuff_better.dylib -install_name @rpath/libstuff.dylib

libbar.dylib : bar.c libstuff.dylib 
	${CC} bar.c -dynamiclib libstuff.dylib  -o libbar.dylib 


main : main.c libbar.dylib libstuff_better.dylib
	${CC} -I${TESTROOT}/include main.c -o main -Wl,-rpath,${PWD}


clean:
	${RM} ${RMFLAGS} *~  main libbar.dylib libstuff.dylib libstuff_better.dylib
