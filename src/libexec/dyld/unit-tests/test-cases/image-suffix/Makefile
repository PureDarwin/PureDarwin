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


all-check: all check

check:  check1 check2 check3 check4 check5 check6 check7 check8  

all:  main1 main2 main3 main4 main5 main6 main7 main8  

main : main.c libfoo.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include main.c -o main libfoo.dylib

libfoo.dylib : foo.c
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib -o $(PWD)/libfoo.dylib

libfoo_debug.dylib : foo.c
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -DDEBUG -o libfoo_debug.dylib -install_name $(PWD)/libfoo.dylib

hide/libfoo.dylib : foo.c
	mkdir -p $(PWD)/hide
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -o hide/libfoo.dylib -install_name $(PWD)/libfoo.dylib

hide/libfoo_debug.dylib : foo.c
	mkdir -p $(PWD)/hide
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -o hide/libfoo_debug.dylib -install_name $(PWD)/libfoo.dylib

# bar_debug has bar_debug as install name
libbar.dylib : foo.c
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib -o libbar.dylib

libbar_debug.dylib : foo.c
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -DDEBUG -o libbar_debug.dylib 

hide/libbar.dylib : foo.c
	mkdir -p $(PWD)/hide
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -o hide/libbar.dylib -install_name $(PWD)/libbar.dylib

hide/libbar_debug.dylib : foo.c
	mkdir -p $(PWD)/hide
	${CC} -I${TESTROOT}/include  foo.c -dynamiclib  -o hide/libbar_debug.dylib -install_name $(PWD)/libbar_debug.dylib

clean:
	rm -rf libfoo.dylib libfoo_debug.dylib hide libbar.dylib libbar_debug.dylib main1 main2 main3 main4 main5 main6 main7 main8


#
# check1: main links with libfoo.dylib sets DYLD_IMAGE_SUFFIX=_debug and dynamically loads libfoo.dylib
#   (fails on 10.3)
#
main1 : main.c libfoo.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main1 libfoo.dylib

check1:	
	DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main1 libfoo.dylib
	DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main1 $(PWD)/libfoo.dylib


#
# check2: main links with libfoo_debug.dylib and dynamically loads libfoo.dylib
#
main2 : main.c libfoo_debug.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main2 libfoo_debug.dylib

check2:	
	echo "pwd-1 is ${PWD}"
	echo "pwd-2 is $(PWD)"
	pwd
	./main2 $(PWD)/libfoo.dylib


#
# check3: main links with libfoo.dylib sets DYLD_LIBRARY_PATH=hide and dynamically loads libfoo.dylib
#
main3 : main.c libfoo.dylib
	${CC} -Wno-deprecated-declarations -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main3 libfoo.dylib

check3:	
	DYLD_LIBRARY_PATH=$(PWD)/hide && export DYLD_LIBRARY_PATH && ./main3 $(PWD)/libfoo.dylib


#
# check4: main links with libfoo.dylib sets DYLD_LIBRARY_PATH=hide, DYLD_IMAGE_SUFFIX=_debug and dynamically loads libfoo.dylib
#   (fails on 10.3)
#
main4 : main.c libfoo.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main4 libfoo.dylib

check4:	
	DYLD_LIBRARY_PATH=$(PWD)/hide && export DYLD_LIBRARY_PATH && DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main4 $(PWD)/libfoo.dylib


#
# check5: main links with libbar.dylib sets DYLD_IMAGE_SUFFIX=_debug and dynamically loads libbar.dylib
#   (fails on 10.3)
#
main5 : main.c libbar.dylib libbar_debug.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main5 libbar.dylib

check5:	
	DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main5 $(PWD)/libbar.dylib


#
# check6: main links with libbar_debug.dylib and dynamically loads libbar.dylib
#   (fails on 10.3)
#
main6 : main.c libbar_debug.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main6 libbar_debug.dylib

check6:	
	 DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main6 $(PWD)/libbar.dylib


#
# check7: main links with libbar.dylib sets DYLD_LIBRARY_PATH=hide and dynamically loads libbar.dylib
#
main7 : main.c libbar.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main7 libbar.dylib

check7:	
	DYLD_LIBRARY_PATH=$(PWD)/hide && export DYLD_LIBRARY_PATH && ./main7 $(PWD)/libbar.dylib


#
# check8: main links with libbar.dylib sets DYLD_LIBRARY_PATH=hide, DYLD_IMAGE_SUFFIX=_debug and dynamically loads libbar.dylib
#   (fails on 10.3)
#
main8 : main.c libbar.dylib
	${CC} -Wno-deprecated-declarations -I${TESTROOT}/include  main.c -o main8 libbar.dylib

check8:	
	DYLD_LIBRARY_PATH=$(PWD)/hide && export DYLD_LIBRARY_PATH && DYLD_IMAGE_SUFFIX=_debug && export DYLD_IMAGE_SUFFIX && ./main8 $(PWD)/libbar.dylib
