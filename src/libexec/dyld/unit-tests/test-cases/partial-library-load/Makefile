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

###
### rdar://problem/3884004 Libraries can be half-baked if an error occurs during their first use
###
### We indirectly load libfoo which depends on libbar.  The libbar that is loaded does not contain
### what libfoo needs, so libfoo fails to bind.  The bug was that libfoo was left marked as bound,
### so the next use of libfoo seemed to succeed, when it should have failed.
###


all-check: all check

check:
	export DYLD_IMAGE_SUFFIX="_missing" && ${TESTROOT}/bin/exit-zero-pass.pl "partial-library-load" "partial-library-load" ./main

all: main test.bundle

main : main.c
	${CC} ${CCFLAGS} -Wno-deprecated-declarations -I${TESTROOT}/include -o main main.c

test.bundle : bundle.c libfoo.dylib
	${CC} ${CCFLAGS} -bundle -o test.bundle bundle.c libfoo.dylib

libfoo.dylib : foo.c libbar_missing.dylib libbar.dylib
	${CC} ${CCFLAGS} foo.c -dynamiclib -o libfoo.dylib libbar.dylib

libbar_missing.dylib : bar.c
	${CC} ${CCFLAGS} bar.c -dynamiclib -o libbar_missing.dylib -install_name libbar.dylib

libbar.dylib : bar.c
	${CC} ${CCFLAGS} bar.c -dynamiclib -o libbar.dylib -DHAS_BAR2=1 

clean:
	${RM} ${RMFLAGS} *~ main test.bundle libfoo.dylib libbar.dylib libbar_missing.dylib

