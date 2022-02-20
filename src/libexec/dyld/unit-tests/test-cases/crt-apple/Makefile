##
# Copyright (c) 2007-2010 Apple Inc. All rights reserved.
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


#
# verify that apple[0] parameter is correct by comparing to argv[1]
#

all-check: all check

check:
	./main ./main
	./main.stripped ./main.stripped
	`pwd`/main `pwd`/main
	`pwd`/main.stripped `pwd`/main.stripped
	export DYLD_LIBRARY_PATH=. && export DYLD_FRAMEWORK_PATH=. && ./main-setuid ./main-setuid 2>/dev/null

all: main  main.stripped main-setuid

main: main.c
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c -w

main.stripped: main
	${STRIP} main -o main.stripped

main-setuid: main
	cp main main-setuid
	sudo chown root main-setuid
	sudo chmod 4755 main-setuid


clean:
	${RM} ${RMFLAGS} *~ main main.stripped main-setuid
 

