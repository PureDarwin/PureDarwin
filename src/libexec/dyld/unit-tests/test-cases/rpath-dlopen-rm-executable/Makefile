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

SHELL = bash # use bash shell so we can redirect just stderr


PWD = $(shell pwd)

#
# a main executable linked with -rpath.  At runtime the exectuable
# deletes itself than calls dlopen().  Test that @executable_path
# does not cause malloc to abort.
#

all-check: all check

check:
	cp main main.rm
	${TESTROOT}/bin/exit-zero-pass.pl "rpath-dlopen-rm-executable" "rpath-dlopen-rm-executable" ./main.rm 2> /dev/null

all: main


main : main.c 
	${CC} -I${TESTROOT}/include main.c -o main -Wl,-rpath -Wl,@executable_path/hide/hole

clean:
	${RM} ${RMFLAGS} *~  main main.rm
