##
# Copyright (c) 2007-2009 Apple Inc. All rights reserved.
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
PWD = $(shell pwd)
TESTROOT = $(PWD)/../..
include ${TESTROOT}/include/common.makefile

ifeq "$(OS_NAME)" "iPhoneOS"
	RUN_AS_USER = login -f -l mobile
else
	RUN_AS_USER = 
endif

#
# a setuid main executable linked with -rpath links against a dylib 
# that uses rpath to find another dylib.  It is an error if
# LC_RPATH uses @loader_path or a relative path, but ok if it is an absolute path
#

all-check: all check

check:
	./main || echo "FAIL rpath-indirect-suid absolute path"
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "rpath-indirect-suid @loader_path path" "rpath-indirect-suid @loader_path path" $(PWD)/main_bad1
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "rpath-indirect-suid relative path" "rpath-indirect-suid relative path" $(PWD)/main_bad2
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "rpath-indirect-suid @rpath spoof" "rpath-indirect-suid @rpath spoof" $(PWD)/main_bad3

all: main main_bad1 main_bad2 main_bad3

hide/hole/libbar.dylib : bar.c
	mkdir -p hide/hole
	${CC} bar.c -dynamiclib -o hide/hole/libbar.dylib -install_name @rpath/libbar.dylib

libfoo.dylib : foo.c hide/hole/libbar.dylib
	${CC} foo.c -dynamiclib -o "${PWD}/libfoo.dylib" hide/hole/libbar.dylib 

main : main.c libfoo.dylib
	${CC} -I${TESTROOT}/include main.c -o main libfoo.dylib -Wl,-rpath -Wl,${PWD}/hide/hole
	sudo chown root main
	sudo chmod 4755 main

main_bad1 : main.c libfoo.dylib
	${CC} -I${TESTROOT}/include main.c -DDEFAULT_FAIL  -o main_bad1 libfoo.dylib -Wl,-rpath -Wl,@loader_path/hide/hole
	sudo chown root main_bad1
	sudo chmod 4755 main_bad1

main_bad2 : main.c libfoo.dylib
	${CC} -I${TESTROOT}/include main.c -DDEFAULT_FAIL -o main_bad2 libfoo.dylib -Wl,-rpath -Wl,hide/hole
	sudo chown root main_bad2
	sudo chmod 4755 main_bad2

main_bad3 : main.c libfoo.dylib
	${CC} -I${TESTROOT}/include main.c -DDEFAULT_FAIL -o main_bad3 libfoo.dylib 
	ln -s hide/hole @rpath
	sudo chown root main_bad3
	sudo chmod 4755 main_bad3

clean:
	${RM} ${RMFLAGS} *~  main main_bad1 main_bad2 main_bad3 hide libfoo.dylib @rpath
