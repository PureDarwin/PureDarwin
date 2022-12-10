##
# Copyright (c) 2007 Apple Inc. All rights reserved.
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
# verify there are no leaks with dlopen/close on success and failure
#

# leaks does not work on rosetta processes
CHECK = check-real
ifeq "ppc" "$(ARCH)"
	MACHINE =  $(shell arch)
	ifeq "i386" "$(MACHINE)"
		CHECK = check-xfail
	endif
endif

all-check: all check

check: ${CHECK}

check-real:
	DYLD_LIBRARY_PATH=hide && ./main 
	./main 

check-xfail:
	echo "XFAIL dlopen-leak"; 

all: main

hide/libbar.dylib : bar.c
	mkdir -p hide
	${CC} bar.c -dynamiclib -o hide/libbar.dylib -install_name libbar.dylib 

libfoo.dylib : foo.c hide/libbar.dylib
	${CC} foo.c hide/libbar.dylib -dynamiclib -o libfoo.dylib 

main : main.c libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c -o main 


clean:
	${RM} ${RMFLAGS} *~  main libfoo.dylib hide
