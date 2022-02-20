##
# Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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
# 1) a main executable built with -rpath run with DYLD_FALLBACK_LIBRARY_PATH and linked rpath wins
# 2) a main executable built without -rpath run with DYLD_FALLBACK_LIBRARY_PATH and dylib found
#


all-check: all check

check:
	export DYLD_FALLBACK_LIBRARY_PATH=`pwd`/bad && ./main
	export DYLD_FALLBACK_LIBRARY_PATH=`pwd`/good && ./main2

all: main main2 bad/libfoo.dylib

	
good/libfoo.dylib : foo.c
	mkdir -p good
	${CC} foo.c -dynamiclib -o good/libfoo.dylib -install_name @rpath/libfoo.dylib
	
bad/libfoo.dylib : foo.c
	mkdir -p bad
	${CC} foo.c -DBAD -dynamiclib -o bad/libfoo.dylib -install_name @rpath/libfoo.dylib

main : main.c good/libfoo.dylib 
	${CC} -I${TESTROOT}/include main.c -o main good/libfoo.dylib -Wl,-rpath -Wl,@loader_path/good

main2 : main.c good/libfoo.dylib 
	${CC} -I${TESTROOT}/include main.c -o main2 good/libfoo.dylib 

clean:
	${RM} ${RMFLAGS} *~  main main2 good bad
