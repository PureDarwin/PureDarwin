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

#
#  This test the ability of dyld to continue searching if a file was found but not usable.
#  Uses DYLD_LIBRARY_PATH to search through many bogus files 
#
#


all-check: all check

check:
	export DYLD_LIBRARY_PATH=locations/datafile:locations/exec:locations/dir && ${TESTROOT}/bin/exit-non-zero-pass.pl "ignore-bad-files intended failure" "ignore-bad-files intended failure" ./main
	export DYLD_LIBRARY_PATH=locations/datafile:locations/exec:locations/dir:locations/real && ${TESTROOT}/bin/exit-zero-pass.pl "ignore-bad-files intended success" "ignore-bad-files intended success" ./main

all: main locations/real/libfoo.dylib locations/exec/libfoo.dylib locations/datafile/libfoo.dylib locations/dir/libfoo.dylib 

main: main.c locations/real/libfoo.dylib
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o main main.c locations/real/libfoo.dylib


# real dylib to use
locations/real/libfoo.dylib : foo.c
	mkdir -p locations/real
	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o locations/real/libfoo.dylib foo.c -install_name libfoo.dylib

# not a dylib - but a mach-o main executable
locations/exec/libfoo.dylib : main.c
	mkdir -p locations/exec
	${CC} ${CCFLAGS} -I${TESTROOT}/include -o locations/exec/libfoo.dylib main.c foo.c

# some random data file
locations/datafile/libfoo.dylib : main.c
	mkdir -p locations/datafile
	cat main.c > locations/datafile/libfoo.dylib

# not a file - but a directory
locations/dir/libfoo.dylib : 
	mkdir -p locations/dir/libfoo.dylib

# file with wrong architecture
#locations/wrongarch/libfoo.dylib : 
#	mkdir -p locations/wrongarch
#	${CC} ${CCFLAGS} -I${TESTROOT}/include -dynamiclib -o locations/wrongarch/libfoo.dylib foo.c -install_name libfoo.dylib -arch ppc64



clean:
	${RM} ${RMFLAGS} *~ main locations

