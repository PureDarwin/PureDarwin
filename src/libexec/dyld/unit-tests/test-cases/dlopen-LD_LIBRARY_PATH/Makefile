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

PWD = $(shell pwd)

#
# tests combinations of dlopen() and LD_LIBRARY_PATH
#
# if dlopen() path contains a slash, LD_LIBRARY_PATH is ignored
#
# 1) leaf name found in current directory  (On linux this fails because . is usually not in default LD_LIBRARY_PATH path)
# 2) cwd relative path 
# 3) full path
# 4) leaf name and LD_LIBRARY_PATH overrides cwd (On 10.3, this fails because cwd was always searched before LD_LIBRARY_PATH??)
# 5) leaf name and LD_LIBRARY_PATH set to alternate directory
# 6) fullpath and LD_LIBRARY_PATH set to alt
#

all-check: all check

check:
	cd alt1 && ../main "libfoo.dylib" 1 "leafname found in cwd"
	./main "./alt1/libfoo.dylib" 1 "relative path"
	./main "${PWD}/alt2/libfoo.dylib" 2 "fullpath"
	export LD_LIBRARY_PATH="${PWD}/alt1" && ./main "libfoo.dylib" 1 "leafname and LD_LIBRARY_PATH overrides cwd"
	export LD_LIBRARY_PATH="${PWD}/alt1" && cd alt3 && ../main "libfoo.dylib" 1 "leafname and alt LD_LIBRARY_PATH"
	export LD_LIBRARY_PATH="${PWD}/alt1" && ./main "${PWD}/alt2/libfoo.dylib" 2 "fullpath and LD_LIBRARY_PATH"

all: main alt1/libfoo.dylib alt2/libfoo.dylib alt3

main : main.c libfoo.dylib 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c libfoo.dylib 


alt1/libfoo.dylib : foo.c
	mkdir -p alt1
	${CC} ${CCFLAGS} -dynamiclib foo.c -o "${PWD}/alt1/libfoo.dylib" -DVALUE=1

alt2/libfoo.dylib : foo.c
	mkdir -p alt2
	${CC} ${CCFLAGS} -dynamiclib foo.c -o "${PWD}/alt2/libfoo.dylib" -DVALUE=2

libfoo.dylib : foo.c
	${CC} ${CCFLAGS} -dynamiclib foo.c -o "${PWD}/libfoo.dylib" -DVALUE=0

alt3 :
	mkdir -p alt3

clean:
	${RM} ${RMFLAGS} *~ main libfoo.dylib alt1 alt2 alt3

