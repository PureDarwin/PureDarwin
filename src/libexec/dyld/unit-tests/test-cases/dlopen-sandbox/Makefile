##
# Copyright (c) 2015 Apple Computer, Inc. All rights reserved.
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

all-check: all check

check:
	sandbox-exec -f sbs/deny-open.sb ./main "file system sandbox blocked open"
	sandbox-exec -f sbs/deny-stat.sb ./main "file system sandbox blocked stat"
	sandbox-exec -f sbs/deny-mmap.sb ./main "file system sandbox blocked mmap"

all:
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c
	mkdir -p sbs
	sed -e 's/FILTER/file-read-data/'      -e "s|DIR|$$PWD|" < base.sb > sbs/deny-open.sb
	sed -e 's/FILTER/file-read-metadata/'  -e "s|DIR|$$PWD|" < base.sb > sbs/deny-stat.sb
	sed -e 's/FILTER/file-map-executable/' -e "s|DIR|$$PWD|" < base.sb > sbs/deny-mmap.sb

clean:
	${RM} -rf *~ main sbs libfoo.dylib


