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

PWD = $(shell pwd)

all-check: all check

check:
	./main10 10
	./main11 11
	./main911 9
	./main911b 9
	./main1112 11
	./main1112b 11
	./main1211 12
	./main1211b 12
	export DYLD_LIBRARY_PATH=${PWD}/alt11 && ./main10 11
	export DYLD_LIBRARY_PATH=${PWD}/alt9 && ./main11 9
	export DYLD_LIBRARY_PATH=${PWD}/alt20 && ./main11 11

all: 
	mkdir -p alt11 alt9 alt12
	${CC} ${CCFLAGS} -dynamiclib foo.c -DRESULT=10 -current_version 10 -o "${PWD}/libfoo.dylib"
	${CC} ${CCFLAGS} -dynamiclib foo.c -DRESULT=11 -current_version 11 -install_name "${PWD}/libfoo.dylib" -o alt11/libfoo.dylib  
	${CC} ${CCFLAGS} -dynamiclib foo.c -DRESULT=9  -current_version 9  -install_name "${PWD}/libfoo.dylib" -o alt9/libfoo.dylib  
	${CC} ${CCFLAGS} -dynamiclib foo.c -DRESULT=12 -current_version 12 -install_name "${PWD}/libfoo.dylib" -o alt12/libfoo.dylib  
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main10 main.c libfoo.dylib 
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main11 main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt11
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main911 main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt9 -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt11
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main911b main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt9:@loader_path/alt11
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main1112 main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt11 -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt12
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main1112b main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt11:@loader_path/alt12
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main1211 main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt12 -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt11
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main1211b main.c libfoo.dylib -Wl,-dyld_env,DYLD_LIBRARY_PATH=@loader_path/alt12:@loader_path/alt11


clean:
	${RM} -rf libfoo.dylib alt9 alt11 alt12 main10 main11 main9 main911 main911b main1112 main1112b main1211 main1211b

