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
PWD = $(shell pwd)
TESTROOT = $(PWD)/../..
include ${TESTROOT}/include/common.makefile

SHELL = bash # use bash shell so we can redirect just stderr

ifeq "$(OS_NAME)" "iPhoneOS"
	RUN_AS_USER = login -f -l mobile
else
	RUN_AS_USER = 
endif

#
# Use of @exectuable_path in setuid binaries is not allowed
# Use of @loader_path in setuid binaries is not allowed
# Use of relative paths in setuid binaries is not allowed
#

all-check: all check

check:
	./main_exe "setuid-executable_path" || echo "FAIL setuid-executable_path @executable_path not setuid"
	./main_loader "setuid-executable_path" || echo "FAIL setuid-executable_path @loader_path not setuid" 
	./main_rel "setuid-executable_path" || echo "FAIL setuid-executable_path relative path not setuid" 
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "setuid-executable_path @executable_path" "setuid-executable_path @executable_path" $(PWD)/main_exe-suid
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "setuid-executable_path @loader_path" "setuid-executable_path @loader_path" $(PWD)/main_loader-suid
	${RUN_AS_USER} ${PASS_IFF_FAILURE} "setuid-executable_path relative path" "setuid-executable_path relative path" $(PWD)/main_rel-suid



all: main_exe main_exe-suid    main_loader main_loader-suid    main_rel main_rel-suid

dir1/libfoo.dylib : foo.c
	mkdir -p dir1
	${CC} ${CCFLAGS} foo.c -dynamiclib -o dir1/libfoo.dylib -install_name @executable_path/dir1/libfoo.dylib

dir2/libbar.dylib : foo.c
	mkdir -p dir2
	${CC} ${CCFLAGS} foo.c -dynamiclib -o dir2/libbar.dylib -install_name @loader_path/dir2/libbar.dylib

dir3/libbaz.dylib : foo.c
	mkdir -p dir3
	${CC} ${CCFLAGS} foo.c -dynamiclib -o ./dir3/libbaz.dylib

main_exe: main.c dir1/libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main_exe main.c dir1/libfoo.dylib
	cp main_exe main_exe-suid
	sudo chown root main_exe-suid
	sudo chmod 4755 main_exe-suid
	
main_loader: main.c dir2/libbar.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main_loader main.c dir2/libbar.dylib
	cp main_loader main_loader-suid
	sudo chown root main_loader-suid
	sudo chmod 4755 main_loader-suid

main_rel: main.c dir3/libbaz.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main_rel main.c dir3/libbaz.dylib
	cp main_rel main_rel-suid
	sudo chown root main_rel-suid
	sudo chmod 4755 main_rel-suid




clean:
	${RM} ${RMFLAGS} *~ main_exe main_exe-suid main_loader main_loader-suid main_rel main_rel-suid dir1 dir2 dir3

