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
# verify there are no leaks with using @loader_path based rpaths
#

# leaks does not work on rosetta processes
EMULATED = 0
ifeq "ppc" "$(ARCH)"
	MACHINE =  $(shell arch)
	ifeq "i386" "$(MACHINE)"
		EMULATED = 1
	endif
endif

all-check: all check

check:
	if [ ${EMULATED} == 0 ]; \
	then \
		./main ; \
		./main.bad ; \
	else \
		echo "XFAIL rpath-dlopen-leak"; \
	fi;
	
all: main main.bad

hide/hole/libfoo.dylib : foo.c
	mkdir -p hide/hole
	${CC} foo.c -dynamiclib -o hide/hole/libfoo.dylib -install_name @rpath/libfoo.dylib
	

main : main.c hide/hole/libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c -o main -Wl,-rpath -Wl,@loader_path/hide/hole

main.bad : main.c hide/hole/libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include main.c -o main.bad -Wl,-rpath -Wl,@loader_path/bad

clean:
	${RM} ${RMFLAGS} *~  main main.bad hide/hole/libfoo.dylib hide
